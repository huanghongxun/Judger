#include "judge/program_output.hpp"
#include <glog/logging.h>
#include <boost/algorithm/string.hpp>
#include "server/judge_server.hpp"

namespace judge {
using namespace std;

//预处理字符串，将字符串中的空格及换行去掉
static string string_preprocess(const string &s) {
    string result;
    remove_copy_if(s.begin(), s.end(), back_inserter(result), boost::is_any_of(" \n"));
    return result;
}

/**
 * @brief 计算两个字符串的编辑距离
 */
static unsigned calc_edit_distance(const string &s1, const string &s2) {
    // 避免字符串过大导致评测被卡死（无论是时间上还是空间上）的问题
    if (s1.length() > 5000 || s2.length() > 5000) return 0;
    size_t len1 = s1.size(), len2 = s2.size();
    size_t d[len1 + 1][len2 + 2];  // 动态大小的数组会被分配进堆内存
    d[0][0] = 0;
    for (size_t i = 1; i <= len1; ++i) d[i][0] = i;
    for (size_t i = 1; i <= len2; ++i) d[0][i] = i;

    for (size_t i = 1; i <= len1; ++i)
        for (size_t j = 1; j <= len2; ++j)
            d[i][j] = min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (s1[i - 1] == s2[j - 1] ? 0 : 1)});
    return d[len1][len2];
}

/**
 * @brief 根据编辑距离计算两个字符串之间的相似度(0~1)
 */
static double calc_similarity(const string &standard, const string &student) {
    return 1.0 - calc_edit_distance(standard, student) * 1.0 / max(standard.size(), student.size());
}

string program_output_judger::type() const {
    return "output";
}

bool program_output_judger::verify(submission &submit) const {
    auto sub = dynamic_cast<program_output_submission *>(&submit);
    if (!sub) return false;
    LOG(INFO) << "Judging program output submission [" << sub->category << "-" << sub->prob_id << "-" << sub->sub_id << "]";

    return true;
}

bool program_output_judger::distribute(concurrent_queue<message::client_task> &task_queue, submission &submit) const {
    // 我们只需要发一个评测请求就行了，以便让 client 能调用我们的 judge 函数
    // 或者我们在 verify 的时候就评测完选择题然后返回 false 也行。
    judge::message::client_task client_task = {
        .submit = &submit,
        .id = 0};
    task_queue.push(client_task);
    return true;
}

void program_output_judger::judge(const message::client_task &task, concurrent_queue<message::client_task> &, const string &) const {
    auto submit = dynamic_cast<program_output_submission *>(task.submit);

    for (auto &q : submit->questions)
        q.grade = q.full_grade * calc_similarity(string_preprocess(q.standard_answer), string_preprocess(q.student_answer));

    submit->judge_server->summarize(*submit);
    fire_judge_finished(*submit);
}

}  // namespace judge
