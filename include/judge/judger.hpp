#pragma once

#include "common/concurrent_queue.hpp"
#include "common/messages.hpp"
#include "judge/submission.hpp"

namespace judge {
using namespace std;

struct judger {
    /**
     * @brief judger 负责评测哪种类型的提交
     * 目前可以为：programming, choice, output, program_blank_filling
     */
    virtual string type() = 0;

    /**
     * @brief 检查 submission 是不是当前 judger 所负责的，而且内部约束都满足
     * @param submit 要检查正确性的 submission
     * @return true 若 submission 是合法的
     */
    virtual bool verify(unique_ptr<submission> &submit) = 0;

    /**
     * @brief 将检查通过的 submission 拆解成评测子任务并分发到内部的消息队列中等待后续处理
     * 调用方确保 submit 是已经通过验证的
     * @param task_queue 评测子任务的消息队列
     * @param submit 要被评测的提交信息
     * @return true 若成功分发子任务
     */
    virtual bool distribute(concurrent_queue<message::client_task> &task_queue, unique_ptr<submission> &submit) = 0;

    /**
     * @brief 当前从消息队列中取到该消息的 worker 将评测子任务发给 judger 进行实际的评测
     * @param task 拆解后的子任务
     * @param task_queue 允许子任务评测完成后继续分发后续的子任务评测
     * @param execcpuset 当前评测任务可以使用哪些 cpu 核心进行评测
     */
    virtual void judge(const message::client_task &task, concurrent_queue<message::client_task> &task_queue, const string &execcpuset) = 0;

    void on_judge_finished(function<void(submission &)> callback);

protected:
    void fire_judge_finished(submission &submit);

private:
    vector<function<void(submission &)>> judge_finished;
};

}  // namespace judge
