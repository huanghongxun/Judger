#include "server/sicily/sicily.hpp"
#include <fmt/printf.h>
#include <glog/logging.h>
#include <fstream>
#include <tuple>
#include "server/status.hpp"
#include "utils.hpp"

namespace judge::server::sicily {
using namespace std;
using namespace nlohmann;
using namespace ormpp;

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

    db.connect(host, user, password, database);
}

bool configuration::fetch_submission(submission &submit) {
    return fetch_queue(*this, submit);
}

static filesystem::path get_data_path(const filesystem::path &testdata, const string &prob_id, const string &filename) {
    return testdata / prob_id / filename;
}

bool fetch_queue(configuration &sicily, submission &submit) {
    submit.category = "sicily";

    int i;
    unsigned long qid;
    string time, sourcecode;

    auto rows = sicily.db.query<std::tuple<unsigned long, string, string, string, string, string, string, string, string>>(
        "SELECT qid, queue.sid, uid, pid, language, time, cid, cpid, sourcecode FROM queue, status WHERE queue.sid=status.sid AND hold=0 AND server_id=? ORDER BY qid LIMIT 1",
        0);

    if (rows.size() == 0) {
        return false;
    }

    tie(qid, submit.sub_id, submit.user_id, submit.prob_id, submit.language, time, submit.contest_id, submit.contest_prob_id, sourcecode) = rows[0];

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
        // test_data
        filesystem::path case_dir = sicily.testdata / submit.prob_id;
        ifstream fin(case_dir / ".DIR");
        string stdin, stdout;
        while (fin >> stdin >> stdout) {
            test_data data;
            data.inputs.push_back(make_unique<local_asset>(stdin, case_dir / stdin));
            data.outputs.push_back(make_unique<local_asset>(stdout, case_dir / stdout));
            submit.test_data.push_back(move(data));
        }
    }

    if (!submit.contest_id.empty()) {
        string starttime;
        unsigned long duration;
        auto rows = sicily.db.query<std::tuple<string, unsigned long>>(
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

    auto probrows = sicily.db.query<std::tuple<int, int, bool, bool>>(
        "SELECT time_limit, memory_limit, special_judge, has_framework FROM problems WHERE pid=?",
        submit.prob_id);
    if (!probrows.empty()) {
        bool spj, has_framework;
        tie(submit.time_limit, submit.memory_limit, spj, has_framework) = probrows[0];

        if (spj) {
            // TODO:
        }

        if (has_framework) {
            prog->assist_files.push_back(
                make_unique<local_asset>("framework.cpp", get_data_path(sicily.testdata, submit.prob_id, "framework.cpp")));
        }
    }

    submit.submission = move(prog);

    return true;
}

void set_compilelog(configuration &sicily, const filesystem::path &logfile, const submission &submit) {
    if (!filesystem::exists(logfile))
        LOG(INFO) << "Error: compile log file " << logfile << " not found.";

    string log = read_file_content(logfile);

    sicily.db.query<std::tuple<>>(
        "UPDATE status set compilelog=? where sid=?",
        log, submit.sub_id);
}

void update_user(configuration &sicily, bool solved, const submission &submit) {
    char list[1051];
    int i;

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
                puts("Update Non-ac Submissions");

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

void set_status(configuration &sicily, const status &judge_status, int current_case, const submission &submit) {
    static set<status> final_status = {status::COMPILING, status::RUNNING, status::ACCEPTED, status::PRESENTATION_ERROR,
                                       status::WRONG_ANSWER, status::TIME_LIMIT_EXCEEDED, status::MEMORY_LIMIT_EXCEEDED,
                                       status::RUNTIME_ERROR, status::SEGMENTATION_FAULT, status::FLOATING_POINT_ERROR};

    int i;
    string runtime, memory;

    bool is_multiple_cases = submit.test_data.size() > 1;
    if (final_status.count(judge_status)) {
        sicily.db.query<std::tuple<>>(
            "UPDATE status SET status=?, failcase=?, run_time=?, run_memory=? WHERE sid=?",
            status_string.at(judge_status),
            is_multiple_cases ? current_case : -1,
            spend_time,
            spend_memory,
            submit.sub_id);
    } else {
        sicily.db.query<std::tuple<>>(
            "UPDATE status SET status=?, failcase=? WHERE sid=?",
            status_string.at(judge_status),
            is_multiple_cases ? current_case : -1,
            submit.sub_id);
    }
    LOG(INFO) << "Judge: " << status_string.at(judge_status) << i;
}

// clang-format off
const unordered_map<status, const char *> status_string = boost::assign::map_list_of
    (status::PENDING, "Waiting")
    (status::RUNNING, "Judging")
    (status::ACCEPTED, "Accepted")
    (status::COMPILATION_ERROR, "Compile Error")
    (status::WRONG_ANSWER, "Wrong Answer")
    (status::RUNTIME_ERROR, "Runtime Error")
    (status::TIME_LIMIT_EXCEEDED, "Time Limit Exceeded")
    (status::MEMORY_LIMIT_EXCEEDED, "Memory Limit Exceeded")
    (status::OUTPUT_LIMIT_EXCEEDED, "Output Limit Exceeded")
    (status::PRESENTATION_ERROR, "Presentation Error")
    (status::RESTRICT_FUNCTION, "Restrict Function")
    (status::OUT_OF_CONTEST_TIME, "Out of Contest Time")
    (status::OTHER, "Other")
    (status::COMPILING, "Judging")
    (status::SEGMENTATION_FAULT, "Segmentation Fault")
    (status::FLOATING_POINT_ERROR, "Floating Point Error");
// clang-format on
}  // namespace judge::server::sicily