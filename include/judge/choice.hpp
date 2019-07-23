#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "judge/judger.hpp"

namespace judge {
using namespace std;
using namespace nlohmann;

struct choice_question {
    /**
     * @brief 表示当前是单选题还是多选题。
     * 可选值：single、multi。
     */
    string type;

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
    vector<int> student_answer;

    /**
     * @brief 标准选项
     */
    vector<int> standard_answer;
};

struct choice_submission : public submission {
    /**
     * @brief 该选择题提交的所有选择题、标答、学生作答
     */
    vector<choice_question> questions;
};

struct choice_judger : public judger {
    string type() override;

    bool verify(unique_ptr<submission> &submit) override;

    bool distribute(concurrent_queue<message::client_task> &task_queue, unique_ptr<submission> &submit) override;

    void judge(const message::client_task &task, concurrent_queue<message::client_task> &task_queue, const string &execcpuset) override;
};

}  // namespace judge
