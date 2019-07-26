#include "judge/judger.hpp"

namespace judge {
using namespace std;

void judger::on_judge_finished(function<void(submission &)> callback) {
    judge_finished.push_back(callback);
}

void judger::fire_judge_finished(submission &submit) const {
    for (auto &f : judge_finished) f(submit);
}

}  // namespace judge
