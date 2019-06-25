#include "server/moj/moj.hpp"
#include <boost/assign.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include "common/io_utils.hpp"
#include "common/messages.hpp"
#include "common/stl_utils.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "server/moj/feedback.hpp"

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;

// clang-format off
const unordered_map<status, const char *> status_string = boost::assign::map_list_of
    (status::PENDING, "")
    (status::RUNNING, "")
    (status::ACCEPTED, "OK")
    (status::COMPILATION_ERROR, "CE")
    (status::WRONG_ANSWER, "WA")
    (status::RUNTIME_ERROR, "RE")
    (status::TIME_LIMIT_EXCEEDED, "TL")
    (status::MEMORY_LIMIT_EXCEEDED, "ML")
    (status::OUTPUT_LIMIT_EXCEEDED, "OL")
    (status::PRESENTATION_ERROR, "PE")
    (status::RESTRICT_FUNCTION, "RF")
    (status::OUT_OF_CONTEST_TIME, "") // Too Late
    (status::COMPILING, "")
    (status::SEGMENTATION_FAULT, "RE")
    (status::FLOATING_POINT_ERROR, "RE");
// clang-format on

configuration::configuration()
    : exec_mgr(CACHE_DIR, EXEC_DIR) {}

void configuration::init(const filesystem::path &config_path) {
    if (!filesystem::exists(config_path))
        throw runtime_error("Unable to find configuration file");
    ifstream fin(config_path);
    json config;
    fin >> config;
    config.at("submissionReportQueue").get_to(submission_report);
    config.at("choiceSubmissionQueue").get_to(choice_submission);
    config.at("programmingSubmissionQueue").get_to(programming_submission);
    config.at("systemConfig").get_to(system);

    submission_reporter = make_unique<judge_result_reporter>(submission_report);
    programming_fetcher = make_unique<submission_fetcher>(programming_submission);
    choice_fetcher = make_unique<submission_fetcher>(choice_submission);
}

string configuration::category() const {
    return "moj";
}

const executable_manager &configuration::get_executable_manager() const {
    return exec_mgr;
}

struct moj_remote_file : public asset {
    moj_remote_file(const string &path)
        : asset(substr_after_last(path, '/')) {
    }

    void fetch(const filesystem::path & /* path */) override {
        // TODO
    }
};

static asset_uptr moj_url_to_remote_file(const string &path) {
    return make_unique<moj_remote_file>(path);
}

