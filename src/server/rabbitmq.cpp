#include "server/rabbitmq.hpp"
#include <glog/logging.h>
#include <thread>

namespace judge::server {
using namespace std;

rabbitmq::rabbitmq(amqp &amqp, bool write) : queue(amqp), write(write) {
    connect();
}

void rabbitmq::connect() {
    channel = AmqpClient::Channel::Create(queue.hostname, queue.port);
    channel->DeclareQueue(queue.queue, /* passive */ false, /* durable */ true, /* exclusive */ false, /* auto_delete */ false);
    channel->DeclareExchange(queue.exchange, queue.exchange_type, /* passive */ false, /* durable */ true);
    channel->BindQueue(queue.queue, queue.exchange, queue.routing_key);
    if (!write)  // 对于从消息队列读取消息的情况，我们需要监听队列
        channel->BasicConsume(queue.queue, /* consumer tag */ "", /* no_local */ true, /* no_ack */ false, /* exclusive */ false);
}

bool rabbitmq::fetch(AmqpClient::Envelope::ptr_t &envelope, unsigned int timeout) {
    int retry = 5;
    while (retry-- > 0)
        try {
            return channel->BasicConsumeMessage(envelope, timeout);
        } catch (std::exception &e) {
            if (retry <= 0) throw e;
            sleep(5);
            connect();
        }
}

void rabbitmq::ack(const AmqpClient::Envelope::ptr_t &envelope) {
    channel->BasicAck(envelope);
}

void rabbitmq::report(const string &message) {
    AmqpClient::BasicMessage::ptr_t msg = AmqpClient::BasicMessage::Create(message);
    DLOG(INFO) << "Sending message to exchange:" << queue.exchange << ", routing_key=" << queue.routing_key << std::endl
               << message;

    channel->BasicPublish(queue.exchange, queue.routing_key, msg);
    DLOG(INFO) << "Sending message succeeded";
}

}  // namespace judge::server
