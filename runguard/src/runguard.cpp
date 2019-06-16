#include <glog/logging.h>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include "system.hpp"
#include "utils.hpp"
#include "run.hpp"

using namespace std;

void validate(boost::any& v, const vector<string>& values, size_t*, int) {
    using namespace boost::program_options;
    validators::check_first_occurrence(v);

    string const& s = validators::get_single_string(values);
    if (s[0] == '-') {
        throw validation_error(validation_error::invalid_option_value);
    }

    v = boost::lexical_cast<size_t>(s);
}

void validate(boost::any& v, const vector<string>& values, struct time_limit*, int) {
    using namespace boost::program_options;
    validators::check_first_occurrence(v);

    struct time_limit result;
    string const& s = validators::get_single_string(values);
    auto colon = s.find(':');
    string left = s.substr(0, colon);
    string right = colon < s.size() ? s.substr(colon + 1) : "";

    result.soft = boost::lexical_cast<double>(left);
    if (right.size())
        result.hard = boost::lexical_cast<double>(right);
    else
        result.hard = result.soft;

    if (result.hard < result.soft ||
        !finite(result.hard) || !finite(result.soft) ||
        result.hard < 0 || result.soft < 0)
        throw validation_error(validation_error::invalid_option_value);

    v = result;
}

int main(int argc, const char* argv[]) {
    //google::InitGoogleLogging(argv[0]);

    namespace po = boost::program_options;
    po::options_description desc("runguard options");
    po::positional_options_description pos;
    po::variables_map vm;

    struct runguard_options opt;

    // clang-format off
    desc.add_options()
        ("root,r", po::value<string>(), "run command with root directory set to root. If this option is provided, running command is executed relative to the chroot.")
        ("user,u", po::value<string>(), "run command as user with username or user id")
        ("group,g", po::value<string>(), "run command under group with groupname or group id. If only 'user' is set, this defaults to the same")
        ("wall-time,T", po::value<time_limit>(), "kill command after wall time clock seconds (floating point is acceptable)")
        ("cpu-time,t", po::value<time_limit>(), "set maximum CPU time (floating point is acceptable) consumption of the command in seconds")
        ("memory-limit,m", po::value<size_t>(), "set maximum memory consumption of the command in KB")
        ("file-limit,f", po::value<size_t>(), "set maximum created file size of the command in KB")
        ("nproc,p", po::value<size_t>(), "set maximum process living simutanously")
        ("cpuset,P", po::value<string>(), "set the processor IDs that can only be used (e.g. \"0,2-3\")")
        ("no-core-dumps", "disable core dumps")
        ("standard-input-file,i", po::value<string>(), "redirect command standard input fd to file")
        ("standard-output-file,o", po::value<string>(), "redirect command standard output fd to file")
        ("standard-error-file,e", po::value<string>(), "redirect command standard error fd to file")
        ("stream-size", po::value<size_t>(), "truncate command output streams at the size in KB")
        ("environment,E", "preseve system environment variables (or only PATH is loaded)")
        ("variable,V", po::value<vector<string>>(), "add additional environment variables (e.g. -Vkey1=value1 -Vkey2=value2)")
        ("out-meta,M", po::value<string>(), "write runguard monitor results (run time, exitcode, memory usage, ...) to file")
        ("cmd", po::value<vector<string>>()->composing()->required(), "commands")
        ("help", "display this help text")
        ("version", "display version of this application");
    // clang-format on

    pos.add("cmd", -1);

    try {
        po::store(po::command_line_parser(argc, argv)
                      .options(desc)
                      .positional(pos)
                      .allow_unregistered()
                      .run(),
                  vm);
        po::notify(vm);
    } catch (po::error& e) {
        cerr << e.what() << endl
             << endl;
        cerr << desc << endl;
        return 1;
    }

    if (vm.count("help")) {
        cout << "Runguard: Running user program in protected mode with system resource access limitations." << endl
             << "This app requires root privilege if either 'root' or 'user' option is provided." << endl
             << "Usage: " << argv[0] << " [options] -- [command]";
        cout << desc << endl;
        return 0;
    }

    if (vm.count("version")) {
        cout << "runguard" << endl;
        return 0;
    }

    if (vm.count("root")) {
        opt.chroot_dir = vm["root"].as<string>();
    }

    if (vm.count("user")) {
        string user = vm["user"].as<string>();
        if (is_number(user)) {
            opt.user_id = boost::lexical_cast<int>(user);
        } else {
            opt.user_id = get_userid(user.c_str());
        }
    }

    if (vm.count("group") || vm.count("user")) {
        string group = vm["group"].as<string>();
        if (is_number(group)) {
            opt.group_id = boost::lexical_cast<int>(group);
        } else {
            opt.group_id = get_groupid(group.c_str());
        }
    }

    if (vm.count("variable")) {
        opt.env = vm["variable"].as<vector<string>>();
    }

    if (vm.count("wall-time")) opt.use_wall_limit = true, opt.wall_limit = vm["wall-time"].as<time_limit>();
    if (vm.count("cpu-time")) opt.use_cpu_limit = true, opt.cpu_limit = vm["cpu-time"].as<time_limit>();
    if (vm.count("memory-limit")) {
        opt.memory_limit = vm["memory-limit"].as<size_t>();
        if (opt.memory_limit != (opt.memory_limit * 1024) / 1024)
            opt.memory_limit = -1;
        else
            opt.memory_limit *= 1024;
    }
    if (vm.count("file-limit")) opt.file_limit = vm["file-limit"].as<size_t>();
    if (vm.count("nproc")) opt.nproc = vm["nproc"].as<size_t>();
    if (vm.count("no-core-dumps")) opt.no_core_dumps = true;
    if (vm.count("cpuset")) opt.cpuset = vm["cpuset"].as<string>();
    if (vm.count("standard-input-file")) opt.stdin_filename = vm["standard-input-file"].as<string>();
    if (vm.count("standard-output-file")) opt.stdout_filename = vm["standard-output-file"].as<string>();
    if (vm.count("standard-error-file")) opt.stderr_filename = vm["standard-error-file"].as<string>();
    if (vm.count("stream-size")) opt.stream_size = vm["stream-size"].as<size_t>();
    if (vm.count("environment")) opt.preserve_sys_env = true;
    if (vm.count("out-meta")) opt.metafile_path = vm["out-meta"].as<string>();
    opt.command = vm["cmd"].as<vector<string>>();

    return runit(opt);
}