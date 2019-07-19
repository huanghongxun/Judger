#include "server/mcourse/feedback.hpp"

namespace judge::server::mcourse {
using namespace std;
using namespace nlohmann;

void to_json(json &j, const compile_check_report &report) {
    j = report.message;
}

}  // namespace judge::server::mcourse
