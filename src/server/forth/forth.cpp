#include "server/forth/forth.hpp"
#include <glog/logging.h>
#include <boost/assign.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include "common/io_utils.hpp"
#include "common/messages.hpp"
#include "common/stl_utils.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "judge/programming.hpp"

namespace judge {
using namespace std;
using namespace nlohmann;

// clang-format off
const unordered_map<status, const char *> status_string = boost::assign::map_list_of
    (status::PENDING, "Pending")
    (status::RUNNING, "Running")
    (status::ACCEPTED, "Accepted")
    (status::PARTIAL_CORRECT, "Partial Correct")
    (status::COMPILATION_ERROR, "Compilation Error")
    (status::EXECUTABLE_COMPILATION_ERROR, "Executable Compilation Error")
    (status::DEPENDENCY_NOT_SATISFIED, "Dependency Not Satisfied")
    (status::WRONG_ANSWER, "Wrong Answer")
    (status::RUNTIME_ERROR, "Runtime Error")
    (status::TIME_LIMIT_EXCEEDED, "Time Limit Exceeded")
    (status::MEMORY_LIMIT_EXCEEDED, "Memory Limit Exceeded")
    (status::OUTPUT_LIMIT_EXCEEDED, "Output Limit Exceeded")
    (status::PRESENTATION_ERROR, "Presentation Error")
    (status::RESTRICT_FUNCTION, "Restrict Function")
    (status::OUT_OF_CONTEST_TIME, "Out of Contest Time")
    (status::COMPILING, "Compiling")
    (status::SEGMENTATION_FAULT, "Segmentation Fault")
    (status::FLOATING_POINT_ERROR, "Floating Point Error")
    (status::RANDOM_GEN_ERROR, "Random Gen Error")
    (status::COMPARE_ERROR, "Compare Error")
    (status::SYSTEM_ERROR, "System Error");
// clang-format on

void from_json(const json &j, judge_task::dependency_condition &value) {
    string str = j.get<string>();
    if (str == "ACCEPTED")
        value = judge_task::dependency_condition::ACCEPTED;
    else if (str == "NOT_TIME_LIMIT")
        value = judge_task::dependency_condition::NON_TIME_LIMIT;
    else if (str == "PARTIAL_CORRECT")
        value = judge_task::dependency_condition::PARTIAL_CORRECT;
    else
        throw std::invalid_argument("Unrecognized dependency_condition " + str);
}

void from_json(const json &j, judge_task &value) {
    value.check_type = 0;
    j.at("check_script").get_to(value.check_script);
    j.at("run_script").get_to(value.run_script);
    j.at("compare_script").get_to(value.compare_script);
    j.at("is_random").get_to(value.is_random);
    assign_optional(j, value.testcase_id, "testcase_id");
    j.at("depends_on").get_to(value.depends_on);
    assign_optional(j, value.depends_cond, "depends_cond");
    assign_optional(j, value.memory_limit, "memory_limit");
    j.at("time_limit").get_to(value.time_limit), value.time_limit /= 1000;
    assign_optional(j, value.file_limit, "file_limit");
    assign_optional(j, value.proc_limit, "proc_limit");
}

void from_json(const json &j, asset_uptr &asset) {
    string type = get_value<string>(j, "type");
    string name = get_value<string>(j, "name");
    if (type == "text") {
        asset = make_unique<text_asset>(name, get_value<string>(j, "text"));
    } else if (type == "remote") {
        asset = make_unique<remote_asset>(name, get_value<string>(j, "url"));
    } else if (type == "local") {
        asset = make_unique<local_asset>(name, filesystem::path(get_value<string>(j, "path")));
    } else {
        throw invalid_argument("Unrecognized asset type " + type);
    }
}

void from_json(const json &j, test_case_data &value) {
    j.at("inputs").get_to(value.inputs);
    j.at("outputs").get_to(value.outputs);
}

void from_json(const json &j, unique_ptr<source_code> &value, executable_manager &exec_mgr) {
    value = make_unique<source_code>(exec_mgr);
    j.at("language").get_to(value->language);
    assign_optional(j, value->entry_point, "entry_point");
    assign_optional(j, value->source_files, "source_files");
    assign_optional(j, value->assist_files, "assist_files");
    assign_optional(j, value->compile_command, "compile_command");
}

void from_json(const json &j, unique_ptr<program> &value, executable_manager &exec_mgr) {
    string type = get_value<string>(j, "type");
    if (type == "source_code") {
        unique_ptr<source_code> p;
        from_json(j, p, exec_mgr);
        value = move(p);
    } else {
        throw invalid_argument("Unrecognized program type " + type);
    }
    // TOOD: type == "executable"
}

void from_json(const json &j, programming_submission &submit, executable_manager &exec_mgr) {
    j.at("judge_tasks").get_to(submit.judge_tasks);
    for (auto &test_data : j.at("test_data")) {
        test_case_data data;
        from_json(test_data, data);
        submit.test_data.push_back(move(data));
    }
    if (exists(j, "submission")) from_json(j.at("submission"), submit.submission, exec_mgr);
    if (exists(j, "standard")) from_json(j.at("standard"), submit.standard, exec_mgr);
    if (exists(j, "compare")) from_json(j.at("compare"), submit.compare, exec_mgr);
    if (exists(j, "random")) from_json(j.at("random"), submit.random, exec_mgr);
}

void from_json(const json &j, unique_ptr<submission> &submit, executable_manager &exec_mgr) {
    string sub_type = get_value<string>(j, "sub_type");
    if (sub_type == "programming") {
        auto p = make_unique<programming_submission>();
        from_json(j, *p, exec_mgr);
        submit = move(p);
    } else {
        throw invalid_argument("Unrecognized submission type " + sub_type);
    }

    submit->sub_type = sub_type;
    j.at("category").get_to(submit->category);
    j.at("prob_id").get_to(submit->prob_id);
    j.at("sub_id").get_to(submit->sub_id);
    j.at("updated_at").get_to(submit->updated_at);
}

struct judge_report {
    string sub_type;
    string category;
    string prob_id;
    string sub_id;
    vector<judge_task_result> results;
};

void to_json(json &j, const judge_task_result &result) {
    json report;
    try {
        report = json::parse(result.report);
    } catch (json::exception &e) {
        report = "";
    }

    j = {{"status", status_string.at(result.status)},
         {"score", fmt::format("{}/{}", result.score.numerator(), max(result.score.denominator(), 1))},
         {"run_time", (int)result.run_time * 1000},
         {"memory_used", result.memory_used >> 10},
         {"error_log", result.error_log},
         {"report", report}};
}

void to_json(json &j, const judge_report &report) {
    j = {{"sub_type", report.sub_type},
         {"category", report.category},
         {"prob_id", report.prob_id},
         {"sub_id", report.sub_id},
         {"results", report.results}};
}

}  // namespace judge

