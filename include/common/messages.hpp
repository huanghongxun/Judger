#pragma once

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
};

}  // namespace judge::message
