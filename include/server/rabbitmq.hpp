#pragma once

#include <chrono>
#include <mutex>
#include "SimpleAmqpClient/SimpleAmqpClient.h"
#include "server/config.hpp"

namespace judge::server {

/**
 * @brief 与消息队列交互的类
 */
struct rabbitmq {
    rabbitmq(amqp &amqp, bool write);
    bool fetch(AmqpClient::Envelope::ptr_t &envelope, unsigned int timeout = -1);
    void ack(const AmqpClient::Envelope::ptr_t &envelope);
    void report(const std::string &message);

private:
    void connect();
    void try_connect(bool force);

    AmqpClient::Channel::ptr_t channel;
    std::string tag;
    judge::server::amqp queue;
    bool write;
    std::mutex mut;
};

}  // namespace judge::server
