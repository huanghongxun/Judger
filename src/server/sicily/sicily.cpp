#include "server/sicily/sicily.hpp"
#include <fmt/printf.h>
#include <glog/logging.h>
#include <boost/assign/list_of.hpp>
#include <fstream>
#include <tuple>
#include "common/io_utils.hpp"
#include "common/status.hpp"

namespace judge::server::sicily {
using namespace std;
using namespace nlohmann;
using namespace ormpp;

// clang-format off
const unordered_map<status, const char *> status_string = boost::assign::map_list_of
    (status::PENDING, "Waiting")
    (status::RUNNING, "Judging")
    (status::ACCEPTED, "Accepted")
    (status::PARTIAL_CORRECT, "Wrong Answer")
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
    string host = config.at("host").get<string>();
    string user = config.at("user").get<string>();
    string password = config.at("password").get<string>();
    string database = config.at("database").get<string>();

    db.connect(host.c_str(), user.c_str(), password.c_str(), database.c_str());
}

void configuration::summarize_invalid(submission &submit) {
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
        kase.check_script = "compile";
        kase.is_random = false;
        kase.score = 0;
        kase.check_type = 0;
        kase.testcase_id = 0;
        kase.depends_on = -1;
        submit.test_cases.push_back(kase);
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
            kase.is_random = false;
            kase.score = 0;
            kase.check_type = 1;
            kase.testcase_id = i;
            kase.depends_on = i;  // 当前的 kase 是第 i + 1 组测试点
            kase.depends_cond = test_check::depends_condition::ACCEPTED;
            submit.test_cases.push_back(kase);
        }
    }

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

            submit.contest_start_time = mktime(&mytm);
            submit.contest_end_time = duration * 3600 + submit.contest_start_time;
        }
    }

    auto probrows = sicily.db.query<std::tuple<int, int, int, int>>(
        "SELECT time_limit, memory_limit, special_judge, has_framework FROM problems WHERE pid=?",
        submit.prob_id);
    if (!probrows.empty()) {
        int spj, has_framework;
        tie(submit.time_limit, submit.memory_limit, spj, has_framework) = probrows[0];

        if (spj) {
            // 由于 Sicily 的 special judge 的比较格式和本评测的不一致，因此我们需要借助 bash 脚本进行转换
            unique_ptr<source_code> spj = make_unique<source_code>(sicily.exec_mgr);
            spj->language = "bash";
            spj->source_files.push_back(make_unique<local_asset>("run", EXEC_DIR / "sicily_spjudge.sh"));
            spj->assist_files.push_back(make_unique<local_asset>("spjudge", get_data_path(sicily.testdata, submit.prob_id, "spjudge")));
            submit.compare = move(spj);
        } else {
            submit.compare = sicily.exec_mgr.get_compare_script("diff-all");
        }

        if (has_framework) {
            prog->source_files.clear();
            prog->assist_files.push_back(make_unique<text_asset>("source.cpp", sourcecode));
            prog->source_files.push_back(
                make_unique<local_asset>("framework.cpp", get_data_path(sicily.testdata, submit.prob_id, "framework.cpp")));
        }
    }

    submit.submission = move(prog);

    return true;
}

static void set_compilelog(configuration &sicily, const string &log, const submission &submit) {
    sicily.db.query<std::tuple<>>(
        "UPDATE status set compilelog=? where sid=?",
        log, submit.sub_id);
}

