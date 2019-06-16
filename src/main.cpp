#include <glog/logging.h>
#include <sys/wait.h>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <set>
#include <regex>
#include <thread>
#include "config.hpp"
#include "msg_queue.hpp"
#include "client/client.hpp"
#include "server/server.hpp"
#include "server/sicily/sicily.hpp"
#include "server/moj/moj.hpp"
using namespace std;

judge::message::queue testcase_queue, result_queue;
set<int> client_pid;

void sig_chld_handler(int sig) {
    pid_t p;
    int status;

    while ((p = waitpid(-1, &status, WNOHANG)) != -1) {
    }
}

struct cpuset {
    string literal;
    set<int> ids;
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
        if (regex_search(s, matches, matcher)) {
            if (matches[3].str().empty()) {
                int i = boost::lexical_cast<int>(matches[1].str());
                result.ids.insert(i);
                CPU_SET(i, &result.cpuset);
            } else {
                int begin = boost::lexical_cast<int>(matches[1].str());
                int end = boost::lexical_cast<int>(matches[3].str());
                if (begin > end || begin < 0 || end < 0)
                    throw validation_error(validation_error::invalid_option_value);
                for (int i = begin; i <= end; ++i) {
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

int main(int argc, const char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    if (!getenv("RUNGUARD")) {
        // RUNGUARD 环境变量将传给 exec/check/standard/run 评测脚本使用
        cerr << "RUNGUARD environment variable should be specified. This env points out where the runguard executable locates in." << endl;
        return EXIT_FAILURE;
    }

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
        ("server", po::value<size_t>(), "set the core the judge server occupies")
        ("clients", po::value<size_t>(), "set number of single thread judge clients to be kept")
        ("client", po::value<vector<cpuset>>(), "run a judge client which cpuset is given")
        ("exec-dir", po::value<string>(), "set the default predefined executables for falling back")
        ("cache-dir", po::value<string>(), "set the directory to store cached test data, compiled spj, random test generator, compiled executables")
        ("run-dir", po::value<string>(), "set the directory to run user programs, store compiled user program")
        ("chroot-dir", po::value<string>(), "set the chroot directory")
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

    if (vm.count("enable-sicily")) {
        auto sicily_servers = vm.at("enable-scicily").as<vector<string>>();
        for (auto &sicily_server : sicily_servers) {
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
        for (auto &third_server : third_servers) {
            auto third_judger = make_unique<judge::server::moj::configuration>();
            third_judger->init(third_server);
            judge::server::register_judge_server(move(third_judger));
        }
    }

    if (vm.count("enable-2")) {
        // TODO: not implemented
    }

    int server_cpuid = 0;
    if (vm.count("server")) {
        auto i = vm.at("server").as<size_t>();
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

    set<int> cores;
    cpu_set_t servercpuset;
    CPU_ZERO(&servercpuset);
    CPU_SET(server_cpuid, &servercpuset);
    int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &servercpuset);
    if (ret < 0) throw std::system_error();

    // 评测服务端需要占用一个 cpu 核心
    for (unsigned i = 0; i < cpus; ++i) {
        if (i != server_cpuid)
            cores.insert(i);
    }

    if (vm.count("client")) {
        auto clients = vm.at("client").as<vector<cpuset>>();
        for (auto& client : clients) {
            for (int id : client.ids) {
                auto it = cores.find(id);
                if (it == cores.end()) {
                    cerr << "Cpuset must be disjoint" << endl;
                    return EXIT_FAILURE;
                } else {
                    cores.erase(it);
                }
            }

            judge::client::start_client(servercpuset, client.literal, testcase_queue, result_queue);
        }
    }

    if (vm.count("clients")) {
        auto clients = vm.at("clients").as<size_t>();
        if (cores.size() < clients) {
            cerr << "Not enough cores" << endl;
            return EXIT_FAILURE;
        }
        size_t i = 0;
        auto it = cores.begin();
        for (; i < clients; ++i, ++it) {
            int cpuid = *it;
            judge::client::start_client(servercpuset, to_string(cpuid), testcase_queue, result_queue);
        }
    }

    judge::server::server(testcase_queue, result_queue);
}
