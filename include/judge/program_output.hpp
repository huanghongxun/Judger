#pragma once

#include "common/json_utils.hpp"
#include <string>
#include <vector>
#include "judge/judger.hpp"

namespace judge {

/**
 * @brief 表示一道程序输出题
 */
struct program_output_question {
    /**
     * @brief 该题实际得分
     */
    double grade;

    /**
     * @brief 该程序输出题满分是多少
     * 选手答案和标准答案一致则可以得到满分。
     * 否则根据编辑距离扣分
     */
    double full_grade;

    /**
     * @brief 选手答案
     */
    std::string student_answer;

    /**
     * @brief 标准答案
     */
    std::string standard_answer;
};

/**
 * @brief 表示一个学生程序输出题提交
 */
struct program_output_submission : public submission {
    /**
     * @brief 该程序输出题提交的所有程序输出题、标答、学生作答
     */
    std::vector<program_output_question> questions;
};

/**
 * @brief 评测程序输出题的 Judger 类，程序输出题评测的逻辑都在这个类里
 */
struct program_output_judger : public judger {
    std::string type() const override;

    bool verify(submission &submit) const override;

    bool distribute(concurrent_queue<message::client_task> &task_queue, submission &submit) const override;

    void judge(const message::client_task &task, concurrent_queue<message::client_task> &task_queue, const std::string &execcpuset) const override;
};

}  // namespace judge
