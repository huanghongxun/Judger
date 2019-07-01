#include <gflags/gflags.h>
#include <glog/logging.h>
#include <sys/wait.h>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <regex>
#include <set>
#include <thread>
#include "client/client.hpp"
#include "common/msg_queue.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "server/moj/moj.hpp"
#include "server/server.hpp"
#include "server/sicily/sicily.hpp"
using namespace std;

judge::message::queue testcase_queue;
set<int> client_pid;

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

void put_error_codes() {
    set_env("E_SUCCESS", to_string(judge::error_codes::E_SUCCESS));
    set_env("E_INTERNAL_ERROR", to_string(judge::error_codes::E_INTERNAL_ERROR));
    set_env("E_ACCEPTED", to_string(judge::error_codes::E_ACCEPTED));
    set_env("E_WRONG_ANSWER", to_string(judge::error_codes::E_WRONG_ANSWER));
    set_env("E_PRESENTATION_ERROR", to_string(judge::error_codes::E_PRESENTATION_ERROR));
    set_env("E_COMPILER_ERROR", to_string(judge::error_codes::E_COMPILER_ERROR));
    set_env("E_RANDOM_GEN_ERROR", to_string(judge::error_codes::E_RANDOM_GEN_ERROR));
    set_env("E_COMPARE_ERROR", to_string(judge::error_codes::E_COMPARE_ERROR));
    set_env("E_RUNTIME_ERROR", to_string(judge::error_codes::E_RUNTIME_ERROR));
    set_env("E_FLOATING_POINT", to_string(judge::error_codes::E_FLOATING_POINT));
    set_env("E_SEG_FAULT", to_string(judge::error_codes::E_SEG_FAULT));
    set_env("E_OUTPUT_LIMIT", to_string(judge::error_codes::E_OUTPUT_LIMIT));
    set_env("E_TIME_LIMIT", to_string(judge::error_codes::E_TIME_LIMIT));
    set_env("E_MEM_LIMIT", to_string(judge::error_codes::E_MEM_LIMIT));
}

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    CHECK(getenv("RUNGUARD"))  // RUNGUARD 环境变量将传给 exec/check/standard/run 评测脚本使用
        << "RUNGUARD environment variable should be specified. This env points out where the runguard executable locates in.";

    put_error_codes();

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
        ("server", po::value<unsigned>(), "set the core the monitor server occupies")
        ("clients", po::value<unsigned>(), "set number of single thread judge clients to be kept")
        ("client", po::value<vector<cpuset>>(), "run a judge client which cpuset is given")
        ("exec-dir", po::value<string>(), "set the default predefined executables for falling back")
        ("cache-dir", po::value<string>(), "set the directory to store cached test data, compiled spj, random test generator, compiled executables")
        ("run-dir", po::value<string>(), "set the directory to run user programs, store compiled user program")
        ("chroot-dir", po::value<string>(), "set the chroot directory")
        ("cache-random-data", po::value<size_t>(), "set the maximum number of cached generated random data, default to 100.")
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
             << "\tSCRIPTTIMELIMIT: time limit for scripts in seconds" << endl
             << "\tSCRIPTMEMLIMIT: memory limit for scripts in KB" << endl
             << "\tSCRIPTFILELIMIT: file limit for scripts in bytes" << endl
             << "Usage: " << argv[0] << " [options]" << endl;
        cout << desc << endl;
        return EXIT_SUCCESS;
    }

    if (vm.count("version")) {
        cout << "judge-system 4.0" << endl;
        return EXIT_SUCCESS;
    }

    if (vm.count("exec-dir")) {
        judge::EXEC_DIR = filesystem::path(vm.at("exec-dir").as<string>());
        set_env("JUDGE_UTILS", judge::EXEC_DIR, true);
    }

    if (vm.count("cache-dir")) {
        judge::CACHE_DIR = filesystem::path(vm.at("cache-dir").as<string>());
    }

    if (vm.count("run-dir")) {
        judge::RUN_DIR = filesystem::path(vm.at("run-dir").as<string>());
    }

    if (vm.count("chroot-dir")) {
        judge::CHROOT_DIR = filesystem::path(vm.at("chroot-dir").as<string>());
    }

    if (vm.count("cache-random-data")) {
        judge::MAX_RANDOM_DATA_NUM = vm["cache-random-data"].as<size_t>();
    }

    if (vm.count("enable-sicily")) {
        auto sicily_servers = vm.at("enable-scicily").as<vector<string>>();
        for (auto& sicily_server : sicily_servers) {
            auto sicily_judger = make_unique<judge::server::sicily::configuration>();
            sicily_judger->init(sicily_server);
            judge::server::register_judge_server(move(sicily_judger));
        }
    }

    if (vm.count("enable")) {
        // TODO: not implemented
    }

    if (vm.count("enable-3")) {
        auto third_servers = vm.at("enable-3").as<vector<string>>();
        for (auto& third_server : third_servers) {
            auto third_judger = make_unique<judge::server::moj::configuration>();
            third_judger->init(third_server);
            judge::server::register_judge_server(move(third_judger));
        }
    }

    if (vm.count("enable-2")) {
        // TODO: not implemented
    }

    int server_cpuid = -1;
    if (vm.count("server")) {
        auto i = vm.at("server").as<unsigned>();
        if (i >= cpus) {
            cerr << "Not enough cores" << endl;
            return EXIT_FAILURE;
        }
        server_cpuid = i;
    }

    if (getuid() != 0) {
        cerr << "You should run this program in privileged mode" << endl;
        return EXIT_FAILURE;
    }

    set<unsigned> cores;
    vector<thread> client_threads;

    // 评测服务端需要占用一个 cpu 核心
    for (unsigned i = 0; i < cpus; ++i) {
        if (i != server_cpuid)
            cores.insert(i);
    }

    if (vm.count("client")) {
        auto clients = vm.at("client").as<vector<cpuset>>();
        for (auto& client : clients) {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);

            for (unsigned id : client.ids) {
                auto it = cores.find(id);
                if (it == cores.end()) {
                    cerr << "Cpuset must be disjoint" << endl;
                    return EXIT_FAILURE;
                } else {
                    cores.erase(it);
                }
                CPU_SET(it, &cpuset);
            }

            client_threads.push_back(move(judge::client::start_client(cpuset, client.literal, testcase_queue)));
        }
    }

    if (vm.count("clients")) {
        auto clients = vm.at("clients").as<unsigned>();
        if (cores.size() < clients) {
            cerr << "Not enough cores" << endl;
            return EXIT_FAILURE;
        }
        unsigned i = 0;
        auto it = cores.begin();
        for (; i < clients; ++i, ++it) {
            int cpuid = *it;
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(cpuid, &cpuset);
            client_threads.push_back(move(judge::client::start_client(cpuset, to_string(cpuid), testcase_queue)));
        }
    }

    // TODO: termination recovery
    for (auto& th : client_threads)
        th.join();
    return 0;
}
