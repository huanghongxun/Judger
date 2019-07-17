#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;

struct choice_question {
    /**
     * @brief 表示当前是单选题还是多选题。
     * 可选值：single、multi。
     */
    string type;

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

struct choice_exam {
    /**
     * @brief 选择题配置版本
     * 仅支持 version 2
     */
    int version;

    /**
     * @brief 选择题提交 id
     */
    int sub_id;

    /**
     * @brief 选择题库 id
     */
    int prob_id;

    /**
     * @brief 选择题数量
     */
    size_t number;

    /**
     * @brief 选择题类型、评分、标准答案、选手答案集合
     */
    vector<choice_question> questions;
};

void from_json(const json &j, choice_exam &value);

/**
 * @brief 选择题报告
 * 
 * @code{json}
 * {
 *   "grade": 10,
 *   "report": [2, 2, 2, 2, 2]
 * }
 * @endcode
 */
struct choice_report {
    /**
     * @brief 选择题得分
     */
    float grade = 0;

    /**
     * @brief 选择题总分
     */
    float full_grade = 0;

    /**
     * @brief 每个选择题的得分情况
     * 比如 [2, 2, 2, 2, 2] 表示 5 道选择题都拿了 2 分
     */
    vector<float> question_grades;
};

void to_json(json &j, const choice_report &report);

/**
 * @brief 评测选择题并返回报告
 * @param exam 选择题提交配置和答案
 * @return 选择题评测结果，包括各题分数和总分
 */
choice_report judge_choice_exam(const choice_exam &exam);

}  // namespace judge::server::moj
