#include "test/mock_judge_server.hpp"
#include "config.hpp"

namespace judge::server::mock {
using namespace std;
using namespace nlohmann;

configuration::configuration()
    : exec_mgr(CACHE_DIR, EXEC_DIR) {}

void configuration::init(const filesystem::path &) {
}

string configuration::category() const {
    return "mock";
}

const executable_manager &configuration::get_executable_manager() const {
    return exec_mgr;
}

bool configuration::fetch_submission(unique_ptr<submission> &) {
    return false;
}

void configuration::summarize_invalid(submission &) {
}

void configuration::summarize(submission &) {
}

}  // namespace judge::server::mock
