#include "server/moj/moj.hpp"
#include <glog/logging.h>
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

static function<asset_uptr(const string &)> moj_url_to_remote_file(configuration &config, const string &subpath) {
    return [&](const string &name) {
        return make_unique<remote_asset>(name, config.system.file_api + "/" + subpath + "/" + name);
    };
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
    DLOG(INFO) << "Receive moj submission: " << j.dump(4);

    submit.category = server.category();

    int version = 1, sub_id, prob_id;
    if (j.count("version")) j.at("version").get_to(version);
    j.at("sub_id").get_to(sub_id);    // type int
    j.at("prob_id").get_to(prob_id);  // type int
    submit.sub_id = to_string(sub_id);
    submit.prob_id = to_string(prob_id);

    const json &config = j.at("config");

    unique_ptr<source_code> submission = make_unique<source_code>(server.exec_mgr);
    unique_ptr<source_code> standard = make_unique<source_code>(server.exec_mgr);

    string language = j.at("language").get<string>();
    // 硬编码将 c++ 改成 cpp
    if (language == "c++")
        submission->language = standard->language = "cpp";
    else
        submission->language = standard->language = language;

    const json &files = config.at("files").at(language);
    if (files.count("support")) {
        auto src_url = files.at("support").at("source_files").get<vector<string>>();
        auto hdr_url = files.at("support").at("header_files").get<vector<string>>();
        // 依赖文件的下载地址：FILE_API/problem/<prob_id>/support/<filename>
        append(submission->source_files, src_url, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));
        append(submission->assist_files, hdr_url, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));
        append(standard->source_files, src_url, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));
        append(standard->assist_files, hdr_url, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));
    }

    const json &compile = config.at("compile").at(language);
    compile.get_to(submission->compile_command);
    compile.get_to(standard->compile_command);

    const json &limits = config.at("limits").at(language);
    limits.at("memory").get_to(submit.memory_limit);
    submit.memory_limit <<= 10;
    submission->memory_limit = standard->memory_limit = submit.memory_limit;
    limits.at("time").get_to(submit.time_limit);
    submit.time_limit /= 1000;
    submit.proc_limit = -1;
    submit.file_limit = 32768;  // 32M

    if (files.count("submission")) {
        auto src_url = files.at("submission").at("source_files").get<vector<string>>();
        // 选手提交的下载地址：FILE_API/submission/<sub_id>/<filename>
        append(submission->source_files, src_url, moj_url_to_remote_file(server, fmt::format("submission/{}", submit.sub_id)));

        auto hdr_url = files.at("submission").at("header_files").get<vector<string>>();
        // 选手提交的下载地址：FILE_API/submission/<sub_id>/<filename>
        append(submission->assist_files, hdr_url, moj_url_to_remote_file(server, fmt::format("submission/{}", submit.sub_id)));

        submit.submission = move(submission);
    }

    if (files.count("standard")) {
        const json &standard_json = files.at("standard");
        auto src_url = standard_json.at("source_files").get<vector<string>>();
        // 标准程序的下载地址：FILE_API/problem/<prob_id>/support/<filename>
        append(standard->source_files, src_url, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));

        auto hdr_url = standard_json.at("header_files").get<vector<string>>();
        // 标准程序的下载地址：FILE_API/problem/<prob_id>/support/<filename>
        append(standard->assist_files, hdr_url, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));

        auto input_url = standard_json.at("input").get<vector<string>>();
        auto output_url = standard_json.at("output").get<vector<string>>();
        for (size_t i = 0; i < input_url.size() && i < output_url.size(); ++i) {
            test_case_data datacase;
            // 标准输入的的下载地址：FILE_API/problem/<prob_id>/standard_input/<filename>
            datacase.inputs.push_back(moj_url_to_remote_file(server, fmt::format("problem/{}/standard_input", submit.prob_id))(input_url[i]));
            datacase.inputs[0]->name = "testdata.in";  // 我们要求输入数据文件名必须为 testdata.in
            // 标准输出的的下载地址：FILE_API/problem/<prob_id>/standard_output/<filename>
            datacase.outputs.push_back(moj_url_to_remote_file(server, fmt::format("problem/{}/standard_output", submit.prob_id))(output_url[i]));
            datacase.outputs[0]->name = "testdata.out";  // 我们要求输出数据文件名必须为 testdata.out
            submit.test_data.push_back(move(datacase));
        }

        submit.standard = move(standard);
    }

    if (config.count("random")) {
        auto random_ptr = make_unique<source_code>(server.exec_mgr);
        random_ptr->memory_limit = SCRIPT_MEM_LIMIT;
        const json &random = config.at("random").at(language);
        random.at("language").get_to(random_ptr->language);
        random.at("compile").get_to(random_ptr->compile_command);
        random.at("run_times").get_to(submit.random_test_times);

        if (files.count("random")) {
            const json &random_json = files.at("random");
            auto src_url = random_json.get<vector<string>>();
            // 随机数据生成器的下载地址：FILE_API/problem/<prob_id>/random_source/<filename>
            append(random_ptr->source_files, src_url, moj_url_to_remote_file(server, fmt::format("problem/{}/random_source", submit.prob_id)));
        }

        submit.random = move(random_ptr);
    }

    submit.compare = server.exec_mgr.get_compare_script("diff-ign-space");

    const json &grading = config.at("grading").at(language);
    vector<int> standard_checks;  // 标准测试的评测点编号
    vector<int> random_checks;    // 随机测试的评测点编号

    for (auto &check : grading.items()) {
        test_check testcase;
        int grade = check.value().get<int>();
        testcase.score = grade;
        testcase.depends_on = 0;  // 依赖编译任务
        testcase.depends_cond = test_check::depends_condition::ACCEPTED;
        if (!grade) continue;
        if (check.key() == "CompileCheck") {
            testcase.is_random = false;
            // testcase.score = 0;
            testcase.check_type = message::client_task::COMPILE_TYPE;
            testcase.testcase_id = -1;
            testcase.depends_on = -1;
            submit.test_cases.push_back(testcase);
        } else if (check.key() == "RandomCheck") {
            testcase.check_type = random_check_report::TYPE;
            testcase.check_script = "standard";
            testcase.run_script = "standard";
            testcase.is_random = true;
            testcase.depends_on = 0;  // 依赖编译任务
            testcase.depends_cond = test_check::depends_condition::ACCEPTED;

            for (size_t i = 0; i < submit.random_test_times; ++i) {
                testcase.testcase_id = -1;  // 随机测试使用哪个测试数据点是未知的，需要实际运行时决定
                // 将当前的随机测试点编号记录下来，给内存测试依赖
                random_checks.push_back(submit.test_cases.size());
                submit.test_cases.push_back(testcase);
            }
        } else if (check.key() == "StandardCheck") {
            testcase.check_type = standard_check_report::TYPE;
            testcase.check_script = "standard";
            testcase.run_script = "standard";
            testcase.is_random = false;

            for (size_t i = 0; i < submit.test_data.size(); ++i) {
                testcase.testcase_id = i;
                // 将当前的标准测试点编号记录下来，给内存测试依赖
                standard_checks.push_back(submit.test_cases.size());
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

    // 内存测试需要依赖标准测试或随机测试以便可以在标准或随机测试没有 AC 的情况下终止内存测试以加速评测速度
    for (auto &check : grading.items()) {
        test_check testcase;
        int grade = check.value().get<int>();
        testcase.score = grade;
        if (!grade) continue;
        if (check.key() == "MemoryCheck") {
            testcase.check_type = memory_check_report::TYPE;
            testcase.check_script = "memory";
            testcase.run_script = "standard";
            testcase.is_random = submit.test_cases.empty();

            if (testcase.is_random) {
                if (!random_checks.empty()) {  // 如果存在随机测试，则依赖随机测试点的数据
                    for (int &i : random_checks) {
                        testcase.testcase_id = submit.test_cases[i].testcase_id;
                        testcase.depends_on = i;
                        testcase.depends_cond = test_check::depends_condition::ACCEPTED;
                        submit.test_cases.push_back(testcase);
                    }
                } else {  // 否则只能生成 10 组随机测试数据
                    for (size_t i = 0; i < 10; ++i) {
                        testcase.testcase_id = i;
                        testcase.depends_on = 0;  // 依赖编译任务
                        testcase.depends_cond = test_check::depends_condition::ACCEPTED;
                        submit.test_cases.push_back(testcase);
                    }
                }
            } else {
                for (int &i : standard_checks) {
                    testcase.testcase_id = submit.test_cases[i].testcase_id;
                    testcase.depends_on = i;
                    testcase.depends_cond = test_check::depends_condition::ACCEPTED;
                    submit.test_cases.push_back(testcase);
                }
            }
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

void configuration::summarize_invalid(submission &) {
    throw runtime_error("Invalid submission");
}

static json get_error_report(const judge::message::task_result &result) {
    error_report report;
    report.message = result.error_log;
    return report;
}

/**
 * 执行条件
 * 1. 配置中开启编译检测（编译检测满分大于0）
 * 2. 已存在提交代码的源文件
 */
static void summarize_compile_check(judge_report &report, submission &submit, const vector<judge::message::task_result> &task_results, json &compile_check_json) {
    compile_check_report compile_check;
    compile_check.full_grade = boost::rational_cast<double>(submit.test_cases[0].score);
    compile_check.report = task_results[0].error_log;
    if (task_results[0].status == status::ACCEPTED) {
        compile_check.grade = compile_check.grade;
        compile_check_json = compile_check;
    } else if (task_results[0].status == status::SYSTEM_ERROR) {
        compile_check_json = get_error_report(task_results[0]);
    } else {
        compile_check.grade = 0;
        compile_check_json = compile_check;
    }
    report.grade += compile_check.grade;
}

/**
 * 通过提交的随机生成器产生随机输入文件，分别运行标准答案与提交的答案，比较输出是否相同。
 * 
 * 执行条件
 * 1. 配置中开启随机检测（随机检测满分大于0）
 * 2. 已存在提交代码的可执行文件
 * 3. 已存在随机输入生成器与题目答案源文件
 * 
 * 评分规则
 * 得分=（通过的测试数）/（总测试数）× 总分。
 */
static void summarize_random_check(judge_report &report, submission &submit, const vector<judge::message::task_result> &task_results, json &random_check_json) {
    random_check_report random_check;
    random_check.pass_cases = 0;
    random_check.total_cases = 0;
    boost::rational<int> score, full_score;
    for (size_t i = 0; i < task_results.size(); ++i) {
        auto &task_result = task_results[i];
        if (task_result.type == random_check_report::TYPE) {
            check_case_report kase;
            kase.result = status_string.at(task_result.status);
            kase.stdin = read_file_content(task_result.data_dir / "input" / "testdata.in");
            kase.stdout = read_file_content(task_result.data_dir / "output" / "testdata.out");
            kase.subout = read_file_content(task_result.run_dir / "program.out");

            if (task_result.status == status::ACCEPTED) {
                score += submit.test_cases[i].score;
                ++random_check.pass_cases;
            } else if (task_result.status == status::PARTIAL_CORRECT) {
                score += task_result.score * submit.test_cases[i].score;
            } else if (task_result.status == status::SYSTEM_ERROR ||
                       task_result.status == status::RANDOM_GEN_ERROR ||
                       task_result.status == status::EXECUTABLE_COMPILATION_ERROR ||
                       task_result.status == status::COMPARE_ERROR) {
                random_check_json = get_error_report(task_result);
                return;
            }

            full_score += submit.test_cases[i].score;
            ++random_check.total_cases;
            random_check.report.push_back(kase);
        }
    }
    random_check.grade = boost::rational_cast<double>(score);
    random_check.full_grade = boost::rational_cast<double>(full_score);
    report.grade += random_check.grade;
}

static void summarize_standard_check(judge_report &report, submission &submit, const vector<judge::message::task_result> &task_results, json &standard_check_json) {
    standard_check_report standard_check;
    standard_check.pass_cases = 0;
    standard_check.total_cases = 0;
    boost::rational<int> score, full_score;
    for (size_t i = 0; i < task_results.size(); ++i) {
        auto &task_result = task_results[i];
        if (task_result.type == standard_check_report::TYPE) {
            check_case_report kase;
            kase.result = status_string.at(task_result.status);
            kase.stdin = read_file_content(task_result.data_dir / "input" / "testdata.in");
            kase.stdout = read_file_content(task_result.data_dir / "output" / "testdata.out");
            kase.subout = read_file_content(task_result.run_dir / "program.out");

            if (task_result.status == status::ACCEPTED) {
                score += submit.test_cases[i].score;
                ++standard_check.pass_cases;
            } else if (task_result.status == status::SYSTEM_ERROR ||
                       task_result.status == status::RANDOM_GEN_ERROR ||
                       task_result.status == status::EXECUTABLE_COMPILATION_ERROR ||
                       task_result.status == status::COMPARE_ERROR) {
                standard_check_json = get_error_report(task_result);
                return;
            }

            full_score += submit.test_cases[i].score;
            ++standard_check.total_cases;
            standard_check.report.push_back(kase);
        }
    }
    standard_check.grade = boost::rational_cast<double>(score);
    standard_check.full_grade = boost::rational_cast<double>(full_score);
    report.grade += standard_check.grade;
}

/**
 * 静态检查目前只支持C/C++
 * 采用oclint进行静态检查，用于检测部分代码风格和代码设计问题
 * 
 * 执行条件
 * 1. 配置中开启静态检查（静态检查满分大于0）
 * 
 * 评分规则
 * oclint评测违规分3个等级：priority 1、priority 2、priority 3
 * 评测代码每违规一个 priority 1 扣 2 分，每违规一个 priority 2 扣 1 分，违规 priority 3 不扣分。扣分扣至 0 分为止.
 */
static void summarize_static_check(judge_report &report, submission &submit, const vector<judge::message::task_result> &task_results, json &standard_check_json) {
}

/**
 * 采用 valgrind 进行内存检测，用于发现内存泄露、访问越界等问题
 * 内存检测需要提交代码的可执行文件，并且基于标准测试或随机测试（优先选用标准测试），因此只有当标准测试或随机测试的条件满足时才能进行内存测试
 * 
 * 执行条件
 * 1. 配置中开启内存检测（内存检测满分大于0）
 * 2. 已存在提交代码的可执行文件
 * 3. 已存在标准测试输入（优先选择）或随机输入生成器
 * 
 * 评分规则
 * 内存检测会多次运行提交代码，未检测出问题则通过，最后总分为通过数/总共运行次数 × 内存检测满分分数
 */
static void summarize_memory_check(judge_report &report, submission &submit, const vector<judge::message::task_result> &task_results, json &memory_check_json) {
    memory_check_report memory_check;
    memory_check.pass_cases = 0;
    memory_check.total_cases = 0;
    boost::rational<int> score, full_score;
    for (size_t i = 0; i < task_results.size(); ++i) {
        auto &task_result = task_results[i];
        if (task_result.type == memory_check_report::TYPE) {
            if (task_result.status == status::ACCEPTED) {
                score += submit.test_cases[i].score;
                ++memory_check.pass_cases;
            } else if (task_result.status == status::WRONG_ANSWER) {
                memory_check_report_report kase;
                kase.valgrindoutput = read_file_content(task_result.run_dir / "feedback" / "report.txt");
                kase.stdin = read_file_content(task_result.data_dir / "input" / "testdata.in");
                memory_check.report.push_back(kase);
            } else {
                memory_check_json = get_error_report(task_result);
                return;
            }

            full_score += submit.test_cases[i].score;
            ++memory_check.total_cases;
        }
    }
    memory_check.grade = boost::rational_cast<double>(score);
    memory_check.full_grade = boost::rational_cast<double>(full_score);
    report.grade += memory_check.grade;
}

void configuration::summarize(submission &submit, const vector<judge::message::task_result> &task_results) {
    judge_report report;
    report.grade = 0;
    report.sub_id = boost::lexical_cast<unsigned>(submit.sub_id);
    report.prob_id = boost::lexical_cast<unsigned>(submit.prob_id);
    report.is_complete = true;

    json compile_check_json;
    summarize_compile_check(report, submit, task_results, compile_check_json);

    json memory_check_json;
    summarize_memory_check(report, submit, task_results, memory_check_json);

    json random_check_json;
    summarize_random_check(report, submit, task_results, random_check_json);

    json standard_check_json;
    summarize_standard_check(report, submit, task_results, standard_check_json);

    static_check_report static_check;
    gtest_check_report gtest_check;

    report.report = {{"CompileCheck", compile_check_json},
                     {"MemoryCheck", memory_check_json},
                     {"RandomCheck", random_check_json},
                     {"StandardCheck", standard_check_json},
                     {"StaticCheck", static_check},
                     {"GTestCheck", gtest_check}};

    json report_json = report;
    submission_reporter->report(report_json.dump());

    DLOG(INFO) << "MOJ submission report: " << report_json.dump(4);
}

}  // namespace judge::server::moj
