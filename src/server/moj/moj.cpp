#include "server/moj/moj.hpp"
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
#include "judge/choice.hpp"
#include "judge/programming.hpp"
#include "server/moj/feedback.hpp"

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;
using namespace ormpp;

const int COMPILE_CHECK_TYPE = 0;
const int MEMORY_CHECK_TYPE = 1;
const int RANDOM_CHECK_TYPE = 2;
const int STANDARD_CHECK_TYPE = 3;
const int STATIC_CHECK_TYPE = 4;
const int GTEST_CHECK_TYPE = 5;

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

static void connect_database(dbng<mysql> &db, const database &dbcfg) {
    db.connect(dbcfg.host.c_str(), dbcfg.user.c_str(), dbcfg.password.c_str(), dbcfg.database.c_str());
}

void configuration::init(const filesystem::path &config_path) {
    if (!filesystem::exists(config_path))
        throw runtime_error("Unable to find configuration file");
    ifstream fin(config_path);
    json config;
    fin >> config;
    config.at("redis").get_to(redis_config);
    redis_server.init(redis_config);

    config.at("database").get_to(dbcfg);
    connect_database(db, dbcfg);

    config.at("choiceSubmissionQueue").get_to(choice_queue);
    config.at("programmingSubmissionQueue").get_to(programming_queue);
    config.at("systemConfig").get_to(system);

    programming_fetcher = make_unique<submission_fetcher>(programming_queue);
    choice_fetcher = make_unique<submission_fetcher>(choice_queue);
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
static void from_json_programming(const json &j, configuration &server, programming_submission &submit) {
    // DLOG(INFO) << "Receive moj submission: " << j.dump(4);

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
    standard->language = "cpp";  // 评测 3.0 没有分开处理选手和标答的编译参数，只能把参数传给标答，并强制标答是 cpp。
    // 硬编码将 c++ 改成 cpp
    if (language == "c++")
        submission->language = "cpp";
    else
        submission->language = language;

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
    compile.get_to(standard->compile_command);

    int memory_limit;
    double time_limit;
    int proc_limit = 10;
    int file_limit = 32768;  // 32M

    const json &limits = config.at("limits").at(language);
    limits.at("memory").get_to(memory_limit);
    memory_limit <<= 10;
    limits.at("time").get_to(time_limit);
    time_limit /= 1000;

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

        if (standard_json.count("output") && standard_json.count("output")) {
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
        }

        submit.standard = move(standard);
    }

    int random_test_times = 0;

    if (config.count("random")) {
        auto random_ptr = make_unique<source_code>(server.exec_mgr);
        const json &random = config.at("random").at(language);
        random.at("language").get_to(random_ptr->language);
        random.at("compile").get_to(random_ptr->compile_command);
        random.at("run_times").get_to(random_test_times);

        if (files.count("random") && random_test_times > 0) {
            const json &random_json = files.at("random");
            auto src_url = random_json.get<vector<string>>();
            // 随机数据生成器的下载地址：FILE_API/problem/<prob_id>/random_source/<filename>
            append(random_ptr->source_files, src_url, moj_url_to_remote_file(server, fmt::format("problem/{}/random_source", submit.prob_id)));

            submit.random = move(random_ptr);
        }
    }

    const json &grading = config.at("grading").at(language);
    vector<int> standard_checks;  // 标准测试的评测点编号
    vector<int> random_checks;    // 随机测试的评测点编号

    // 下面构造测试的顺序很重要，编译测试必须第一个处理，内存测试必须最后处理

    {  // 编译测试
        judge_task testcase;
        testcase.score = grading.count("CompileCheck") ? grading.at("CompileCheck").get<int>() : 0;
        testcase.depends_on = -1;
        testcase.depends_cond = judge_task::depends_condition::ACCEPTED;
        testcase.check_type = COMPILE_CHECK_TYPE;
        testcase.check_script = "compile";
        testcase.is_random = false;
        testcase.time_limit = server.system.time_limit.compile_time_limit;
        testcase.memory_limit = judge::SCRIPT_MEM_LIMIT;
        testcase.file_limit = judge::SCRIPT_FILE_LIMIT;
        testcase.proc_limit = proc_limit;
        testcase.testcase_id = -1;
        submit.judge_tasks.push_back(testcase);
    }

    int random_full_grade = grading.count("RandomCheck") ? grading.at("RandomCheck").get<int>() : 0;
    if (random_test_times > 0 && random_full_grade > 0) {  // 随机测试
        judge_task testcase;
        testcase.score = random_full_grade / random_test_times;
        testcase.depends_on = 0;  // 依赖编译任务
        testcase.depends_cond = judge_task::depends_condition::ACCEPTED;
        testcase.check_type = RANDOM_CHECK_TYPE;
        testcase.check_script = "standard";
        testcase.run_script = "standard";
        testcase.compare_script = "diff-all";
        testcase.is_random = true;
        testcase.time_limit = time_limit;
        testcase.memory_limit = memory_limit;
        testcase.file_limit = file_limit;
        testcase.proc_limit = proc_limit;

        for (int i = 0; i < random_test_times; ++i) {
            testcase.testcase_id = -1;  // 随机测试使用哪个测试数据点是未知的，需要实际运行时决定
            // 将当前的随机测试点编号记录下来，给内存测试依赖
            random_checks.push_back(submit.judge_tasks.size());
            submit.judge_tasks.push_back(testcase);
        }
    }

    int standard_full_grade = grading.count("StandardCheck") ? grading.at("StandardCheck").get<int>() : 0;
    if (!submit.test_data.empty() && standard_full_grade > 0) {  // 标准测试
        judge_task testcase;
        testcase.score = standard_full_grade / submit.test_data.size();
        testcase.depends_on = 0;  // 依赖编译任务
        testcase.depends_cond = judge_task::depends_condition::ACCEPTED;
        testcase.check_type = STANDARD_CHECK_TYPE;
        testcase.check_script = "standard";
        testcase.run_script = "standard";
        testcase.compare_script = "diff-all";
        testcase.is_random = false;
        testcase.time_limit = time_limit;
        testcase.memory_limit = memory_limit;
        testcase.file_limit = file_limit;
        testcase.proc_limit = proc_limit;

        for (size_t i = 0; i < submit.test_data.size(); ++i) {
            testcase.testcase_id = i;
            // 将当前的标准测试点编号记录下来，给内存测试依赖
            standard_checks.push_back(submit.judge_tasks.size());
            submit.judge_tasks.push_back(testcase);
        }
    }

    int static_full_grade = grading.count("StaticCheck") ? grading.at("StaticCheck").get<int>() : 0;
    if (static_full_grade > 0) {  // 静态测试
        judge_task testcase;
        testcase.score = static_full_grade;
        testcase.depends_on = 0;
        testcase.depends_cond = judge_task::depends_condition::ACCEPTED;
        testcase.check_type = STATIC_CHECK_TYPE;
        testcase.check_script = "static";
        testcase.run_script = "standard";
        testcase.compare_script = "diff-all";
        testcase.is_random = false;
        testcase.time_limit = server.system.time_limit.oclint;
        testcase.memory_limit = judge::SCRIPT_MEM_LIMIT;
        testcase.file_limit = judge::SCRIPT_FILE_LIMIT;
        testcase.proc_limit = proc_limit;
        testcase.testcase_id = -1;
        submit.judge_tasks.push_back(testcase);
    }

    int gtest_full_grade = grading.count("GTestCheck") ? grading.at("GTestCheck").get<int>() : 0;
    if (gtest_full_grade > 0) {  // GTest 测试
        judge_task testcase;
        testcase.score = gtest_full_grade;
        testcase.depends_on = 0;  // 依赖编译任务
        testcase.depends_cond = judge_task::depends_condition::ACCEPTED;
        testcase.check_type = GTEST_CHECK_TYPE;
        testcase.check_script = "standard";
        testcase.run_script = "gtest";
        testcase.compare_script = "gtest";
        testcase.is_random = false;
        testcase.time_limit = time_limit;
        testcase.memory_limit = memory_limit;
        testcase.file_limit = file_limit;
        testcase.proc_limit = proc_limit;
        testcase.testcase_id = -1;
        submit.judge_tasks.push_back(testcase);
    }

    {  // 内存测试需要依赖标准测试或随机测试以便可以在标准或随机测试没有 AC 的情况下终止内存测试以加速评测速度
        judge_task testcase;
        testcase.score = grading.count("MemoryCheck") ? grading.at("MemoryCheck").get<int>() : 0;
        testcase.check_type = MEMORY_CHECK_TYPE;
        testcase.check_script = "standard";
        testcase.run_script = "valgrind";
        testcase.compare_script = "valgrind";
        testcase.is_random = submit.judge_tasks.empty();
        testcase.time_limit = server.system.time_limit.valgrind;
        testcase.memory_limit = judge::SCRIPT_MEM_LIMIT;
        testcase.file_limit = judge::SCRIPT_FILE_LIMIT;
        testcase.proc_limit = proc_limit;

        if (testcase.is_random) {
            if (!random_checks.empty()) {  // 如果存在随机测试，则依赖随机测试点的数据
                testcase.score /= random_checks.size();
                for (int &i : random_checks) {
                    testcase.testcase_id = submit.judge_tasks[i].testcase_id;
                    testcase.depends_on = i;
                    testcase.depends_cond = judge_task::depends_condition::ACCEPTED;
                    submit.judge_tasks.push_back(testcase);
                }
            } else if (submit.random) {  // 否则只能生成 10 组随机测试数据
                testcase.score /= 10;
                for (size_t i = 0; i < 10; ++i) {
                    testcase.testcase_id = i;
                    testcase.depends_on = 0;  // 依赖编译任务
                    testcase.depends_cond = judge_task::depends_condition::ACCEPTED;
                    submit.judge_tasks.push_back(testcase);
                }
            }
        } else if (!standard_checks.empty()) {
            testcase.score /= standard_checks.size();
            for (int &i : standard_checks) {
                testcase.testcase_id = submit.judge_tasks[i].testcase_id;
                testcase.depends_on = i;
                testcase.depends_cond = judge_task::depends_condition::ACCEPTED;
                submit.judge_tasks.push_back(testcase);
            }
        }
    }

    submit.sub_type = "programming";
}

