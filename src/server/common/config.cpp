#include "server/common/config.hpp"

namespace judge::server {
using namespace std;
using namespace nlohmann;

void from_json(const json &j, amqp &mq) {
    j.at("port").get_to(mq.port);
    j.at("exchange").get_to(mq.exchange);
    j.at("hostname").get_to(mq.hostname);
    j.at("queue").get_to(mq.queue);
    if (j.count("routing_key"))
        j.at("routing_key").get_to(mq.routing_key);
    else
        mq.routing_key = mq.queue;
}

void from_json(const json &j, database &db) {
    j.at("host").get_to(db.host);
    j.at("user").get_to(db.user);
    j.at("password").get_to(db.password);
    j.at("database").get_to(db.database);
}

}  // namespace judge::server::moj