namespace judge::server::forth {
using namespace std;
using namespace nlohmann;

configuration::configuration()
    : exec_mgr(CACHE_DIR, EXEC_DIR) {}

void configuration::init(const filesystem::path &config_path) {
    if (!filesystem::exists(config_path))
        throw runtime_error("Unable to find configuration file");
    ifstream fin(config_path);
    json config;
    fin >> config;

    config.at("category").get_to(category_name);
    config.at("submissionQueue").get_to(sub_queue);
    sub_fetcher = make_unique<rabbitmq>(sub_queue, false);
    config.at("reportQueue").get_to(report_queue);
    judge_reporter = make_unique<rabbitmq>(report_queue, true);
}

string configuration::category() const {
    return category_name;
}

const executable_manager &configuration::get_executable_manager() const {
    return exec_mgr;
}

static bool report_to_server(configuration &server, const judge_report &report) {
    server.judge_reporter->report(json(report).dump());
    return true;
}

bool configuration::fetch_submission(unique_ptr<submission> &submit) {
    AmqpClient::Envelope::ptr_t envelope;
    if (sub_fetcher->fetch(envelope, 0)) {
        try {
            from_json(json::parse(envelope->Message()->Body()), submit, exec_mgr);
            submit->envelope = envelope;
            return true;
        } catch (exception &ex) {
            sub_fetcher->ack(envelope);
            throw ex;
        }
    }

    return false;
}

void configuration::summarize_invalid(submission &) {
    // TODO
}

void summarize_programming(configuration &server, programming_submission &submit) {
    judge_report report;
    report.category = submit.category;
    report.sub_type = submit.sub_type;
    report.sub_id = submit.sub_id;
    report.prob_id = submit.prob_id;
    report.results = submit.results;

    if (report_to_server(server, report))
        server.sub_fetcher->ack(any_cast<AmqpClient::Envelope::ptr_t>(submit.envelope));

    // DLOG(INFO) << "Submission report: " << report_json.dump(4);
}

void configuration::summarize(submission &submit) {
    if (submit.sub_type == "programming") {
        summarize_programming(*this, dynamic_cast<programming_submission &>(submit));
    } else {
        throw runtime_error("Unrecognized submission type " + submit.sub_type);
    }
}

}  // namespace judge::server::forth
