#include "server/rabbitmq.hpp"
#include <glog/logging.h>

namespace judge::server {
using namespace std;

mq_publisher::mq_publisher(amqp &amqp) {
    channel = AmqpClient::Channel::Create(amqp.hostname, amqp.port);
    channel->DeclareQueue(amqp.queue, /* passive */ false, /* durable */ true, /* exclusive */ false, /* auto_delete */ false);
    channel->DeclareExchange(amqp.exchange, amqp.exchange_type, /* passive */ false, /* durable */ true);
    channel->BindQueue(amqp.queue, amqp.exchange, amqp.routing_key);
    channel->BasicConsume(amqp.queue, /* consumer tag */ "", /* no_local */ true, /* no_ack */ false, /* exclusive */ false);
}

bool mq_publisher::fetch(AmqpClient::Envelope::ptr_t &envelope, unsigned int timeout) {
    return channel->BasicConsumeMessage(envelope, timeout);
}

void mq_publisher::ack(const AmqpClient::Envelope::ptr_t &envelope) {
    channel->BasicAck(envelope);
}

mq_consumer::mq_consumer(amqp &amqp) : queue(amqp) {
    channel = AmqpClient::Channel::Create(amqp.hostname, amqp.port);
    channel->DeclareQueue(amqp.queue, /* passive */ false, /* durable */ true, /* exclusive */ false, /* auto_delete */ false);
    channel->DeclareExchange(amqp.exchange, amqp.exchange_type, /* passive */ false, /* durable */ true);
    channel->BindQueue(amqp.queue, amqp.exchange, amqp.routing_key);
}

void mq_consumer::report(const string &message) {
    AmqpClient::BasicMessage::ptr_t msg = AmqpClient::BasicMessage::Create(message);
    DLOG(INFO) << "Sending message to exchange:" << queue.exchange << ", routing_key=" << queue.routing_key << std::endl
               << message;
    
    channel->BasicPublish(queue.exchange, queue.routing_key, msg);
    DLOG(INFO) << "Sending message succeeded";
}

}  // namespace judge::server