struct choice_exam {
    /**
     * @brief 选择题配置版本
     * 仅支持 version 2
     */
    int version;

    /**
     * @brief 选择题提交 id
     */
    int sub_id;

    /**
     * @brief 选择题库 id
     */
    int prob_id;

    /**
     * @brief 选择题类型、评分、标准答案、选手答案集合
     */
    vector<choice_question> questions;
};

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
static void from_json_choice(const json &j, configuration &server, choice_submission &submit) {
    // DLOG(INFO) << "Receive moj choice submission: " << j.dump(4);

    submit.category = server.category();

    int version = 1, sub_id, prob_id;
    if (j.count("version")) j.at("version").get_to(version);
    j.at("sub_id").get_to(sub_id);    // type int
    j.at("prob_id").get_to(prob_id);  // type int
    submit.sub_id = to_string(sub_id);
    submit.prob_id = to_string(prob_id);

    auto &standard = j.at("standard");
    auto &answer = j.at("answer");
    for (json::const_iterator qit = standard.begin(), ait = answer.begin();
         qit != standard.end() && ait != answer.end();
         ++qit, ++ait) {
        choice_question q;

        const json &qobj = *qit;
        const json &aobj = *ait;

        qobj.at("grading").at("max_grade").get_to(q.full_grade);
        if (qobj.at("grading").count("half_grade"))
            qobj.at("grading").at("half_grade").get_to(q.half_grade);
        qobj.at("type").get_to(q.type);
        qobj.at("answer").get_to(q.standard_answer);
        aobj.get_to(q.student_answer);
        submit.questions.push_back(q);
    }

    submit.sub_type = "choice";
}

