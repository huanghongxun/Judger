#pragma once

#include <string>
#include "server/status.hpp"

namespace judge::client {

struct runguard_result {
    /**
     * @brief runguard 运行结果状态
     * 不会为 WRONG_ANSWER、PRESENTATION_ERROR
     */
    status judge_status;

    /**
     * @brief 时钟时间
     * 单位为秒
     */
    double wall_time;

    /**
     * @brief 用户时间（指在用户态下运行的 CPU 时间）
     * 单位为秒，如果是多线程程序，所有线程的 CPU 时间会累加
     */
    double user_time;

    /**
     * @brief 系统时间（指在内核态下运行的时间）
     * 单位为秒，如果是多线程程序，所有线程的 CPU 时间会累加  
     */
    double sys_time;

    /**
     * @brief CPU 时间
     * 单位为秒，如果是多线程程序，所有线程的 CPU 时间会累加  
     */
    double cpu_time;

    int exitcode;

    int received_signal;

    std::string internal_error;

    /**
     * @brief 实际内存使用（单位为字节）
     */
    int memory;
};

}