/*
json 示例
{
    "version": 2,
    "sub_id": 1,
    "prob_id": 1,
    "language": "c++",
    "last_update": 1111111111,
    "config": {
        "grading": {
            "c++": {
                "CompileCheck": 20,
                "MemoryCheck": 0,
                "RandomCheck": 50,
                "StandardCheck": 20,
                "StaticCheck": 10,
                "GTestCheck": 0
            }
        },
        "files": {
            "c++": {
                "support": {            //support内为依赖文件, 即提供给学生的不用作答的文件(包括可见和不可见), 学生的提交的代码(submission部分)会和依赖文件一起编译，随机测试时标准答案(standard部分)也会和依赖文件一起编译
                    "source_files": [
                        "main.cpp"
                    ],
                    "header_files": [
                        "BulkItem.h",
                        "BookItem.h"
                    ],
                    "rw_files": []     //普通文件，如可用作文件读写
                },
                "submission": {
                    "source_files": [
                        "BulkItem.cpp",
                        "BookItem.cpp"
                    ],
                    "header_files": []
                },
                "standard": {
                    "source_files": [
                        "BulkItem.cpp",
                        "BookItem.cpp"
                    ],
                    "header_files": [],
                    "input": [
                        "input0"
                    ],
                    "output": [
                        "output0"
                    ]
                },
                "random": [
                    "random.c"    //随机测试分数不为0时，需要提交随机数生成器
                ]
            }
        },
        "compile": {
            "c++": [
                "{INPUT}",
                "-Werror",
                "-g",
                "-lm",
                "-o",
                "{OUTPUT}"
            ]
        },
        "limits": {
            "c++": {
                "memory": 32,
                "time": 1000
            }
        },
        "random": {
            "c++": {
                "language": "c",
                "compile": [
                    "{INPUT}",
                    "-w",
                    "-o",
                    "{OUTPUT}"
                ],
                "run_times": 100
            }
        }
    }
}
*/
static void from_json_moj(const json &j, configuration &server, judge::server::submission &submit) {
    submit.category = server.category();

    int version, sub_id, prob_id;
    j.at("version").get_to(version);
    j.at("sub_id").get_to(sub_id);    // type int
    j.at("prob_id").get_to(prob_id);  // type int
    submit.sub_id = to_string(sub_id);
    submit.prob_id = to_string(prob_id);

    const json &config = j.at("config");

    unique_ptr<source_code> submission = make_unique<source_code>(server.exec_mgr);
    unique_ptr<source_code> standard = make_unique<source_code>(server.exec_mgr);

    string language = j.at("language").get<string>();
    submission->language = standard->language = language;

    const json &files = config.at("files").at(language);
    if (files.count("support")) {
        auto src_url = files.at("support").at("source_files").get<vector<string>>();
        auto hdr_url = files.at("support").at("header_files").get<vector<string>>();
        append(submission->source_files, src_url, moj_url_to_remote_file);
        append(submission->assist_files, hdr_url, moj_url_to_remote_file);
        append(standard->source_files, src_url, moj_url_to_remote_file);
        append(standard->assist_files, hdr_url, moj_url_to_remote_file);
    }

    if (files.count("submission")) {
        auto src_url = files.at("submission").at("source_files").get<vector<string>>();
        append(submission->source_files, src_url, moj_url_to_remote_file);

        auto hdr_url = files.at("submission").at("header_files").get<vector<string>>();
        append(submission->assist_files, hdr_url, moj_url_to_remote_file);
    }

    if (files.count("standard")) {
        const json &standard_json = files.at("standard");
        auto src_url = standard_json.at("source_files").get<vector<string>>();
        append(standard->source_files, src_url, moj_url_to_remote_file);

        auto hdr_url = standard_json.at("header_files").get<vector<string>>();
        append(standard->assist_files, hdr_url, moj_url_to_remote_file);

        auto input_url = standard_json.at("input").get<vector<string>>();
        auto output_url = standard_json.at("output").get<vector<string>>();
        for (size_t i = 0; i < input_url.size() && output_url.size(); ++i) {
            test_case_data datacase;
            datacase.inputs.push_back(moj_url_to_remote_file(input_url[i]));
            datacase.inputs[0]->name = "testdata.in"; // 我们要求输入数据文件名必须为 testdata.in
            datacase.outputs.push_back(moj_url_to_remote_file(output_url[i]));
            datacase.outputs[0]->name = "testdata.out"; // 我们要求输出数据文件名必须为 testdata.out
            submit.test_data.push_back(move(datacase));
        }
    }

    const json &compile = config.at("compile").at(language);
    compile.get_to(submission->compile_command);
    compile.get_to(standard->compile_command);

    const json &limits = config.at("limits").at(language);
    limits.at("memory").get_to(submit.memory_limit);
    limits.at("time").get_to(submit.time_limit);

    if (config.count("random")) {
        auto random_ptr = make_unique<source_code>(server.exec_mgr);
        const json &random = config.at("random").at(language);
        random.at("language").get_to(random_ptr->language);
        random.at("compile").get_to(random_ptr->compile_command);
        random.at("run_times").get_to(submit.random_test_times);
        submit.random = move(random_ptr);
    }

    submit.submission = move(submission);
    submit.standard = move(standard);
    submit.compare = server.exec_mgr.get_compare_script("diff-ign-space");

    const json &grading = config.at("grading").at(language);
    for (auto &check : grading.items()) {
        test_check testcase;
        int grade = check.value().get<int>();
        testcase.score = grade;
        if (!grade) continue;
        if (check.key() == "CompileCheck") {
            testcase.check_type = 0;
        } else if (check.key() == "MemoryCheck") {
            testcase.check_type = memory_check_report::TYPE;
            testcase.check_script = "memory";
            testcase.is_random = submit.test_cases.empty();

            size_t case_num = testcase.is_random ? 10 : submit.test_cases.size();
            for (size_t i = 0; i < case_num; ++i) {
                testcase.testcase_id = i;
                submit.test_cases.push_back(testcase);
            }
        } else if (check.key() == "RandomCheck") {
            testcase.check_type = random_check_report::TYPE;
            testcase.check_script = "standard";
            testcase.is_random = true;

            for (size_t i = 0; i < submit.random_test_times; ++i) {
                testcase.testcase_id = i;
                submit.test_cases.push_back(testcase);
            }
        } else if (check.key() == "StandardCheck") {
            testcase.check_type = standard_check_report::TYPE;
            testcase.check_script = "standard";
            testcase.is_random = false;

            for (size_t i = 0; i < submit.test_cases.size(); ++i) {
                testcase.testcase_id = i;
                submit.test_cases.push_back(testcase);
            }
        } else if (check.key() == "StaticCheck") {
            testcase.check_type = static_check_report::TYPE;
            // TODO: not implemented
        } else if (check.key() == "GTestCheck") {
            testcase.check_type = gtest_check_report::TYPE;
            // TODO: not implemented
        }
    }
}

