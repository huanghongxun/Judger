#include "server/mcourse/feedback.hpp"
#include <boost/lexical_cast.hpp>

namespace judge::server::mcourse {
using namespace boost;
using namespace nlohmann;
using namespace std;

void from_json(const json &j, judge_request &report) {
    if (j.at("submissionId").is_number())
        report.sub_id = j.at("submissionId").get<int>();
    else
        report.sub_id = lexical_cast<int>(j.at("submissionId").get<string>());

    if (j.at("standardId").is_number())
        report.prob_id = j.at("standardId").get<int>();
    else
        report.prob_id = lexical_cast<int>(j.at("standardId").get<string>());

    if (j.at("problemType").is_number()) {
        switch (j.at("problemType").get<int>()) {
            case 0:
                report.prob_type = judge_request::programming;
                break;
            case 1:
                report.prob_type = judge_request::choice;
                break;
            case 4:
                report.prob_type = judge_request::output;
                break;
            case 5:
                report.prob_type = judge_request::program_blank_filling;
                break;
            default:
                throw out_of_range("Unrecognized problem type");
        }
    } else {
        string problem_type = j.at("problemType").get<string>();
        if (problem_type == "programming")
            report.prob_type = judge_request::programming;
        else if (problem_type == "choice")
            report.prob_type = judge_request::choice;
        else if (problem_type == "output")
            report.prob_type = judge_request::output;
        else if (problem_type == "programBlankFilling")
            report.prob_type = judge_request::program_blank_filling;
        else
            throw out_of_range("Unrecognized problem type " + problem_type);
    }

    if (j.at("submissionType").is_number()) {
        int sub_type = j.at("submissionType").get<int>();
        if (sub_type == 0)
            report.sub_type = judge_request::student;
        else if (sub_type == 1)
            report.sub_type = judge_request::teacher;
        else
            throw out_of_range("Unrecognized submission type " + to_string(sub_type));
    } else {
        string sub_type = j.at("submissionType").get<string>();
        if (sub_type == "0" || sub_type == "student")
            report.sub_type = judge_request::student;
        else if (sub_type == "1" || sub_type == "teacher")
            report.sub_type = judge_request::teacher;
        else
            throw out_of_range("Unrecognized submission type " + sub_type);
    }

    if (exists(j, "token")) j.at("token").get_to(report.token);
    if (exists(j, "config")) report.config = j.at("config");
    if (exists(j, "detail")) report.detail = j.at("detail");
    if (exists(j, "updated_at")) {
        if (j.at("updated_at").is_number())
            report.updated_at = j.at("updated_at").get<double>();
        else {
            string time = j.at("updated_at").get<string>();
            struct tm mytm;
            strptime(time.c_str(), "%Y-%m-%d %H:%M:%S", &mytm);
            report.updated_at = mktime(&mytm);
        }
    }
}

void to_json(json &j, const error_report &report) {
    j = {{"message", report.message},
         {"result", report.result},
         {"continue", false},
         {"grade", 0}};
}

void to_json(json &j, const compile_check_report &report) {
    j = {{"continue", report.cont},
         {"grade", report.grade},
         {"compile check", report.message}};
}

void to_json(json &j, const check_case_report &report) {
    j = {{"stdin", report.stdin},
         {"result", report.result},
         {"stdout", report.stdout},
         {"message", report.message},
         {"timeused", report.timeused},
         {"memoryused", report.memoryused},
         {"standard_stdout", report.standard_stdout}};
}

void to_json(json &j, const standard_check_report &report) {
    j = {{"grade", report.grade},
         {"continue", true},
         {"standard tests", report.cases}};
}

void to_json(json &j, const random_check_report &report) {
    j = {{"grade", report.grade},
         {"continue", true},
         {"random tests", report.cases}};
}

void to_json(json &j, const memory_check_error_report &report) {
    if (report.message.empty())
        j = {{"valgrindoutput", report.valgrindoutput},
             {"stdin", report.stdin}};
    else {
        j = {{"message", report.message},
             {"stdin", report.stdin}};
    }
}

void to_json(json &j, const memory_check_report &report) {
    j = {{"grade", report.grade},
         {"continue", true},
         {"memory check", report.report}};
}

void to_json(json &j, const static_check_report &report) {
    j = {{"grade", report.grade},
         {"continue", true},
         {"static check", report.report}};
}

void to_json(json &j, const gtest_check_report &report) {
    json failure = json::array();
    if (report.error_message.empty()) {
        for (auto &[key, value] : report.failed_cases) {
            failure += {{key, value}};
        }
    } else {
        failure += {{"error", report.error_message}};
    }
    if (failure.empty()) failure = json();
    json gtest = {{"info", report.info},
                  {"grade", report.grade},
                  {"failure", failure}};
    gtest = {{"gtest", gtest}};
    j = {{"grade", report.grade},
         {"continue", report.cont},
         {"google tests", json::array({gtest})}};
}

}  // namespace judge::server::mcourse
