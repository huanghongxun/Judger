#include "config.hpp"
#include <boost/assign.hpp>
#include <fstream>
#include "common/messages.hpp"
#include "common/stl_utils.hpp"
#include "common/utils.hpp"
#include "server/moj/moj.hpp"

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;

void from_json(const json &j, system_config &config) {
    j.at("maxReportIOSize").get_to(config.max_report_io_size);
    j.at("timeLimit").get_to(config.time_limit);
    j.at("fileApi").get_to(config.file_api);
}

void from_json(const json &j, amqp &mq) {
    j.at("port").get_to(mq.port);
    j.at("exchange").get_to(mq.exchange);
    j.at("hostname").get_to(mq.hostname);
    j.at("queue").get_to(mq.queue);
    if (j.count("routing_key"))
        j.at("routing_key").get_to(mq.routing_key);
}

void from_json(const json &j, time_limit_config &limit) {
    j.at("cCompile").get_to(limit.compile_time_limit);
    j.at("crun").get_to(limit.crun);
    j.at("oclint").get_to(limit.oclint);
    j.at("randomGenerate").get_to(limit.random_generator);
    j.at("valgrind").get_to(limit.valgrind);
}

}  // namespace judge::server::moj
