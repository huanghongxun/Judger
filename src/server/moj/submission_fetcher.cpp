#include "server/moj/submission_fetcher.hpp"

namespace judge::server::moj {

submission_fetcher::submission_fetcher(amqp &amqp) {
    channel = AmqpClient::Channel::Create(amqp.hostname, amqp.port);
    channel->DeclareQueue(amqp.queue, /* passive */ false, /* durable */ true, /* exclusive */ false, /* auto_delete */ false);
    channel->BasicConsume(amqp.queue, /* consumer tag */ "", /* no_local */ true, /* no_ack */ false, /* exclusive */ false);
}

bool submission_fetcher::fetch(AmqpClient::Envelope::ptr_t &envelope, unsigned int timeout) {
    return channel->BasicConsumeMessage(envelope, timeout);
}

void submission_fetcher::ack(const AmqpClient::Envelope::ptr_t &envelope) {
    channel->BasicAck(envelope);
}

judge_result_reporter::judge_result_reporter(judge::server::moj::amqp &queue) : queue(queue) {
    channel = AmqpClient::Channel::Create(queue.hostname, queue.port);
}

void judge_result_reporter::report(const string &message) {
    AmqpClient::BasicMessage::ptr_t msg = AmqpClient::BasicMessage::Create(message);
    channel->BasicPublish(queue.exchange, queue.routing_key, msg, true);
}

}  // namespace judge::server::moj
