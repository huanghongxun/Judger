#pragma once

#include "SimpleAmqpClient/SimpleAmqpClient.h"
#include "server/config.hpp"

namespace judge::server {

/**
 * @brief 从消息队列中获取消息的类
 */
struct mq_publisher {
    mq_publisher(amqp &amqp);
    bool fetch(AmqpClient::Envelope::ptr_t &envelope, unsigned int timeout = -1);
    void ack(const AmqpClient::Envelope::ptr_t &envelope);

private:
    AmqpClient::Channel::ptr_t channel;
    std::string tag;
};

/**
 * @brief 将消息发送给消息队列的类
 */
struct mq_consumer {
    mq_consumer(amqp &queue);
    void report(const std::string &message);

private:
    AmqpClient::Channel::ptr_t channel;
    judge::server::amqp queue;
};

}  // namespace judge::server