#pragma once

#include <boost/rational.hpp>
#include <variant>
#include "judge/submission.hpp"

namespace judge::message {
using namespace std;

/**
 * @brief 评测服务端发送给评测客户端的评测任务
 */
struct client_task {
    /**
     * @brief 当前客户端所评测的提交
     */
    judge::submission *submit;

    /**
     * @brief 本测试点的 id
     * 对于编程题评测，为当前评测点在 submit.test_cases 的下标
     * 用于返回评测结果的时候统计
     */
    size_t id;

    /**
     * @brief 本测试点的类型
     * 一道题的评测可以包含多种评测类型，每种评测类型都包含
     * 设定的 check script 和 run
     * 
     * 这个类型由 judge_server 自行决定，比如对于 Sicily、Judge-System 3.0:
     * 0: CompileCheck
     * 1: MemoryCheck
     * 2: RandomCheck
     * 3: StandardCheck
     * 4: StaticCheck
     * 5: GTestCheck
     * 
     * 对于 Judge-System 4.0：
     * 0: CompileCheck
     * 1: OtherCheck
     * 
     * 对于 Sicily:
     * 0: CompileCheck
     * 1: StandardCheck
     * 
     * 总之，0 必定是编译任务，其他必定是评测任务。
     */
    uint8_t type;
};

}  // namespace judge::message
