#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace judge::server {
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
     * @brief Exchange 类型，可选 direct, topic, fanout
     */
    string exchange_type;

    /**
     * @brief AMQP 消息队列的队列名
     */
    string queue;

    /**
     * @brief AMQP 消息队列的 Routing Key
     */
    string routing_key;
};

void from_json(const json &j, amqp &mq);

/**
 * @brief 描述一个 MySQL 数据库连接信息
 */
struct database {
    /**
     * @brief 数据库服务器的地址
     */
    string host;

    /**
     * @brief 数据库服务器的账号
     */
    string user;

    /**
     * @brief 数据库服务器的密码
     */
    string password;

    /**
     * @brief 使用连接到的数据库服务器的哪一个数据库
     */
    string database;
};

void from_json(const json &j, database &db);

struct redis {
    string host;
    int port;
    string password;
};

void from_json(const json &j, redis &redis_config);

}  // namespace judge::server
