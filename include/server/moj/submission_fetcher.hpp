#pragma once

#include "SimpleAmqpClient/SimpleAmqpClient.h"
#include "server/moj/moj.hpp"

namespace judge::server::moj {

struct submission_fetcher {
    submission_fetcher(amqp &amqp);
    bool fetch(string &message, unsigned int timeout = -1);
    void ack();

private:
    AmqpClient::Channel::ptr_t channel;
    AmqpClient::Envelope::ptr_t envelope;
    string tag;
};

}  // namespace judge::server::moj