#include "server/moj/choice.hpp"

namespace judge::server::moj {
using namespace std;

/**
 * {
 *   "version": 2,
 *   "sub_id": 1,
 *   "prob_id": 1,
 *   "standard": [
 *     {
 *       "grading": {
 *         "max_grade": 5,
 *         "half_grade": 2.5
 *       },
 *       "type": "multi",
 *       "answer": [1, 2]
 *     },
 *     {
 *       "grading": {
 *         "max_grade": 5,
 *         "half_grade": 2.5
 *       },
 *       "type": "single",
 *       "answer": [2]
 *     }
 *   ],
 *   "answer": [
 *     [1, 2],
 *     [2]
 *   ]
 * }
 */
void from_json(const json &j, choice_exam &value) {
    if (j.count("version"))
        j.at("version").get_to(value.version);
    else
        value.version = 1;

    j.at("sub_id").get_to(value.sub_id);
    j.at("prob_id").get_to(value.prob_id);

    auto &standard = j.at("standard");
    auto &answer = j.at("answer");
    for (json::const_iterator qit = standard.begin(), ait = answer.begin(); qit != standard.end() && ait != answer.end(); ++qit, ++ait) {
        choice_question q;

        const json &qobj = *qit;
        const json &aobj = *ait;

        qobj.at("grading").at("max_grade").get_to(q.full_grade);
        if (qobj.at("grading").count("half_grade"))
            qobj.at("grading").at("half_grade").get_to(q.half_grade);
        qobj.at("type").get_to(q.type);
        qobj.at("answer").get_to(q.standard_answer);
        aobj.get_to(q.student_answer);
    }
}

void to_json(json &j, const choice_report &report) {
    j = {{"grade", report.grade},
         {"full_grade", report.full_grade},
         {"report", report.question_grades}};
}

static float judge_choice_question(const choice_question &q) {
    unsigned long std = 0, ans = 0;
    for (auto &choice : q.student_answer)
        ans |= 1 << choice;
    for (auto &choice : q.standard_answer)
        std |= 1 << choice;
    if (ans == std)
        return q.full_grade;
    else if ((ans & std) == ans)
        return q.half_grade;
    else
        return 0;
}

choice_report judge_choice_exam(const choice_exam &exam) {
    choice_report report;
    for (auto &q : exam.questions) {
        float score = judge_choice_question(q);
        report.grade += score;
        report.full_grade += q.full_grade;
        report.question_grades.push_back(score);
    }
}

}  // namespace judge::server::moj
