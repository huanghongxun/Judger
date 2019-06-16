#pragma once

#include <limits>
#include <string>
#include <vector>

struct time_limit {
    double soft, hard;
};

struct runguard_options {
    std::string cgroupname;
    std::string chroot_dir;
    std::string work_dir;
    size_t nproc = std::numeric_limits<size_t>::max();
    int user_id = -1;
    int group_id = -1;
    std::string cpuset;  // processor id to run client program.

    bool use_wall_limit = false;
    struct time_limit wall_limit;  // wall clock time
    bool use_cpu_limit = false;
    struct time_limit cpu_limit;  // CPU time

    int64_t memory_limit = -1;  // Memory limit in bytes
    int file_limit = -1;        // Output limit
    int stream_size = -1;
    bool no_core_dumps = false;

    std::string stdin_filename;
    std::string stdout_filename;
    std::string stderr_filename;

    bool preserve_sys_env = false;
    std::vector<std::string> env;

    std::string metafile_path;
    std::vector<std::string> command;
};
