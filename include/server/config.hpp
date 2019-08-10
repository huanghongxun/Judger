#pragma once

#include "common/json_utils.hpp"

namespace judge::server {

/**
 * @brief 描述一个 AMQP 消息队列的配置数据结构
 */
struct amqp {
    /**
     * @brief AMQP 消息队列的主机地址
     */
    std::string hostname;

    /**
     * @brief AMQP 消息队列的主机端口
     */
    int port;

    /**
     * @brief 通过该结构体发送的消息的 Exchange 名
     */
    std::string exchange;

    /**
     * @brief Exchange 类型，可选 direct, topic, fanout
     */
    std::string exchange_type;

    /**
     * @brief AMQP 消息队列的队列名
     */
    std::string queue;

    /**
     * @brief AMQP 消息队列的 Routing Key
     */
    std::string routing_key;
};

void from_json(const nlohmann::json &j, amqp &mq);

/**
 * @brief 描述一个 MySQL 数据库连接信息
 */
struct database {
    /**
     * @brief 数据库服务器的地址
     */
    std::string host;

    /**
     * @brief 数据库服务器的账号
     */
    std::string user;

    /**
     * @brief 数据库服务器的密码
     */
    std::string password;

    /**
     * @brief 使用连接到的数据库服务器的哪一个数据库
     */
    std::string database;
};

void from_json(const nlohmann::json &j, database &db);

/**
 * redis 的登录情况
 */
struct redis {
    /**
     * @brief redis 服务器地址
     */
    std::string host;

    /**
     * @brief redis 服务器端口
     */
    int port;

    /**
     * @brief 重试时间间隔，单位毫秒
     */
    unsigned retry_interval;

    /**
     * @brief 密码，若不为空，则使用该密码登录
     */
    std::string password;

    /**
     * @brief 发布的通道名，允许服务端通过 subscribe 来监听 redis 的更改
     * 如果为空，那么 publish 操作都改成 set 操作
     */
    std::string channel;
};

void from_json(const nlohmann::json &j, redis &redis_config);

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

void from_json(const nlohmann::json &j, time_limit_config &limit);

struct system_config {
    time_limit_config time_limit;

    /**
     * @brief 返回的评测报告的最大长度
     * 如果长度超限，那么应该想办法将报告的长度减小，比如不再返回测试数据内容
     */
    std::size_t max_report_io_size;

    /**
     * @brief 文件系统根路径
     */
    std::string file_api;
};

void from_json(const nlohmann::json &j, system_config &config);

}  // namespace judge::server