static bool report_to_server(configuration &server, bool is_complete, const judge_report &report) {
    string report_string = json(report).dump();
    try {
        LOG(INFO) << "MOJ Submission Reporter: inserting submission " << report.sub_id << " into redis";
        std::future<cpp_redis::reply> reply;
        server.redis_server.execute([=, &server, &reply](cpp_redis::client &redis) {
            if (server.redis_config.channel.empty())
                reply = redis.set(to_string(report.sub_id), report_string);
            else
                reply = redis.publish(server.redis_config.channel, report_string);
        });
        cpp_redis::reply msg = reply.get();
        if (!msg.ok()) return false;
        LOG(INFO) << "MOJ Submission Reporter: inserting submission " << report.sub_id << " into redis, reply " << msg;

        if (is_complete) {
            LOG(INFO) << "MOJ Submission Reporter: updating database record of submission " << report.sub_id;
            if (!server.db.ping()) connect_database(server.db, server.dbcfg);
            server.db.execute("UPDATE submission, submission_detail SET submission.grade=?, submission_detail.report=? WHERE submission.sub_id=? AND submission_detail.sub_id=?",
                              report.grade, report.report.dump(), report.sub_id, report.sub_id);
        }
        return true;
    } catch (std::exception &ex) {
        LOG(ERROR) << "MOJ Submission Reporter: unable to report to server: " << ex.what() << ", report: " << endl
                   << report_string;
        return false;
    }
}