bool configuration::fetch_submission(submission &submit) {
    string message;
    if (programming_fetcher->fetch(message, 0)) {
        from_json_moj(json::parse(message), *this, submit);
        return true;
    }

    if (choice_fetcher->fetch(message, 0)) {
        // TODO: choice judger
        return true;
    }

    return false;
}

static json get_error_report(const judge::message::task_result &result) {
    error_report report;
    report.message = read_file_content(filesystem::path(result.path_to_error));
    return report;
}

void configuration::summarize(submission &submit, const vector<judge::message::task_result> &task_results) {
    judge_report report;
    report.grade = 0;
    report.sub_id = boost::lexical_cast<unsigned>(submit.sub_id);
    report.prob_id = boost::lexical_cast<unsigned>(submit.prob_id);
    report.is_complete = true;

    json compile_check_json;
    {
        compile_check_report compile_check;
        compile_check.full_grade = boost::rational_cast<double>(submit.test_cases[0].score);
        compile_check.report = read_file_content(filesystem::path(task_results[0].path_to_error));
        if (task_results[0].status == status::ACCEPTED) {
            compile_check.grade = compile_check.grade;
            compile_check_json = compile_check;
        } else if (task_results[0].status == status::SYSTEM_ERROR) {
            compile_check_json = get_error_report(task_results[0]);
        } else {
            compile_check.grade = 0;
            compile_check_json = compile_check;
        }
    }

    memory_check_report memory_check;

    json random_check_json;
    {
        random_check_report random_check;
        random_check.pass_cases = 0;
        random_check.total_cases = 0;
        boost::rational<int> score, full_score;
        for (size_t i = 0; i < task_results.size(); ++i) {
            auto &task_result = task_results[i];
            if (task_result.type == random_check_report::TYPE) {
                check_case_report kase;
                kase.result = status_string.at(task_result.status);
                filesystem::path rundir(task_result.path_to_error);
                kase.stdin = read_file_content(rundir / "input" / "testdata.in");
                kase.stdout = read_file_content(rundir / "output" / "testdata.out");
                kase.subout = read_file_content(rundir / "program.out");

                if (task_result.status == status::ACCEPTED) {
                    score += submit.test_cases[i].score;
                    ++random_check.pass_cases;
                } else if (task_result.status == status::SYSTEM_ERROR) {
                    random_check_json = get_error_report(task_result);
                    goto random_check_final;
                }

                full_score += submit.test_cases[i].score;
                ++random_check.total_cases;
                random_check.report.push_back(kase);
            }
        }
        random_check.grade = boost::rational_cast<double>(score);
        random_check.full_grade = boost::rational_cast<double>(full_score);
    random_check_final:;
    }

    json standard_check_json;
    {
        standard_check_report standard_check;
        standard_check.pass_cases = 0;
        standard_check.total_cases = 0;
        boost::rational<int> score, full_score;
        for (size_t i = 0; i < task_results.size(); ++i) {
            auto &task_result = task_results[i];
            if (task_result.type == standard_check_report::TYPE) {
                check_case_report kase;
                kase.result = status_string.at(task_result.status);
                filesystem::path rundir(task_result.run_dir);
                filesystem::path datadir(task_result.data_dir);
                kase.stdin = read_file_content(datadir / "input" / "testdata.in");
                kase.stdout = read_file_content(datadir / "output" / "testdata.out");
                kase.subout = read_file_content(rundir / "program.out");

                if (task_result.status == status::ACCEPTED) {
                    score += submit.test_cases[i].score;
                    ++standard_check.pass_cases;
                } else if (task_result.status == status::SYSTEM_ERROR) {
                    standard_check_json = get_error_report(task_result);
                    goto standard_check_final;
                }

                full_score += submit.test_cases[i].score;
                ++standard_check.total_cases;
                standard_check.report.push_back(kase);
            }
        }
        standard_check.grade = boost::rational_cast<double>(score);
        standard_check.full_grade = boost::rational_cast<double>(full_score);
    standard_check_final:;
    }

    static_check_report static_check;
    gtest_check_report gtest_check;

    report.report = {{"CompileCheck", compile_check_json},
                     {"MemoryCheck", memory_check},
                     {"RandomCheck", random_check_json},
                     {"StandardCheck", standard_check_json},
                     {"StaticCheck", static_check},
                     {"GTestCheck", gtest_check}};

    json report_json = report;
    submission_reporter->report(report_json.dump());
}

}  // namespace judge::server::moj
