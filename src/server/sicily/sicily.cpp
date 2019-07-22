#include "server/sicily/sicily.hpp"
#include <fmt/printf.h>
#include <glog/logging.h>
#include <boost/assign.hpp>
#include <fstream>
#include <tuple>
#include "common/io_utils.hpp"
#include "common/status.hpp"
#include "server/common/config.hpp"
using namespace boost::assign;

namespace judge::server::sicily {
using namespace std;
using namespace nlohmann;
using namespace ormpp;

// clang-format off
const unordered_map<status, const char *> status_string = boost::assign::map_list_of
    (status::PENDING, "Waiting")
    (status::RUNNING, "Judging")
    (status::ACCEPTED, "Accepted")
    (status::PARTIAL_CORRECT, "Wrong Answer") // Sicily 不支持部分分
    (status::COMPILATION_ERROR, "Compile Error")
    (status::EXECUTABLE_COMPILATION_ERROR, "Other") // 假定我们默认提供的 executable 是正确的，因此不存在这个 status
    (status::WRONG_ANSWER, "Wrong Answer")
    (status::RUNTIME_ERROR, "Runtime Error")
    (status::TIME_LIMIT_EXCEEDED, "Time Limit Exceeded")
    (status::MEMORY_LIMIT_EXCEEDED, "Memory Limit Exceeded")
    (status::OUTPUT_LIMIT_EXCEEDED, "Output Limit Exceeded")
    (status::PRESENTATION_ERROR, "Presentation Error")
    (status::RESTRICT_FUNCTION, "Restrict Function")
    (status::OUT_OF_CONTEST_TIME, "Out of Contest Time")
    (status::COMPILING, "Judging")
    (status::SEGMENTATION_FAULT, "Runtime Error")
    (status::FLOATING_POINT_ERROR, "Runtime Error")
    (status::RANDOM_GEN_ERROR, "Other") // Sicily 评测不包含此项
    (status::COMPARE_ERROR, "Wrong Answer") // Sicily 评测若遇到 spj 崩溃则认为评测结果是 WA
    (status::SYSTEM_ERROR, "Other");
// clang-format on

configuration::configuration()
    : exec_mgr(CACHE_DIR, EXEC_DIR) {}

string configuration::category() const {
    return "sicily";
}

const executable_manager &configuration::get_executable_manager() const {
    return exec_mgr;
}

void configuration::init(const filesystem::path &config_path) {
    if (!filesystem::exists(config_path))
        throw runtime_error("Unable to find configuration file");
    ifstream fin(config_path);
    json config;
    fin >> config;
    string testdata = config.at("data-dir").get<string>();
    this->testdata = filesystem::path(testdata);
    // 设置数据库连接信息
    database dbcfg = config;
    db.connect(dbcfg.host.c_str(), dbcfg.user.c_str(), dbcfg.password.c_str(), dbcfg.database.c_str());
}

void configuration::summarize_invalid(submission &) {
    throw runtime_error("Invalid submission");
}

static filesystem::path get_data_path(const filesystem::path &testdata, const string &prob_id, const string &filename) {
    return testdata / prob_id / filename;
}

static bool fetch_queue(configuration &sicily, submission &submit) {
    submit.category = sicily.category();

    string time, sourcecode;

    auto rows = sicily.db.query<std::tuple<string, string, string, string, string, string, string, string, string>>(
        "SELECT qid, queue.sid, uid, pid, language, time, cid, cpid, sourcecode FROM queue, status WHERE queue.sid=status.sid AND hold=0 AND server_id=? ORDER BY qid LIMIT 1",
        0);

    if (rows.size() == 0) {
        return false;
    }

    tie(submit.queue_id, submit.sub_id, submit.user_id, submit.prob_id, submit.language, time, submit.contest_id, submit.contest_prob_id, sourcecode) = rows[0];

    struct tm mytm;

    if (!time.empty()) {
        strptime(time.c_str(), "%Y-%m-%d %H:%M:%S", &mytm);

        submit.submit_time = mktime(&mytm);
    } else {
        submit.submit_time = 0;
    }

    unique_ptr<source_code> prog = make_unique<source_code>(sicily.exec_mgr);
    prog->language = submit.language;
    prog->source_files.push_back(make_unique<text_asset>("source.cpp", sourcecode));

    {
        // 编译任务
        test_check kase;
        kase.is_random = false;
        kase.score = 0;
        kase.check_script = message::client_task::COMPILE_TYPE;
        kase.testcase_id = 0;
        kase.depends_on = -1;  // 编译任务没有依赖，因此可以先执行
        submit.test_cases.push_back(kase);
    }

    // Sicily 评测需要处理比赛信息
    if (!submit.contest_id.empty()) {
        string starttime;
        int duration;
        auto rows = sicily.db.query<std::tuple<string, int>>(
            "SELECT starttime, during FROM contests WHERE cid=?",
            submit.contest_id);

        if (!rows.empty()) {
            tie(starttime, duration) = rows[0];
            struct tm mytm;

            strptime(starttime.c_str(), "%Y-%m-%d %T", &mytm);

            // 收集当前提交所属比赛的信息，用来判断是否 Out of Contest Time
            submit.contest_start_time = mktime(&mytm);
            submit.contest_end_time = duration * 3600 + submit.contest_start_time;
        }
    }

    auto probrows = sicily.db.query<std::tuple<int, int, int, int>>(
        "SELECT time_limit, memory_limit, special_judge, has_framework FROM problems WHERE pid=?",
        submit.prob_id);
    if (!probrows.empty()) {
        int spj, has_framework;
        double time_limit;
        int memory_limit;
        tie(time_limit, memory_limit, spj, has_framework) = probrows[0];
        time_limit /= 1000;
        int proc_limit = -1;
        int file_limit = 32768;  // 32M

        if (spj) {
            // 由于 Sicily 的 special judge 的比较格式和本评测的不一致，因此我们需要借助 bash 脚本进行转换
            unique_ptr<source_code> spj = make_unique<source_code>(sicily.exec_mgr);
            spj->language = "bash";  // bash 语言不需要执行编译步骤，实际上 bash 语言允许任何其他编译语言的执行
            spj->source_files.push_back(make_unique<local_asset>("run", EXEC_DIR / "sicily_spjudge.sh"));
            spj->assist_files.push_back(make_unique<local_asset>("spjudge", get_data_path(sicily.testdata, submit.prob_id, "spjudge")));
            submit.compare = move(spj);
        } else {
            // Sicily 默认使用完全比较，格式错误时返回 Presentation Error
            submit.compare = sicily.exec_mgr.get_compare_script("diff-all");
        }

        if (has_framework) {
            // framework 提交：
            // framework.cpp 中包含 #include "source.cpp" 的形式来支持添加隐藏代码，比如不提供 main 函数的具体实现
            // 这种情况下，source.cpp 不能参与编译，否则会发生链接错误，因此 source.cpp 被加入 assist_files
            // 我们只编译 framework.cpp 即可
            prog->source_files.clear();
            prog->assist_files.push_back(make_unique<text_asset>("source.cpp", sourcecode));
            prog->source_files.push_back(
                make_unique<local_asset>("framework.cpp", get_data_path(sicily.testdata, submit.prob_id, "framework.cpp")));
        }

        {
            // test_data
            filesystem::path case_dir = sicily.testdata / submit.prob_id;
            ifstream fin(case_dir / ".DIR");
            string stdin, stdout;
            for (unsigned i = 0; fin >> stdin >> stdout; ++i) {
                // 注册标准测试数据
                test_case_data data;
                data.inputs.push_back(make_unique<local_asset>("testdata.in", case_dir / stdin));
                data.outputs.push_back(make_unique<local_asset>("testdata.out", case_dir / stdout));
                submit.test_data.push_back(move(data));

                // 添加评测任务，sicily 只有标准测试，因此为每个测试数据添加一个标准测试数据组
                test_check kase;
                kase.check_script = "standard";
                kase.run_script = "standard";
                kase.is_random = false;
                kase.score = 0;
                kase.check_type = 1;
                kase.testcase_id = i;
                kase.depends_on = i;  // 当前的 kase 是第 i + 1 组测试点，依赖第 i 组测试点，最开始的测试数据将依赖编译
                kase.depends_cond = test_check::depends_condition::ACCEPTED;
                kase.time_limit = time_limit;
                kase.memory_limit = memory_limit;
                kase.file_limit = file_limit;
                kase.proc_limit = proc_limit;
                submit.test_cases.push_back(kase);
            }
        }
    }

    submit.submission = move(prog);
    submit.tag = judge::message::task_result(0, 0, 0);

    return true;
}

static void set_compilelog(configuration &sicily, const string &log, const submission &submit) {
    sicily.db.execute("UPDATE status set compilelog=? where sid=?",
                      log, submit.sub_id);
}

static void update_user(configuration &sicily, bool compilation_error, bool solved, const submission &submit) {
    auto rows = sicily.db.query<tuple<int>>(
        "SELECT sid FROM status WHERE pid=? AND uid=? AND status.status = 'Accepted' and sid != ?",
        submit.prob_id, submit.user_id, submit.sub_id);
    LOG(INFO) << "update status pid=" << submit.prob_id << " uid=" << submit.user_id << " sid=" << submit.sub_id << " solved=" << solved;

    if (rows.empty() && solved) {
        auto rows = sicily.db.query<std::tuple<int>>(
            "SELECT 0 FROM stock_problems WHERE pid=?",
            submit.prob_id);
        if (!rows.empty()) {
            auto rows = sicily.db.query<std::tuple<int>>(
                "UPDATE user SET stock_solved=stock_solved+1 WHERE uid=?",
                submit.user_id);
        }
    }

    if (solved) {
        sicily.db.execute("UPDATE problems SET accepted = accepted + 1 WHERE pid=?",
                          submit.prob_id);
    }

    if (!submit.contest_id.empty()) {
        LOG(INFO) << "Contest " << submit.contest_id << " submission";

        auto rows = sicily.db.query<std::tuple<int, int, int>>(
            "SELECT accepted, ac_time, submissions FROM ranklist WHERE uid=? AND cid=? AND pid=?",
            submit.user_id, submit.contest_id, submit.contest_prob_id);

        if (rows.empty()) {
            LOG(INFO) << "No submissions";

            sicily.db.execute(
                "INSERT INTO ranklist (uid, cid, pid, accepted, submissions, ac_time) VALUES (?, ?, ?, ?, ?, ?)",
                submit.user_id, submit.contest_id, submit.contest_prob_id, (int)solved, compilation_error ? 0 : 1, (submit.submit_time - submit.contest_start_time) / 60 + 1);
        } else {
            int accepted, ac_time, submissions;
            tie(accepted, ac_time, submissions) = rows[0];
            if (accepted == 0) {
                LOG(INFO) << "Update Non-ac Submissions";

                sicily.db.execute(
                    "UPDATE ranklist SET accepted=?, ac_time=?,submissions=? WHERE uid=? AND cid=? AND pid=?",
                    (int)solved, (submit.submit_time - submit.contest_start_time) / 60 + 1, submissions + (compilation_error ? 0 : 1), submit.user_id, submit.contest_id, submit.contest_prob_id);
            } else if (accepted == 1 && solved && ac_time > (submit.submit_time - submit.contest_start_time) / 60 + 1) {  // rejudge
                LOG(INFO) << "Rejudge";

                sicily.db.execute(
                    "DELETE FROM ranklist WHERE uid=? AND cid=? AND pid=? AND ac_time>?",
                    submit.user_id, submit.contest_id, submit.contest_prob_id, (submit.submit_time - submit.contest_start_time) / 60 + 1);

                sicily.db.execute(
                    "INSERT INTO ranklist (uid, cid, pid, accepted, submissions, ac_time) VALUES (?, ?, ?, ?, ?, ?)",
                    submit.user_id, submit.contest_id, submit.contest_prob_id, (int)solved, compilation_error ? 0 : 1, (submit.submit_time - submit.contest_start_time) / 60 + 1);

                sicily.db.execute(
                    "UPDATE problems SET accepted=accepted+1 WHERE cid=? AND pid=?",
                    submit.contest_id, submit.contest_prob_id);
            }
        }
    }
}

/**
 * @brief 更新提交的指定测试点的评测状态
 * @param sicily Sicily 服务器配置信息
 * @param task_result 当前测试点的评测结果
 * @param current_case 当前测试点编号
 */
static void set_status(configuration &sicily, const judge::message::task_result &task_result, int current_case, const submission &submit) {
    // final_status 保存可以统计运行时间和运行内存占用的评测结果状态
    static set<status> final_status = {status::COMPILING, status::RUNNING, status::ACCEPTED, status::PRESENTATION_ERROR,
                                       status::WRONG_ANSWER, status::TIME_LIMIT_EXCEEDED, status::MEMORY_LIMIT_EXCEEDED,
                                       status::RUNTIME_ERROR, status::SEGMENTATION_FAULT, status::FLOATING_POINT_ERROR};
    string runtime, memory;

    bool is_multiple_cases = submit.test_data.size() > 1;
    if (final_status.count(task_result.status)) {
        sicily.db.execute(
            "UPDATE status SET status=?, failcase=?, run_time=?, run_memory=? WHERE sid=?",
            status_string.at(task_result.status),
            is_multiple_cases ? current_case : -1,
            task_result.run_time,
            task_result.memory_used >> 10,  // Sicily 的内存占用单位是 KB
            submit.sub_id);
    } else {
        sicily.db.execute(
            "UPDATE status SET status=?, failcase=? WHERE sid=?",
            status_string.at(task_result.status),
            is_multiple_cases ? current_case : -1,
            submit.sub_id);
    }
    LOG(INFO) << "Judge: " << status_string.at(task_result.status) << ' ' << current_case;
}

static void popup_queue(configuration &sicily, submission &submit) {
    sicily.db.execute("DELETE FROM queue WHERE qid=?",
                      submit.queue_id);
}

bool configuration::fetch_submission(submission &submit) {
    return fetch_queue(*this, submit);
}

void configuration::summarize(submission &submit, size_t completed, const vector<judge::message::task_result> &task_results) {
    if (completed < 1 || completed > task_results.size()) return;

    if (completed == task_results.size())
        popup_queue(*this, submit);
    set_compilelog(*this, task_results[0].error_log, submit);

    judge::message::task_result current = task_results[completed - 1];
    auto final_result = any_cast<judge::message::task_result>(submit.tag);

    if (completed == 1) {
        if (current.status != judge::status::ACCEPTED) {
            // 先检查是否存在编译错误的情况
            set_status(*this, task_results[0], 0, submit);
            update_user(*this, /* compilation_error */ true, /* solved */ false, submit);
            return;
        }
    } else {
        final_result.run_time += current.run_time;
        final_result.memory_used = max(final_result.memory_used, current.memory_used);
    }

    submit.tag = final_result;

    // 如果选手程序在当前测试点失败，则直接返回提交结果。
    // 根据我们构造的 submission，之后一定不会再调用 summarize 函数
    if (current.status != judge::status::ACCEPTED) {
        // completed 同时包含 1 组编译测试和一些标准测试
        // set_status 要求传标准测试的标号，那么就是 completed - 1(一组编译测试) - 1(标准测试点编号从 0 开始)
        set_status(*this, current, completed - 2, submit);
        update_user(*this, /* compilation error */ false, /* solved */ false, submit);
        return;
    }

    // 所有测试数据都通过了测试，此时我们返回 Accepted
    // 对于可能没有标准测试数据的题目，completed == 1 满足之后会到这里返回 Accepted
    if (completed == task_results.size()) {
        set_status(*this, final_result, submit.test_data.size(), submit);
        update_user(*this, /* compilation_error */ false, /* solved */ true, submit);
    }
}

}  // namespace judge::server::sicily