bool configuration::fetch_submission(unique_ptr<submission> &submit) {
    AmqpClient::Envelope::ptr_t envelope;
    if (programming_fetcher->fetch(envelope, 0)) {
        auto prog_submit = make_unique<programming_submission>();
        prog_submit->envelope = envelope;
        try {
            from_json_programming(json::parse(envelope->Message()->Body()), *this, *prog_submit.get());
            submit = move(prog_submit);
            return true;
        } catch (exception &ex) {
            programming_fetcher->ack(envelope);
            throw ex;
        }
    }

    if (choice_fetcher->fetch(envelope, 0)) {
        auto choice_submit = make_unique<choice_submission>();
        choice_submit->envelope = envelope;
        try {
            from_json_choice(json::parse(envelope->Message()->Body()), *this, *choice_submit.get());
            submit = move(choice_submit);
            return true;
        } catch (exception &ex) {
            choice_fetcher->ack(envelope);
            throw ex;
        }
    }

    return false;
}

void configuration::summarize_invalid(submission &) {
    throw runtime_error("Invalid submission");
}

static json get_error_report(const judge_task_result &result) {
    error_report report;
    report.message = "SystemError: " + result.error_log;
    return report;
}

/**
 * 执行条件
 * 1. 配置中开启编译检测（编译检测满分大于0）
 * 2. 已存在提交代码的源文件
 */
