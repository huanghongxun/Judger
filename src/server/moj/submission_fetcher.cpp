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

}  // namespace judge::server::moj