static void update_user(configuration &sicily, bool solved, const submission &submit) {
    auto rows = sicily.db.query<std::tuple<>>(
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
        sicily.db.query<std::tuple<>>(
            "UPDATE problems SET accepted = accepted + 1 WHERE pid=?",
            submit.prob_id);
    }

    if (!submit.contest_id.empty()) {
        LOG(INFO) << "Contest " << submit.contest_id << " submission";

        auto rows = sicily.db.query<std::tuple<int, int, int>>(
            "SELECT accepted, ac_time, submissions FROM ranklist WHERE uid=? AND cid=? AND pid=?",
            submit.user_id, submit.contest_id, submit.contest_prob_id);

        if (rows.empty()) {
            LOG(INFO) << "No submissions";

            sicily.db.query<std::tuple<>>(
                "INSERT INTO ranklist (uid, cid, pid, accepted, submissions, ac_time) VALUES (?, ?, ?, ?, 1, ?)",
                submit.user_id, submit.contest_id, submit.contest_prob_id, solved, (submit.submit_time - submit.contest_start_time) / 60 + 1);
        } else {
            int accepted, ac_time, submissions;
            tie(accepted, ac_time, submissions) = rows[0];
            if (accepted == 0) {
                LOG(INFO) << "Update Non-ac Submissions";

                sicily.db.query<std::tuple<>>(
                    "UPDATE ranklist SET accepted=?,ac_time=?,submissions=? WHERE uid=? AND cid=? AND pid=?",
                    solved, (submit.submit_time - submit.contest_start_time) / 60 + 1, submissions + 1, submit.user_id, submit.contest_id, submit.contest_prob_id);
            } else if (accepted == 1 && solved && ac_time > (submit.submit_time - submit.contest_start_time) / 60 + 1) {  // rejudge
                LOG(INFO) << "Rejudge";

                sicily.db.query<std::tuple<>>(
                    "DELETE FROM ranklist WHERE uid=? AND cid=? AND pid=? AND ac_time>?",
                    submit.user_id, submit.contest_id, submit.contest_prob_id, (submit.submit_time - submit.contest_start_time) / 60 + 1);

                sicily.db.query<std::tuple<>>(
                    "INSERT INTO ranklist (uid, cid, pid, accepted, submissions, ac_time) VALUES (?, ?, ?, ?, 1, ?)",
                    submit.user_id, submit.contest_id, submit.contest_prob_id, solved, (submit.submit_time - submit.contest_start_time) / 60 + 1);

                sicily.db.query<std::tuple<>>(
                    "UPDATE problems SET accepted=accepted+1 WHERE cid=? AND pid=?", submit.contest_id, submit.contest_prob_id);
            }
        }
    }
}

static void set_status(configuration &sicily, const judge::message::task_result &task_result, int current_case, const submission &submit) {
    static set<status> final_status = {status::COMPILING, status::RUNNING, status::ACCEPTED, status::PRESENTATION_ERROR,
                                       status::WRONG_ANSWER, status::TIME_LIMIT_EXCEEDED, status::MEMORY_LIMIT_EXCEEDED,
                                       status::RUNTIME_ERROR, status::SEGMENTATION_FAULT, status::FLOATING_POINT_ERROR};
    string runtime, memory;

    bool is_multiple_cases = submit.test_data.size() > 1;
    if (final_status.count(task_result.status)) {
        sicily.db.query<std::tuple<>>(
            "UPDATE status SET status=?, failcase=?, run_time=?, run_memory=? WHERE sid=?",
            status_string.at(task_result.status),
            is_multiple_cases ? current_case : -1,
            task_result.run_time,
            task_result.memory_used >> 10,
            submit.sub_id);
    } else {
        sicily.db.query<std::tuple<>>(
            "UPDATE status SET status=?, failcase=? WHERE sid=?",
            status_string.at(task_result.status),
            is_multiple_cases ? current_case : -1,
            submit.sub_id);
    }
    LOG(INFO) << "Judge: " << status_string.at(task_result.status) << ' ' << current_case;
}

static void popup_queue(configuration &sicily, submission &submit) {
    sicily.db.query<std::tuple<>>(
        "DELETE FROM queue WHERE qid=?",
        submit.queue_id);
}

bool configuration::fetch_submission(submission &submit) {
    return fetch_queue(*this, submit);
}

void configuration::summarize(submission &submit, const vector<judge::message::task_result> &task_results) {
    popup_queue(*this, submit);

    if (task_results[0].status != judge::status::ACCEPTED) {
        set_status(*this, task_results[0], 0, submit);
        set_compilelog(*this, task_results[0].error_log, submit);
        update_user(*this, false, submit);
    } else {
        judge::message::task_result final_result;
        final_result.status = judge::status::ACCEPTED;
        final_result.run_time = 0;     // Sicily 需要计算总的运行时间
        final_result.memory_used = 0;  // 所有测试点的最大运行内存使用量
        for (size_t i = 1; i < task_results.size(); ++i) {
            final_result.run_time += task_results[i].run_time;
            final_result.memory_used = max(final_result.memory_used, task_results[i].memory_used);
            if (task_results[i].status != judge::status::ACCEPTED) {
                set_status(*this, task_results[i], i - 1, submit);
                update_user(*this, false, submit);
                return;
            }
        }

        set_status(*this, final_result, submit.test_data.size(), submit);
        update_user(*this, true, submit);
    }
}

}  // namespace judge::server::sicily