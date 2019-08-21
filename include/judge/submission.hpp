#pragma once

#include <any>
#include <ctime>
#include <mutex>
#include <string>
#include "common/utils.hpp"

namespace judge {

namespace server {
struct judge_server;
}

/**
 * @brief 一个选手提交，可能是编程题、选择题、完善程序题、程序输出题
 */
struct submission {
    virtual ~submission();

    unsigned judge_id;

    /**
     * @brief 选手代码归属的程序
     * 如：moj, mcourse, sicily 来区分评测结果应该发送给哪个服务器
     */
    std::string category;

    server::judge_server *judge_server;

    /**
     * @brief 选手提交类型
     * 如：programming, choice, program_blank_filling, output
     * 分别对应 programming_submission, choice_submission, program_blank_filling_submission, output_submission
     */
    std::string sub_type;

    /**
     * @brief 选手代码提交的 id
     * string 可以兼容一切情况
     */
    std::string sub_id;

    /**
     * @brief 选手代码提交的题目 id
     * string 可以兼容一切情况
     */
    std::string prob_id;

    /**
     * @brief 题目最后更新时间（从 1970 年 1 月 1 日开始的时间戳）
     * 根据最后更新时间来确定是否需要重新编译随机数据生成器、标程、生成随机测试数据
     */
    time_t updated_at;

    /**
     * @brief 队列 id
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    std::string queue_id;

    /**
     * @brief 选手用户 id
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    std::string user_id;

    /**
     * @brief 题目所属比赛 id，如果不存在比赛则为空
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    std::string contest_id;

    /**
     * @brief 题目所属比赛的题目 id，如果不存在比赛则为空
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    std::string contest_prob_id;

    /**
     * @brief 评测系统获取到的提交时间
     */
    elapsed_time judge_time;

    /**
     * @brief 提交时间
     */
    time_t submit_time;

    /**
     * @brief 比赛开始时间
     */
    time_t contest_start_time;

    /**
     * @brief 比赛结束时间
     */
    time_t contest_end_time;

    /**
     * @brief 给 judge_server 保存消息队列信封的地方
     */
    std::any envelope;

    /**
     * @brief 给 judge_server 保存题目配置的地方
     */
    std::any config;

    std::mutex mut;
};

template <typename T>
T &operator<<(T &os, const submission &submit) {
    os << "Submission[" << submit.sub_type << ":" << submit.category << "-" << submit.prob_id << "-" << submit.sub_id << "]";
    return os;
}

}  // namespace judge
