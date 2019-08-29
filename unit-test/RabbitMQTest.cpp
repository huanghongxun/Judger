#include "env.hpp"
#include "gtest/gtest.h"
#include "judge/programming.hpp"
#include "server/rabbitmq.hpp"

using namespace std;
using namespace std::filesystem;
using namespace judge;

class DISABLED_RabbitMQTest : public ::testing::Test {
};

TEST_F(DISABLED_RabbitMQTest, MultipleConsumerTest) {
    judge::server::amqp config = {
        .hostname = "192.168.1.111",
        .port = 5673,
        .exchange = "CourseTestSubmission",
        .exchange_type = "direct",
        .queue = "CourseTestSubmission",
        .routing_key = "CourseTestSubmission",
        .concurrency = 4
    };
    judge::server::rabbitmq mq(config, false);
    AmqpClient::Envelope::ptr_t e1, e2;
    EXPECT_TRUE(mq.fetch(e1));
    EXPECT_TRUE(mq.fetch(e2));
}
