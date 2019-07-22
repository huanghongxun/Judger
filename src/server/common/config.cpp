#include "server/common/config.hpp"

namespace judge::server {
using namespace std;
using namespace nlohmann;

void from_json(const json &j, amqp &mq) {
    j.at("port").get_to(mq.port);
    j.at("exchange").get_to(mq.exchange);
    if (j.count("exchange_type"))
        j.at("exchange_type").get_to(mq.exchange_type);
    else
        mq.exchange_type = "direct";
    j.at("hostname").get_to(mq.hostname);
    j.at("queue").get_to(mq.queue);
    if (j.count("routing_key"))
        j.at("routing_key").get_to(mq.routing_key);
    else
        mq.routing_key = "";
}

void from_json(const json &j, database &db) {
    j.at("host").get_to(db.host);
    j.at("user").get_to(db.user);
    j.at("password").get_to(db.password);
    j.at("database").get_to(db.database);
}

void from_json(const json &j, redis &redis_config) {
    j.at("host").get_to(redis_config.host);
    j.at("port").get_to(redis_config.port);
    if (j.count("password"))
        j.at("password").get_to(redis_config.password);
    else
        redis_config.password = "";
    if (j.count("channel"))
        j.at("channel").get_to(redis_config.channel);
    else
        redis_config.channel = "";
}

void from_json(const json &j, system_config &config) {
    j.at("maxReportIOSize").get_to(config.max_report_io_size);
    j.at("timeLimit").get_to(config.time_limit);
    j.at("fileApi").get_to(config.file_api);
}

void from_json(const json &j, time_limit_config &limit) {
    j.at("cCompile").get_to(limit.compile_time_limit);
    j.at("crun").get_to(limit.crun);
    j.at("oclint").get_to(limit.oclint);
    j.at("randomGenerate").get_to(limit.random_generator);
    j.at("valgrind").get_to(limit.valgrind);
}

}  // namespace judge::server
