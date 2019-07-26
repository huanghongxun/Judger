#pragma once

#include <vector>
#include "judge/judger.hpp"

namespace judge {

/**
 * @brief 表示一道选择题
 */
struct choice_question {
    /**
     * @brief 表示当前是单选题还是多选题。
     * 可选值：single、multi。
     */
    std::string type;

    /**
     * @brief 该选择题实际得分
     */
    float grade;

    /**
     * @brief 该选择题满分是多少
     * 选手答案和标准答案一致则可以得到满分。
     */
    float full_grade;

    /**
     * @brief 该选择题部分分是多少
     * 对于多选题，选手选了至少一个选项，且未选错项时可以得到部分分。
     */
    float half_grade = 0;

    /**
     * @brief 选手选择的选项
     */
    std::vector<int> student_answer;

    /**
     * @brief 标准选项
     */
    std::vector<int> standard_answer;
};

/**
 * @brief 表示一个学生选择题提交
 */
struct choice_submission : public submission {
    /**
     * @brief 该选择题提交的所有选择题、标答、学生作答
     */
    std::vector<choice_question> questions;
};

/**
 * @brief 评测选择题的 Judger 类，选择题评测的逻辑都在这个类里
 */
struct choice_judger : public judger {
    std::string type() const override;

    bool verify(submission &submit) const override;

    bool distribute(concurrent_queue<message::client_task> &task_queue, submission &submit) const override;

    void judge(const message::client_task &task, concurrent_queue<message::client_task> &task_queue, const std::string &execcpuset) const override;
};

}  // namespace judge
