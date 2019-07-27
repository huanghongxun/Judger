#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <regex>
#include <set>
#include <thread>
#include "common/concurrent_queue.hpp"
#include "common/messages.hpp"
#include "common/system.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "env.hpp"
#include "judge/choice.hpp"
#include "judge/program_output.hpp"
#include "judge/programming.hpp"
#include "server/forth/forth.hpp"
#include "server/mcourse/mcourse.hpp"
#include "server/moj/moj.hpp"
#include "server/sicily/sicily.hpp"
#include "worker.hpp"
using namespace std;

judge::concurrent_queue<judge::message::client_task> testcase_queue;
set<int> worker_pid;

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

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    // 默认情况下，假设运行环境是拉取代码直接编译的环境，此时我们可以假定 runguard 的运行路径
    if (!getenv("RUNGUARD")) {
        filesystem::path current(argv[0]);
        filesystem::path runguard(filesystem::weakly_canonical(current).parent_path().parent_path() / "runguard" / "bin" / "runguard");
        if (filesystem::exists(runguard)) {
            set_env("RUNGUARD", filesystem::weakly_canonical(runguard).string());
        }
    }

    CHECK(getenv("RUNGUARD"))  // RUNGUARD 环境变量将传给 exec/check/standard/run 评测脚本使用
        << "RUNGUARD environment variable should be specified. This env points out where the runguard executable locates in.";

    judge::put_error_codes();

    const unsigned cpus = std::thread::hardware_concurrency();

    namespace po = boost::program_options;
    po::options_description desc("judge-system options");
    po::variables_map vm;

    // clang-format off
    desc.add_options()
        ("enable-sicily", po::value<vector<string>>(), "run Sicily Online Judge submission fetcher, with configuration file path.")
        ("enable",   po::value<vector<string>>(), "run Matrix Judge System 4.0 submission fetcher, with configuration file path.")
        ("enable-3", po::value<vector<string>>(), "run Matrix Judge System 3.0 submission fetcher, with configuration file path.")
        ("enable-2", po::value<vector<string>>(), "run Matrix Judge System 2.0 submission fetcher, with configuration file path.")
        ("monitor-port", po::value<unsigned>(), "set the port the monitor server listens to, default to 80")
        ("workers", po::value<unsigned>(), "set number of single thread judge workers to be kept")
        ("worker", po::value<vector<cpuset>>(), "run a judge worker which cpuset is given")
        ("auto-workers", "start workers with number of hardware concurrency")
        ("exec-dir", po::value<string>()->required(), "set the default predefined executables for falling back")
        ("cache-dir", po::value<string>()->required(), "set the directory to store cached test data, compiled spj, random test generator, compiled executables")
        ("data-dir", po::value<string>(), "set the directory to store test data to be judged, for ramdisk to speed up IO performance of user program.")
        ("run-dir", po::value<string>()->required(), "set the directory to run user programs, store compiled user program")
        ("log-dir", po::value<string>(), "set the directory to store log files")
        ("chroot-dir", po::value<string>()->required(), "set the chroot directory")
        ("script-mem-limit", po::value<unsigned>(), "set memory limit in KB for random data generator, scripts, default to 262144(256MB).")
        ("script-time-limit", po::value<unsigned>(), "set time limit in seconds for random data generator, scripts, default to 10(10 second).")
        ("script-file-limit", po::value<unsigned>(), "set file limit in KB for random data generator, scripts, default to 524288(512MB).")
        ("run-user", po::value<string>()->required(), "set run user")
        ("run-group", po::value<string>()->required(), "set run group")
        ("cache-random-data", po::value<size_t>(), "set the maximum number of cached generated random data, default to 100.")
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
    }

    if (getuid() != 0) {
        cerr << "You should run this program in privileged mode" << endl;
        if (!judge::DEBUG) return EXIT_FAILURE;
    }

    if (vm.count("exec-dir")) {
        judge::EXEC_DIR = filesystem::path(vm.at("exec-dir").as<string>());
        set_env("JUDGE_UTILS", judge::EXEC_DIR / "utils", true);
        CHECK(filesystem::is_directory(judge::EXEC_DIR))
            << "Executables directory " << judge::EXEC_DIR << " does not exist";

        for (auto& p : filesystem::directory_iterator(judge::EXEC_DIR))
            if (filesystem::is_regular_file(p))
                filesystem::permissions(p,
                                        filesystem::perms::group_exec | filesystem::perms::others_exec | filesystem::perms::owner_exec,
                                        filesystem::perm_options::add);
    }

    if (vm.count("cache-dir")) {
        judge::CACHE_DIR = filesystem::path(vm.at("cache-dir").as<string>());
        CHECK(filesystem::is_directory(judge::CACHE_DIR))
            << "Cache directory " << judge::CACHE_DIR << " does not exist";
    }

    if (vm.count("data-dir")) {
        judge::DATA_DIR = filesystem::path(vm.at("data-dir").as<string>());
        judge::USE_DATA_DIR = true;
        CHECK(filesystem::is_directory(judge::DATA_DIR))
            << "Data directory " << judge::DATA_DIR << " does not exist";
    }

    if (vm.count("run-dir")) {
        judge::RUN_DIR = filesystem::path(vm.at("run-dir").as<string>());
        CHECK(filesystem::is_directory(judge::RUN_DIR))
            << "Run directory " << judge::RUN_DIR << " does not exist";
    }

    if (vm.count("log-dir")) {
        string logdir = vm.at("log-dir").as<string>();
        CHECK(filesystem::is_directory(filesystem::path(logdir)))
            << "Log directory " << logdir << " does not exist";
        set_env("LOGDIR", logdir);
    }
    set_env("LOGDIR", filesystem::weakly_canonical(filesystem::current_path()).string(), false);

    if (vm.count("chroot-dir")) {
        judge::CHROOT_DIR = filesystem::path(vm.at("chroot-dir").as<string>());
        CHECK(filesystem::is_directory(judge::CHROOT_DIR))
            << "Chroot directory " << judge::CHROOT_DIR << " does not exist";
    }

    if (vm.count("script-mem-limit")) {
        judge::SCRIPT_MEM_LIMIT = vm["script-mem-limit"].as<unsigned>();
    }
    set_env("SCRIPTMEMLIMIT", to_string(judge::SCRIPT_MEM_LIMIT), false);

    if (vm.count("script-time-limit")) {
        judge::SCRIPT_TIME_LIMIT = vm["script-time-limit"].as<unsigned>();
    }
    set_env("SCRIPTTIMELIMIT", to_string(judge::SCRIPT_TIME_LIMIT), false);

    if (vm.count("script-file-limit")) {
        judge::SCRIPT_FILE_LIMIT = vm["script-file-limit"].as<unsigned>();
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

    if (vm.count("enable")) {
        auto forth_servers = vm.at("enable").as<vector<string>>();
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

    judge::register_judger(make_unique<judge::programming_judger>());
    judge::register_judger(make_unique<judge::choice_judger>());
    judge::register_judger(make_unique<judge::program_output_judger>());

    set<unsigned> cores;
    vector<thread> worker_threads;
    size_t worker_id = 0;

    if (vm.count("auto-workers")) {
        // 对于自动设置客户端的情况，我们为每个 CPU 都生成一个 worker
        cpu_set_t cpuset;
        for (unsigned i = 0; i < cpus; ++i) {
            CPU_ZERO(&cpuset);
            CPU_SET(i, &cpuset);
            worker_threads.push_back(move(judge::start_worker(worker_id++, cpuset, to_string(i), testcase_queue)));
        }
    } else {
        // 对于手动设置客户端的情况，我们记录可以使用的核心
        for (unsigned i = 0; i < cpus; ++i) {
            cores.insert(i);
        }
    }

    if (vm.count("worker")) {
        auto workers = vm.at("worker").as<vector<cpuset>>();
        for (auto& worker : workers) {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);

            for (unsigned id : worker.ids) {
                auto it = cores.find(id);
                if (it == cores.end()) {
                    cerr << "Cpuset must be disjoint" << endl;
                    return EXIT_FAILURE;
                } else {
                    cores.erase(it);
                }
                CPU_SET(id, &cpuset);
            }

            worker_threads.push_back(move(judge::start_worker(worker_id++, cpuset, worker.literal, testcase_queue)));
        }
    }

    if (vm.count("workers")) {
        auto workers = vm.at("workers").as<unsigned>();
        if (cores.size() < workers) {
            cerr << "Not enough cores" << endl;
            return EXIT_FAILURE;
        }
        unsigned i = 0;
        auto it = cores.begin();
        for (; i < workers; ++i, ++it) {
            int cpuid = *it;
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(cpuid, &cpuset);
            worker_threads.push_back(move(judge::start_worker(worker_id++, cpuset, to_string(cpuid), testcase_queue)));
        }
    }

    for (auto& th : worker_threads)
        th.join();
    return 0;
}
