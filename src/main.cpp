#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <boost/algorithm/string.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <regex>
#include <set>
#include <thread>
#include "common/concurrent_queue.hpp"
#include "common/messages.hpp"
#include "common/python.hpp"
#include "common/system.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "env.hpp"
#include "judge/choice.hpp"
#include "judge/program_output.hpp"
#include "judge/programming.hpp"
#include "monitor/elastic_apm.hpp"
#include "server/forth/forth.hpp"
#include "server/mcourse/mcourse.hpp"
#include "server/moj/moj.hpp"
#include "server/sicily/sicily.hpp"
#include "worker.hpp"
using namespace std;

judge::concurrent_queue<judge::message::client_task> testcase_queue;
judge::concurrent_queue<judge::message::core_request> core_acq_queue;

struct cpuset {
    string literal;
    set<unsigned> ids;
    cpu_set_t cpuset;
};

void validate(boost::any& v, const vector<string>& values, cpuset*, int) {
    using namespace boost::program_options;
    static regex matcher("^([0-9]+)(-([0-9]+))?$");
    validators::check_first_occurrence(v);

    cpuset result;
    CPU_ZERO(&result.cpuset);

    string const& s = validators::get_single_string(values);
    result.literal = s;
    vector<string> splitted;
    boost::split(splitted, s, boost::is_any_of(","));
    for (auto& token : splitted) {
        smatch matches;
        if (regex_search(token, matches, matcher)) {
            if (matches[3].str().empty()) {
                int i = boost::lexical_cast<int>(matches[1].str());
                result.ids.insert(i);
                CPU_SET(i, &result.cpuset);
            } else {
                int begin = boost::lexical_cast<int>(matches[1].str());
                int end = boost::lexical_cast<int>(matches[3].str());
                if (begin > end || begin < 0 || end < 0)
                    throw validation_error(validation_error::invalid_option_value);
                for (unsigned i = (unsigned)begin; i <= (unsigned)end; ++i) {
                    result.ids.insert(i);
                    CPU_SET(i, &result.cpuset);
                }
            }
            v = result;
        } else {
            throw validation_error(validation_error::invalid_option_value);
        }
    }
}

