#pragma once

#include <filesystem>
#include "common/status.hpp"

namespace judge {

struct runguard_result {
    /**
     * @brief 时钟时间
     * 单位为秒
     */
    double wall_time = -1;

    /**
     * @brief 用户时间（指在用户态下运行的 CPU 时间）
     * 单位为秒，如果是多线程程序，所有线程的 CPU 时间会累加
     */
    double user_time = -1;

    /**
     * @brief 系统时间（指在内核态下运行的时间）
     * 单位为秒，如果是多线程程序，所有线程的 CPU 时间会累加  
     */
    double sys_time = -1;

    /**
     * @brief CPU 时间
     * 单位为秒，如果是多线程程序，所有线程的 CPU 时间会累加  
     */
    double cpu_time = -1;

    int exitcode = -1;

    int signal = -1;

    std::string internal_error;

    /**
     * @brief 实际内存使用（单位为字节）
     */
    int memory = -1;

    std::string time_result;
};

runguard_result read_runguard_result(const std::filesystem::path &metafile);

}  // namespace judge
