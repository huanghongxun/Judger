#include "server/moj/submission_fetcher.hpp"

namespace judge::server::moj {

submission_fetcher::submission_fetcher(amqp &amqp) {
    channel = AmqpClient::Channel::Create(amqp.hostname, amqp.port);
    channel->DeclareQueue(amqp.queue, /* passive */ false, /* durable */ true, /* exclusive */ false, /* auto_delete */ false);
    channel->BasicConsume(amqp.queue, /* consumer tag */ "", /* no_local */ true, /* no_ack */ false, /* exclusive */ false);
}

bool submission_fetcher::fetch(string &message, unsigned int timeout) {
    if (!channel->BasicConsumeMessage(envelope, timeout))
        return false;
    message = envelope->Message()->Body();
    return true;
}

void submission_fetcher::ack() {
    channel->BasicAck(envelope);
    envelope = nullptr;
}

judge_result_reporter::judge_result_reporter(judge::server::moj::amqp &queue) : queue(queue) {
    channel = AmqpClient::Channel::Create(queue.hostname, queue.port);
}

void judge_result_reporter::report(const string &message) {
    AmqpClient::BasicMessage::ptr_t msg = AmqpClient::BasicMessage::Create(message);
    channel->BasicPublish(queue.exchange, queue.routing_key, msg, true);
}

}  // namespace judge::server::moj
