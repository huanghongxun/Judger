#pragma once

#include "SimpleAmqpClient/SimpleAmqpClient.h"
#include "server/moj/config.hpp"

namespace judge::server::moj {

struct submission_fetcher {
    submission_fetcher(amqp &amqp);
    bool fetch(AmqpClient::Envelope::ptr_t &envelope, unsigned int timeout = -1);
    void ack(const AmqpClient::Envelope::ptr_t &envelope);

private:
    AmqpClient::Channel::ptr_t channel;
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
