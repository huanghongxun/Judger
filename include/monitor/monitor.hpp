#pragma once

#include "common/messages.hpp"
#include "judge/submission.hpp"
#include "worker_state.hpp"

namespace judge {

/**
 * @brief 执行监控行为
 */
struct monitor {
    virtual ~monitor();

    /**
     * @brief 监控上报当前已经拉取到一个提交
     */
    virtual void start_submission(const submission &submit);

    /**
     * @brief 监控上报当前已经开始某个评测子任务
     * @param worker_id 执行评测子任务的 Worker 编号
     * @param client_task 当前评测子任务的信息，属于哪个提交的第几个评测子任务
     */
    virtual void start_judge_task(int worker_id, const message::client_task &client_task);

    /**
     * @brief 监控上报当前某个评测子任务已经评测结束
     * @param worker_id 执行评测子任务的 Worker 编号
     * @param client_task 当前评测子任务的信息，属于哪个提交的第几个评测子任务
     */
    virtual void end_judge_task(int worker_id, const message::client_task &client_task);

    /**
     * @brief 监控上报当前某个 Worker 的状态
     * @param worker_id Worker 编号
     * @param state Worker 的新状态
     * @param information 如果 Worker 崩溃，则为错误原因，用于日志记录
     */
    virtual void worker_state_changed(int worker_id, worker_state state, const std::string &information);

    /**
     * @brief 监控上报当前已经完成一个提交的评测
     * @param submit 提交
     */
    virtual void end_submission(const submission &submit);
};

}  // namespace judge
