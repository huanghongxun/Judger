#pragma once

#include <any>
#include <boost/rational.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include "program.hpp"

/**
 * 这个头文件包含提交信息
 * 包含：
 * 1. submission 类（表示一个提交）
 * 2. test_case_data 类（表示一个标准测试数据组）
 * 3. judge_task 类（表示一个测试点）
 */
namespace judge {
using namespace std;
using namespace nlohmann;

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
    string category;

    /**
     * @brief 选手提交类型
     * 如：programming, choice, program_blank_filling, output
     * 分别对应 programming_submission, choice_submission, program_blank_filling_submission, output_submission
     */
    string sub_type;

    /**
     * @brief 选手代码提交的 id
     * string 可以兼容一切情况
     */
    string sub_id;

    /**
     * @brief 选手代码提交的题目 id
     * string 可以兼容一切情况
     */
    string prob_id;

    /**
     * @brief 题目最后更新时间
     * 根据最后更新时间来确定是否需要重新编译随机数据生成器、标程、生成随机测试数据
     */
    int updated_at;

    /**
     * @brief 队列 id
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    string queue_id;

    /**
     * @brief 选手用户 id
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    string user_id;

    /**
     * @brief 题目所属比赛 id，如果不存在比赛则为空
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    string contest_id;

    /**
     * @brief 题目所属比赛的题目 id，如果不存在比赛则为空
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    string contest_prob_id;

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
    any envelope;

    /**
     * @brief 给 judge_server 保存题目配置的地方
     */
    any config;
};

}  // namespace judge
