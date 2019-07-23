#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "judge/judger.hpp"

namespace judge {
using namespace std;
using namespace nlohmann;

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
    string student_answer;

    /**
     * @brief 标准答案
     */
    string standard_answer;
};

struct program_output_submission : public submission {
    /**
     * @brief 该选择题提交的所有选择题、标答、学生作答
     */
    vector<program_output_question> questions;
};

struct program_output_judger : public judger {
    string type() override;

    bool verify(unique_ptr<submission> &submit) override;

    bool distribute(concurrent_queue<message::client_task> &task_queue, unique_ptr<submission> &submit) override;

    void judge(const message::client_task &task, concurrent_queue<message::client_task> &task_queue, const string &execcpuset) override;
};

}  // namespace judge
