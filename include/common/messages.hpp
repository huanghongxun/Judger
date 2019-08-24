#pragma once

#include <boost/thread/latch.hpp>
#include <mutex>
#include <vector>
#include <condition_variable>
#include "judge/submission.hpp"

namespace judge::message {

/**
 * @brief 评测服务端发送给评测客户端的评测任务
 */
struct client_task {
    /**
     * @brief 当前客户端所评测的提交
     */
    submission *submit;

    /**
     * @brief 本测试点的 id
     * 对于编程题评测，为当前评测点在 submit.test_cases 的下标
     * 用于返回评测结果的时候统计
     */
    std::size_t id;

    /**
     * @brief 本测试点的中文识别名
     * 监控系统查看当前正在评测的题目类型使用
     */
    std::string name;

    /**
     * @brief 执行该评测子任务需要多少个核心
     */
    std::size_t cores;
};

struct core_request {
    std::vector<size_t> *core_ids;

    std::mutex *write_lock;

    /**
     * @brief 在分配核心后从 worker 调用该锁
     * 当核心数足够后锁被释放，主 worker 将开始评测
     */
    boost::latch *lock;

    /**
     * @brief 从 worker 的等待评测完成所用的锁
     * 主 worker 在评测结束后将释放该锁
     */
    std::condition_variable *cv;

    std::mutex *mut;
};

}  // namespace judge::message
