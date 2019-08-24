#include "monitor/monitor.hpp"
namespace judge {

monitor::~monitor() {
}

void monitor::start_submission(const submission &submit) {
}

void monitor::start_judge_task(int worker_id, const message::client_task &client_task) {
}

void monitor::end_judge_task(int worker_id, const message::client_task &client_task) {
}

void monitor::worker_state_changed(int worker_id, worker_state state, const std::string &information) {
}

void monitor::end_submission(const submission &submit) {
}

}  // namespace judge
