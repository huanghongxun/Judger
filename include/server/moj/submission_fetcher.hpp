#pragma once

#include "SimpleAmqpClient/SimpleAmqpClient.h"
#include "server/moj/config.hpp"

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

struct judge_result_reporter {
    judge_result_reporter(amqp &queue);
    void report(const string &message);

private:
    AmqpClient::Channel::ptr_t channel;
    judge::server::moj::amqp queue;
};

}  // namespace judge::server::moj
