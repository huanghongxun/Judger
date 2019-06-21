#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;

/**
 * @brief 描述一个 AMQP 消息队列的配置数据结构
 */
struct amqp {
    /**
     * @brief AMQP 消息队列的主机地址
     */
    string hostname;

    /**
     * @brief AMQP 消息队列的主机端口
     */
    int port;

    /**
     * @brief 通过该结构体发送的消息的 Exchange 名
     */
    string exchange;

    /**
     * @brief AMQP 消息队列的队列名
     */
    string queue;

    string routing_key;
};

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
};

void from_json(const json &j, system_config &config);
void from_json(const json &j, amqp &mq);
void from_json(const json &j, time_limit_config &limit);

}  // namespace judge::server::moj