static bool summarize_compile_check(boost::rational<int> &total_score, programming_submission &submit, json &compile_check_json) {
    compile_check_report compile_check;
    compile_check.full_grade = (int)round(boost::rational_cast<double>(submit.judge_tasks[0].score));
    compile_check.report = submit.results[0].error_log;
    if (submit.results[0].status == status::ACCEPTED) {
        compile_check.grade = compile_check.full_grade;
        compile_check.pass = true;
        total_score += submit.judge_tasks[0].score;
        compile_check_json = compile_check;
    } else if (submit.results[0].status == status::SYSTEM_ERROR) {
        compile_check.grade = 0;
        compile_check_json = get_error_report(submit.results[0]);
    } else {
        compile_check.grade = 0;
        compile_check_json = compile_check;
    }
    return true;
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
static bool summarize_random_check(boost::rational<int> &total_score, programming_submission &submit, json &random_check_json) {
    bool exists = false;
    random_check_report random_check;
    random_check.pass_cases = 0;
    random_check.total_cases = 0;
    boost::rational<int> score, full_score;
    for (size_t i = 0; i < submit.results.size(); ++i) {
        auto &task_result = submit.results[i];
        if (submit.judge_tasks[i].check_type == RANDOM_CHECK_TYPE) {
            full_score += submit.judge_tasks[i].score;
            ++random_check.total_cases;
            if (task_result.status == status::PENDING || task_result.status == status::DEPENDENCY_NOT_SATISFIED) continue;
            exists = true;

            if (task_result.status == status::ACCEPTED) {
                score += submit.judge_tasks[i].score;
                ++random_check.pass_cases;
            } else if (task_result.status == status::PARTIAL_CORRECT) {
                score += task_result.score * submit.judge_tasks[i].score;
            } else if (task_result.status == status::SYSTEM_ERROR ||
                       task_result.status == status::RANDOM_GEN_ERROR ||
                       task_result.status == status::EXECUTABLE_COMPILATION_ERROR ||
                       task_result.status == status::COMPARE_ERROR) {
                random_check_json = get_error_report(task_result);
                return true;
            }

            check_case_report kase;
            kase.result = status_string.at(task_result.status);
            kase.stdin = read_file_content(task_result.data_dir / "input" / "testdata.in");
            kase.stdout = read_file_content(task_result.data_dir / "output" / "testdata.out");
            kase.subout = read_file_content(task_result.run_dir / "run" / "program.out");
            random_check.report.push_back(kase);
        }
    }
    random_check.grade = (int)round(boost::rational_cast<double>(score));
    random_check.full_grade = (int)round(boost::rational_cast<double>(full_score));
    random_check_json = random_check;
    total_score += score;
    return exists;
}

static bool summarize_standard_check(boost::rational<int> &total_score, programming_submission &submit, json &standard_check_json) {
    bool exists = false;
    standard_check_report standard_check;
    standard_check.pass_cases = 0;
    standard_check.total_cases = 0;
    boost::rational<int> score, full_score;
    for (size_t i = 0; i < submit.results.size(); ++i) {
        auto &task_result = submit.results[i];
        if (submit.judge_tasks[i].check_type == STANDARD_CHECK_TYPE) {
            full_score += submit.judge_tasks[i].score;
            ++standard_check.total_cases;
            if (task_result.status == status::PENDING || task_result.status == status::DEPENDENCY_NOT_SATISFIED) continue;
            exists = true;

            if (task_result.status == status::ACCEPTED) {
                score += submit.judge_tasks[i].score;
                ++standard_check.pass_cases;
            } else if (task_result.status == status::PARTIAL_CORRECT) {
                score += task_result.score * submit.judge_tasks[i].score;
            } else if (task_result.status == status::SYSTEM_ERROR ||
                       task_result.status == status::RANDOM_GEN_ERROR ||
                       task_result.status == status::EXECUTABLE_COMPILATION_ERROR ||
                       task_result.status == status::COMPARE_ERROR) {
                standard_check_json = get_error_report(task_result);
                return true;
            }

            check_case_report kase;
            kase.result = status_string.at(task_result.status);
            kase.stdin = read_file_content(task_result.data_dir / "input" / "testdata.in");
            kase.stdout = read_file_content(task_result.data_dir / "output" / "testdata.out");
            kase.subout = read_file_content(task_result.run_dir / "run" / "program.out");
            standard_check.report.push_back(kase);
        }
    }
    standard_check.grade = (int)round(boost::rational_cast<double>(score));
    standard_check.full_grade = (int)round(boost::rational_cast<double>(full_score));
    standard_check_json = standard_check;
    total_score += score;
    return exists;
}

/**
 * 静态检查目前只支持C/C++
 * 采用oclint进行静态检查，用于检测部分代码风格和代码设计问题
 * 
 * 执行条件
 * 1. 配置中开启静态检查（静态检查满分大于 0）
 * 
 * 评分规则
 * oclint评测违规分3个等级：priority 1、priority 2、priority 3
 * 评测代码每违规一个 priority 1 扣 20%，每违规一个 priority 2 扣 10%，违规 priority 3 不扣分。扣分扣至 0 分为止.
 */
static bool summarize_static_check(boost::rational<int> &total_score, programming_submission &submit, json &static_check_json) {
    bool exists = false;
    static_check_report static_check;
    boost::rational<int> score, full_score;
    vector<json> oclint_violations;
    for (size_t i = 0; i < submit.results.size(); ++i) {
        auto &task_result = submit.results[i];
        if (submit.judge_tasks[i].check_type == STATIC_CHECK_TYPE) {
            full_score += submit.judge_tasks[i].score;
            if (task_result.status == status::PENDING || task_result.status == status::DEPENDENCY_NOT_SATISFIED) continue;
            exists = true;

            try {
                json report = json::parse(task_result.report);
                if (report.count("violation"))
                    oclint_violations.push_back(report.at("violation"));
            } catch (std::exception &e) {  // 非法 json 文件
                task_result.status = status::SYSTEM_ERROR;
                task_result.error_log = boost::diagnostic_information(e);
                static_check_json = get_error_report(task_result);
            }

            if (task_result.status == status::ACCEPTED) {
                score += submit.judge_tasks[i].score;
            } else if (task_result.status == status::PARTIAL_CORRECT) {
                score += task_result.score * submit.judge_tasks[i].score;
            } else if (task_result.status == status::SYSTEM_ERROR ||
                       task_result.status == status::RANDOM_GEN_ERROR ||
                       task_result.status == status::EXECUTABLE_COMPILATION_ERROR ||
                       task_result.status == status::COMPARE_ERROR) {
                static_check_json = get_error_report(task_result);
                return true;
            }
        }
    }
    static_check.grade = (int)round(boost::rational_cast<double>(score));
    static_check.full_grade = (int)round(boost::rational_cast<double>(full_score));
    static_check.report["oclintoutput"] = oclint_violations;
    static_check_json = static_check;
    total_score += score;
    return exists;
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
static bool summarize_memory_check(boost::rational<int> &total_score, programming_submission &submit, json &memory_check_json) {
    bool exists = false;
    memory_check_report memory_check;
    memory_check.pass_cases = 0;
    memory_check.total_cases = 0;
    boost::rational<int> score, full_score;
    for (size_t i = 0; i < submit.results.size(); ++i) {
        auto &task_result = submit.results[i];
        if (submit.judge_tasks[i].check_type == MEMORY_CHECK_TYPE) {
            full_score += submit.judge_tasks[i].score;
            ++memory_check.total_cases;
            if (task_result.status == status::PENDING || task_result.status == status::DEPENDENCY_NOT_SATISFIED) continue;
            exists = true;

            if (task_result.status == status::ACCEPTED) {
                score += submit.judge_tasks[i].score;
                ++memory_check.pass_cases;
            } else if (task_result.status == status::WRONG_ANSWER) {
                memory_check_error_report kase;
                try {
                    kase.valgrindoutput = json::parse(task_result.report).at("error");
                } catch (std::exception &e) {  // 非法 json 文件
                    kase.message = boost::diagnostic_information(e) + "\n" + task_result.report;
                }
                kase.stdin = read_file_content(task_result.data_dir / "input" / "testdata.in");
                memory_check.report.push_back(kase);
            } else if (task_result.status == status::TIME_LIMIT_EXCEEDED) {
                memory_check_error_report kase;
                kase.message = "Time limit exceeded";
                kase.stdin = read_file_content(task_result.data_dir / "input" / "testdata.in");
                memory_check.report.push_back(kase);
            } else {
                memory_check_error_report kase;
                kase.message = "Internal Error:\n" + task_result.error_log;
                memory_check.report.push_back(kase);
            }
        }
    }
    memory_check.grade = (int)round(boost::rational_cast<double>(score));
    memory_check.full_grade = (int)round(boost::rational_cast<double>(full_score));
    memory_check_json = memory_check;
    total_score += score;
    return exists;
}

/*
 * @brief GTest 检查报告
 * 
 * @code{json}
 * {
 *   "grade": 0,
 *   "full_grade": 0,
 *   "result": "", // 和 report.crun.result 一致，若 OK，则其他项启用
 *   "error_cases": 0,
 *   "disabled_cases": 0,
 *   "time": 0, // 单位为秒
 *   "report": {
 *     "googletest": [
 *       {
 *         // InstanceName/TestName/CaseName/ParamValue for TEST_P
 *         // TestName/CaseName for OTHERS
 *         "name": "",
 *         "message": "",
 *         "param": "", // For TEST_P only
 *       },
 *     ],
 *     "crun": { // 程序的运行结果，这里感觉很冗余。。
 *         // 评测结果，比如 OK、PR、TL、ML、WA、RE、OL、CE、RF，和 ::result 一致
 *         "result": "OK",
 *         // message 对于评测 4.0 没什么用，原来一般是报告程序运行结束或者传参（传给原来的 sandbox crun 用的）有误
 *         "message": "Program finished running./limit arguments are not int"
 *     },
 *   }
 * }
 * @endcode
 */
static bool summarize_gtest_check(boost::rational<int> &total_score, programming_submission &submit, json &gtest_check_json) {
    bool exists = false;
    boost::rational<int> score, full_score;
    for (size_t i = 0; i < submit.results.size(); ++i) {
        auto &task_result = submit.results[i];
        if (submit.judge_tasks[i].check_type == GTEST_CHECK_TYPE) {
            full_score += submit.judge_tasks[i].score;
            if (task_result.status == status::PENDING || task_result.status == status::DEPENDENCY_NOT_SATISFIED) continue;
            exists = true;

            try {
                // report 的格式参见 exec/compare/gtest/run
                gtest_check_json = json::parse(task_result.report);
                gtest_check_json["result"] = status_string.at(task_result.status);
                json googletest = gtest_check_json.count("report") ? gtest_check_json["report"] : json{};
                gtest_check_json["report"] = {{"googletest", googletest},
                                              {"crun",
                                               {{"result", status_string.at(task_result.status)},
                                                {"message", "Program finished running."}}}};
            } catch (std::exception &e) {  // 非法 json 文件
                task_result.status = status::SYSTEM_ERROR;
                task_result.error_log = boost::diagnostic_information(e) + "\n" + task_result.report;
                gtest_check_json = get_error_report(task_result);
                break;
            }

            if (task_result.status == status::ACCEPTED) {
                score += submit.judge_tasks[i].score;
            } else if (task_result.status == status::PARTIAL_CORRECT) {
                score += task_result.score * submit.judge_tasks[i].score;
            } else if (task_result.status == status::SYSTEM_ERROR ||
                       task_result.status == status::RANDOM_GEN_ERROR ||
                       task_result.status == status::EXECUTABLE_COMPILATION_ERROR ||
                       task_result.status == status::COMPARE_ERROR) {
                gtest_check_json = get_error_report(task_result);
                return true;
            }
        }
    }
    gtest_check_json["grade"] = (int)round(boost::rational_cast<double>(score));
    gtest_check_json["full_grade"] = (int)round(boost::rational_cast<double>(full_score));
    total_score += score;
    return exists;
}

void summarize_programming(configuration &server, programming_submission &submit) {
    judge_report report;
    report.sub_id = boost::lexical_cast<unsigned>(submit.sub_id);
    report.prob_id = boost::lexical_cast<unsigned>(submit.prob_id);
    report.is_complete = submit.finished == submit.judge_tasks.size();
    report.report = {};

    boost::rational<int> total_score;

    if (json compile_check_json; summarize_compile_check(total_score, submit, compile_check_json))
        report.report["CompileCheck"] = compile_check_json;

    if (json memory_check_json; summarize_memory_check(total_score, submit, memory_check_json))
        report.report["MemoryCheck"] = memory_check_json;

    if (json random_check_json; summarize_random_check(total_score, submit, random_check_json))
        report.report["RandomCheck"] = random_check_json;

    if (json standard_check_json; summarize_standard_check(total_score, submit, standard_check_json))
        report.report["StandardCheck"] = standard_check_json;

    if (json static_check_json; summarize_static_check(total_score, submit, static_check_json))
        report.report["StaticCheck"] = static_check_json;

    if (json gtest_check_json; summarize_gtest_check(total_score, submit, gtest_check_json))
        report.report["GTestCheck"] = gtest_check_json;

    report.grade = (int)round(boost::rational_cast<double>(total_score));

    if (report_to_server(server, report.is_complete, report))
        server.programming_fetcher->ack(any_cast<AmqpClient::Envelope::ptr_t>(submit.envelope));

    // DLOG(INFO) << "MOJ submission report: " << report_json.dump(4);
}

void summarize_choice(configuration &server, choice_submission &submit) {
    judge_report report;
    report.grade = 0;
    report.sub_id = boost::lexical_cast<int>(submit.sub_id);
    report.prob_id = boost::lexical_cast<int>(submit.prob_id);
    report.is_complete = true;
    float full_grade = 0;

    vector<float> grades;
    for (auto &q : submit.questions) {
        report.grade += q.grade;
        full_grade += q.full_grade;
        grades.push_back(q.grade);
    }

    // {
    //   "grade": 10,
    //   "report": [2, 2, 2, 2, 2]
    // }
    report.report = {{"grade", report.grade},
                     {"full_grade", full_grade},
                     {"report", grades}};

    if (report_to_server(server, report.is_complete, report))
        server.choice_fetcher->ack(any_cast<AmqpClient::Envelope::ptr_t>(submit.envelope));

    // DLOG(INFO) << "Matrix Course submission report: " << report_json.dump(4);
}

void configuration::summarize(submission &submit) {
    if (submit.sub_type == "programming") {
        summarize_programming(*this, dynamic_cast<programming_submission &>(submit));
    } else if (submit.sub_type == "choice") {
        summarize_choice(*this, dynamic_cast<choice_submission &>(submit));
    } else {
        throw runtime_error("Unrecognized submission type " + submit.sub_type);
    }
}

}  // namespace judge::server::moj
