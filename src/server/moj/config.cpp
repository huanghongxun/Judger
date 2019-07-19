#include "server/moj/config.hpp"

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;

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

}  // namespace judge::server::moj
