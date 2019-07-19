#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include "server/common/config.hpp"

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;

/**
 * @brief 时间限制配置
 */
struct time_limit_config {
    /**
     * @brief 编译时间限制（单位为秒）
     */
    double compile_time_limit;

    /**
     * @brief 静态检查时限（单位为秒）
     */
    double oclint;

    /**
     * @brief 
     */
    double crun;

    /**
     * @brief 内存检查时限（单位为秒）
     */
    double valgrind;

    /**
     * @brief 随机数据生成器运行时限（单位为秒）
     */
    double random_generator;
};

struct system_config {
    time_limit_config time_limit;

    size_t max_report_io_size;

    string file_api;
};

void from_json(const json &j, system_config &config);
void from_json(const json &j, time_limit_config &limit);

}  // namespace judge::server::moj