void sigintHandler(int /* signum */) {
    LOG(ERROR) << "Received SIGINT, stopping workers";
    judge::stop_workers();
}

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    wchar_t* progname = Py_DecodeLocale(argv[0], NULL);
    Py_SetProgramName(progname);
    Py_Initialize();
    PyEval_InitThreads();
    vector<wchar_t*> wargv;
    for (int i = 0; i < argc; ++i)
        wargv.push_back(Py_DecodeLocale(argv[i], NULL));
    PySys_SetArgv(argc, wargv.data());
    PyThread_guard guard;

    filesystem::path current(argv[0]);
    filesystem::path repo_dir(filesystem::weakly_canonical(current).parent_path().parent_path());

    signal(SIGINT, sigintHandler);

    // 默认情况下，假设运行环境是拉取代码直接编译的环境，此时我们可以假定 runguard 的运行路径
    if (!getenv("RUNGUARD")) {
        filesystem::path runguard(repo_dir / "runguard" / "bin" / "runguard");
        if (filesystem::exists(runguard)) {
            set_env("RUNGUARD", filesystem::weakly_canonical(runguard).string());
        }
    }

    CHECK(getenv("RUNGUARD"))  // RUNGUARD 环境变量将传给 exec/check/standard/run 评测脚本使用
        << "RUNGUARD environment variable should be specified. This env points out where the runguard executable locates in.";

    judge::put_error_codes();

    namespace po = boost::program_options;
    po::options_description desc("judge-system options");
    po::variables_map vm;

    // clang-format off
    desc.add_options()
        ("enable-sicily", po::value<vector<string>>(), "run Sicily Online Judge submission fetcher, with configuration file path.")
        ("enable", po::value<vector<string>>(), "load fetcher configurations in given directory with extension .json")
        ("enable-4", po::value<vector<string>>(), "run Matrix Judge System 4.0 submission fetcher, with configuration file path.")
        ("enable-3", po::value<vector<string>>(), "run Matrix Judge System 3.0 submission fetcher, with configuration file path.")
        ("enable-2", po::value<vector<string>>(), "run Matrix Judge System 2.0 submission fetcher, with configuration file path.")
        ("cores", po::value<cpuset>()->required(), "set the cores the judge-system can make use of")
        ("exec-dir", po::value<string>(), "set the default predefined executables for falling back. You can either pass it from environ EXECDIR")
        ("script-dir", po::value<string>(), "set the directory with required scripts stored. You can either pass it from environ SCRIPTDIR")
        ("cache-dir", po::value<string>(), "set the directory to store cached test data, compiled spj, random test generator, compiled executables. You can either pass it from environ CACHEDIR")
        ("data-dir", po::value<string>(), "set the directory to store test data to be judged, for ramdisk to speed up IO performance of user program. You can either pass it from environ DATADIR")
        ("run-dir", po::value<string>(), "set the directory to run user programs, store compiled user program. You can either pass it from environ RUNDIR")
        ("chroot-dir", po::value<string>(), "set the chroot directory. You can either pass it from environ CHROOTDIR")
        ("script-mem-limit", po::value<unsigned>(), "set memory limit in KB for random data generator, scripts, default to 262144(256MB). You can either pass it from environ SCRIPTMEMLIMIT")
        ("script-time-limit", po::value<unsigned>(), "set time limit in seconds for random data generator, scripts, default to 10(10 second). You can either pass it from environ SCRIPTTIMELIMIT")
        ("script-file-limit", po::value<unsigned>(), "set file limit in KB for random data generator, scripts, default to 524288(512MB). You can either pass it from environ SCRIPTFILELIMIT")
        ("run-user", po::value<string>(), "set run user. You can either pass it from environ RUNUSER")
        ("run-group", po::value<string>(), "set run group. You can either pass it from environ RUNGROUP")
        ("cache-random-data", po::value<size_t>(), "set the maximum number of cached generated random data, default to 100. You can either pass it from environ CACHERANDOMDATA")
        ("debug", "turn on the debug mode to disable checking whether it is in privileged mode, and not to delete submission directory to check the validity of result files.")
        ("help", "display this help text")
        ("version", "display version of this application");
    // clang-format on

    try {
        po::store(po::command_line_parser(argc, argv)
                      .options(desc)
                      .run(),
                  vm);
        po::notify(vm);
    } catch (po::error& e) {
        cerr << e.what() << endl
             << endl;
        cerr << desc << endl;
        return EXIT_FAILURE;
    }

    if (vm.count("help")) {
        cout << "JudgeSystem: Fetch submissions from server, judge them" << endl
             << "This app requires root privilege" << endl
             << "Required Environment Variables:" << endl
             << "\tRUNGUARD: location of runguard" << endl
             << "Usage: " << argv[0] << " [options]" << endl;
        cout << desc << endl;
        return EXIT_SUCCESS;
    }

    if (vm.count("version")) {
        cout << "judge-system 4.0" << endl;
        return EXIT_SUCCESS;
    }

    if (vm.count("debug")) {
        judge::DEBUG = true;
    } else if (getenv("DEBUG")) {
        judge::DEBUG = true;
    }

    if (getuid() != 0) {
        cerr << "You should run this program in privileged mode" << endl;
        if (!judge::DEBUG) return EXIT_FAILURE;
    }

    if (vm.count("exec-dir")) {
        judge::EXEC_DIR = filesystem::path(vm.at("exec-dir").as<string>());
    } else if (getenv("EXECDIR")) {
        judge::EXEC_DIR = filesystem::path(getenv("EXECDIR"));
    } else {
        filesystem::path execdir(repo_dir / "exec");
        if (filesystem::exists(execdir)) {
            judge::EXEC_DIR = execdir;
        }
    }
    set_env("JUDGE_UTILS", judge::EXEC_DIR / "utils", true);
    CHECK(filesystem::is_directory(judge::EXEC_DIR))
        << "Executables directory " << judge::EXEC_DIR << " does not exist";

    for (auto& p : filesystem::directory_iterator(judge::EXEC_DIR))
        if (filesystem::is_regular_file(p))
            filesystem::permissions(p,
                                    filesystem::perms::group_exec | filesystem::perms::others_exec | filesystem::perms::owner_exec,
                                    filesystem::perm_options::add);

    if (vm.count("script-dir")) {
        judge::SCRIPT_DIR = filesystem::path(vm.at("script-dir").as<string>());
    } else if (getenv("SCRIPTDIR")) {
        judge::SCRIPT_DIR = filesystem::path(getenv("SCRIPTDIR"));
    } else {
        filesystem::path scriptdir(repo_dir / "script");
        if (filesystem::exists(scriptdir)) {
            judge::SCRIPT_DIR = scriptdir;
        }
    }

    if (vm.count("cache-dir")) {
        judge::CACHE_DIR = filesystem::path(vm.at("cache-dir").as<string>());
    } else if (getenv("CACHEDIR")) {
        judge::CACHE_DIR = filesystem::path(getenv("CACHEDIR"));
    }
    CHECK(filesystem::is_directory(judge::CACHE_DIR))
        << "Cache directory " << judge::CACHE_DIR << " does not exist";

    if (vm.count("data-dir")) {
        judge::DATA_DIR = filesystem::path(vm.at("data-dir").as<string>());
        judge::USE_DATA_DIR = true;
        CHECK(filesystem::is_directory(judge::DATA_DIR))
            << "Data directory " << judge::DATA_DIR << " does not exist";
    } else if (getenv("DATADIR")) {
        judge::DATA_DIR = filesystem::path(getenv("DATADIR"));
        judge::USE_DATA_DIR = true;
        CHECK(filesystem::is_directory(judge::DATA_DIR))
            << "Data directory " << judge::DATA_DIR << " does not exist";
    }

    if (vm.count("run-dir")) {
        judge::RUN_DIR = filesystem::path(vm.at("run-dir").as<string>());
    } else if (getenv("RUNDIR")) {
        judge::RUN_DIR = filesystem::path(getenv("RUNDIR"));
    }
    CHECK(filesystem::is_directory(judge::RUN_DIR))
        << "Run directory " << judge::RUN_DIR << " does not exist";

    if (vm.count("chroot-dir")) {
        judge::CHROOT_DIR = filesystem::path(vm.at("chroot-dir").as<string>());
    } else if (getenv("CHROOTDIR")) {
        judge::CHROOT_DIR = filesystem::path(getenv("CHROOTDIR"));
    }
    CHECK(filesystem::is_directory(judge::CHROOT_DIR))
        << "Chroot directory " << judge::CHROOT_DIR << " does not exist";

    if (vm.count("script-mem-limit")) {
        judge::SCRIPT_MEM_LIMIT = vm["script-mem-limit"].as<unsigned>();
    } else if (getenv("SCRIPTMEMLIMIT")) {
        judge::SCRIPT_MEM_LIMIT = boost::lexical_cast<unsigned>("SCRIPTMEMLIMIT");
    }
    set_env("SCRIPTMEMLIMIT", to_string(judge::SCRIPT_MEM_LIMIT), false);

    if (vm.count("script-time-limit")) {
        judge::SCRIPT_TIME_LIMIT = vm["script-time-limit"].as<unsigned>();
    } else if (getenv("SCRIPTTIMELIMIT")) {
        judge::SCRIPT_TIME_LIMIT = boost::lexical_cast<unsigned>("SCRIPTTIMELIMIT");
    }
    set_env("SCRIPTTIMELIMIT", to_string(judge::SCRIPT_TIME_LIMIT), false);

    if (vm.count("script-file-limit")) {
        judge::SCRIPT_FILE_LIMIT = vm["script-file-limit"].as<unsigned>();
    } else if (getenv("SCRIPTFILELIMIT")) {
        judge::SCRIPT_FILE_LIMIT = boost::lexical_cast<unsigned>("SCRIPTFILELIMIT");
    }
    set_env("SCRIPTFILELIMIT", to_string(judge::SCRIPT_FILE_LIMIT), false);

    if (vm.count("run-user")) {
        string runuser = vm["run-user"].as<string>();
        set_env("RUNUSER", runuser);
    }

    if (vm.count("run-group")) {
        string rungroup = vm["run-group"].as<string>();
        set_env("RUNGROUP", rungroup);
    }

    // 让评测系统写入的数据只允许当前用户写入
    umask(0022);

    if (vm.count("cache-random-data")) {
        judge::MAX_RANDOM_DATA_NUM = vm["cache-random-data"].as<size_t>();
    } else if (getenv("CACHERANDOMDATA")) {
        judge::MAX_RANDOM_DATA_NUM = boost::lexical_cast<unsigned>(getenv("CACHERANDOMDATA"));
    }

    if (vm.count("enable-sicily")) {
        auto sicily_servers = vm.at("enable-scicily").as<vector<string>>();
        for (auto& sicily_server : sicily_servers) {
            CHECK(filesystem::is_regular_file(sicily_server))
                << "Configuration file " << sicily_server << " does not exist";

            auto sicily_judger = make_unique<judge::server::sicily::configuration>();
            sicily_judger->init(sicily_server);
            judge::register_judge_server(move(sicily_judger));
        }
    }

    if (vm.count("enable-4")) {
        auto forth_servers = vm.at("enable-4").as<vector<string>>();
        for (auto& forth_server : forth_servers) {
            CHECK(filesystem::is_regular_file(forth_server))
                << "Configuration file " << forth_server << " does not exist";

            auto forth_judger = make_unique<judge::server::forth::configuration>();
            forth_judger->init(forth_server);
            judge::register_judge_server(move(forth_judger));
        }
    }

    if (vm.count("enable-3")) {
        auto third_servers = vm.at("enable-3").as<vector<string>>();
        for (auto& third_server : third_servers) {
            CHECK(filesystem::is_regular_file(third_server))
                << "Configuration file " << third_server << " does not exist";

            auto third_judger = make_unique<judge::server::moj::configuration>();
            third_judger->init(third_server);
            judge::register_judge_server(move(third_judger));
        }
    }

    if (vm.count("enable-2")) {
        auto second_servers = vm.at("enable-2").as<vector<string>>();
        for (auto& second_server : second_servers) {
            CHECK(filesystem::is_regular_file(second_server))
                << "Configuration file " << second_server << " does not exist";

            auto second_judger = make_unique<judge::server::mcourse::configuration>();
            second_judger->init(second_server);
            judge::register_judge_server(move(second_judger));
        }
    }

    if (vm.count("enable")) {
        auto servers = vm.at("enable").as<vector<string>>();
        vector<filesystem::path> config;
        for (auto& server : servers) {
            if (filesystem::is_directory(server)) {
                for (const auto& p : filesystem::directory_iterator(server)) {
                    auto& path = p.path();
                    if (path.extension() == ".json")
                        config.push_back(path);
                }
            } else if (filesystem::is_regular_file(server)) {
                config.emplace_back(server);
            } else {
                LOG(FATAL) << "Configuration file " << server << " does not exist";
            }
        }

        for (const auto& p : config) {
            try {
                nlohmann::json j = nlohmann::json::parse(judge::read_file_content(p));
                string type = j.at("type").get<string>();
                if (type == "mcourse") {
                    auto second_judger = make_unique<judge::server::mcourse::configuration>();
                    second_judger->init(p);
                    judge::register_judge_server(move(second_judger));
                } else if (type == "moj") {
                    auto third_judger = make_unique<judge::server::moj::configuration>();
                    third_judger->init(p);
                    judge::register_judge_server(move(third_judger));
                } else if (type == "sicily") {
                    auto sicily_judger = make_unique<judge::server::sicily::configuration>();
                    sicily_judger->init(p);
                    judge::register_judge_server(move(sicily_judger));
                } else if (type == "forth") {
                    auto forth_judger = make_unique<judge::server::forth::configuration>();
                    forth_judger->init(p);
                    judge::register_judge_server(move(forth_judger));
                } else {
                    LOG(FATAL) << "Unrecognized configuration type " << type << " in file " << p;
                }
            } catch (std::exception& e) {
                LOG(FATAL) << "Configuration file " << p << " is malformed";
            }
        }
    }

    judge::register_judger(make_unique<judge::programming_judger>());
    judge::register_judger(make_unique<judge::choice_judger>());
    judge::register_judger(make_unique<judge::program_output_judger>());

    judge::register_monitor(make_unique<judge::elastic>(judge::SCRIPT_DIR / "elastic"));

    vector<thread> worker_threads;

    // 我们为每个注册的 CPU 核心 都生成一个 worker
    if (vm.count("cores")) {
        cpuset set = vm["cores"].as<cpuset>();
        for (unsigned i : set.ids) {
            worker_threads.push_back(move(judge::start_worker(i, testcase_queue, core_acq_queue)));
        }
    }

    for (auto& th : worker_threads)
        th.join();

    return 0;
}
