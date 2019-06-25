#include "server/moj/feedback.hpp"

namespace judge::server::moj {

void to_json(json &j, const judge_report &report) {
    j = {{"sub_id", report.sub_id},
         {"prob_id", report.prob_id},
         {"grade", report.grade},
         {"report", report.report},
         {"is_complete", report.is_complete}};
}

void to_json(json &j, const error_report &report) {
    j = {{"message", report.message}};
}

void to_json(json &j, const check_error_report &report) {
    j = {{"error", report.error}};
}

void to_json(json &j, const compile_check_report &report) {
    j = {{"grade", report.grade},
         {"full_grade", report.full_grade},
         {"pass", report.pass},
         {"report", report.report}};
}

void to_json(json &j, const static_check_report &report) {
    j = {{"grade", report.grade},
         {"full_grade", report.full_grade},
         {"report", report.report}};
}

void to_json(json &j, const check_case_report &report) {
    j = {{"result", report.result},
         {"stdin", report.stdin},
         {"stdout", report.stdout},
         {"subout", report.subout}};
}

void to_json(json &j, const random_check_report &report) {
    j = {{"grade", report.grade},
         {"full_grade", report.full_grade},
         {"pass_cases", report.pass_cases},
         {"total_cases", report.total_cases},
         {"report", report.report}};
}

void to_json(json &j, const standard_check_report &report) {
    j = {{"grade", report.grade},
         {"full_grade", report.full_grade},
         {"pass_cases", report.pass_cases},
         {"total_cases", report.total_cases},
         {"report", report.report}};
}

void to_json(json &j, const memory_check_report_report &report) {
    j = {{"valgrindoutput", report.valgrindoutput},
         {"stdin", report.stdin}};
}

void to_json(json &j, const memory_check_report &report) {
    j = {{"grade", report.grade},
         {"full_grade", report.full_grade},
         {"pass_cases", report.pass_cases},
         {"total_cases", report.total_cases},
         {"report", report.report}};
}

void to_json(json & /* j */, const gtest_check_report & /* report */) {
    // TODO: not implemented
}

}  // namespace judge::server::moj
