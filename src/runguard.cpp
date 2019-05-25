#include <sys/sysinfo.h>
#include <string>
#include <map>

using namespace std;

struct runguard_options
{
    string chroot_dir;
    size_t nproc = LONG_MAX;
    bool use_user = false;
    string user_id;
    bool use_group = false;
    string group_id;
    int cpuset; // processor id to run client program.
    int time = -1; // wall clock time
    int cpu_time = -1; // CPU time
    int memory_limit = -1; // Memory limit in KB
    int file_size = -1; // Output limit

    bool redirect_stderr = false;
    string stdout_filename;
    bool redirect_stdout = false;
    string stderr_filename;

    int stream_size = -1;
    map<string, string> env;
};

void check_cpuset(int cpuset)
{
    int nprocs = get_nprocs_conf();
    if (cpuset < 0 || cpuset >= nprocs)
        
}

void runguard_main(const runguard_options &options)
{

}