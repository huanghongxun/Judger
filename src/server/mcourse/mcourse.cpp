#include "server/mcourse/mcourse.hpp"
#include <glog/logging.h>
#include <boost/algorithm/string.hpp>
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
#include "judge/program_output.hpp"
#include "judge/programming.hpp"
#include "server/mcourse/feedback.hpp"

namespace judge::server::mcourse {
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

    config.at("matrix_db").get_to(matrix_dbcfg);
    matrix_db.connect(matrix_dbcfg.host.c_str(), matrix_dbcfg.user.c_str(), matrix_dbcfg.password.c_str(), matrix_dbcfg.database.c_str());

    config.at("monitor_db").get_to(monitor_dbcfg);
    monitor_db.connect(monitor_dbcfg.host.c_str(), monitor_dbcfg.user.c_str(), monitor_dbcfg.password.c_str(), monitor_dbcfg.database.c_str());

    config.at("submission_queue").get_to(sub_queue);
    config.at("systemConfig").get_to(system);
    config.at("host").get_to(host);
    config.at("port").get_to(port);

    sub_fetcher = make_unique<rabbitmq>(sub_queue, false);

    monitor_db.execute("INSERT INTO judge_node_config (host, port, thread_number, is_working, load_factor) VALUES (?, ?, 0, true, 0) ON DUPLICATE KEY UPDATE host=?, port=?, load_factor=0, thread_number=0",
                       host, port, host, port);
}

string configuration::category() const {
    return "mcourse";
}

const executable_manager &configuration::get_executable_manager() const {
    return exec_mgr;
}

static function<asset_uptr(const string &)> moj_url_to_remote_file(configuration &config, const string &subpath) {
    return [&](const string &name) {
        return make_unique<remote_asset>(name, config.system.file_api + "/" + subpath + "/" + name);
    };
}

/**
 * @brief 导入 Matrix 课程系统题目的 config json
 * @code{json}
 * {
 *   "limits": {
 *     "time": 1000,
 *     "memory": 32
 *   },
 *   "random": {
 *     "run_times": 20,
 *     "compile_command": "g++ SOURCE -w -std=c++14 -o random"
 *   },
 *   "grading": {
 *     "google tests": 0,
 *     "memory check": 0,
 *     "random tests": 50,
 *     "static check": 0,
 *     "compile check": 10,
 *     "standard tests": 40,
 *     "google tests detail": {}
 *   },
 *   "language": [
 *     "c++"
 *   ],
 *   "standard": {
 *     "support": [],
 *     "random_source": [
 *       "random.c"
 *     ],
 *     "hidden_support": [],
 *     "standard_input": [
 *       "standard_input0",
 *       "standard_input1",
 *       "standard_input2",
 *       "standard_input3",
 *       "standard_input4",
 *       "standard_input5",
 *       "standard_input6",
 *       "standard_input7",
 *       "standard_input8",
 *       "standard_input9"
 *     ],
 *     "standard_output": [
 *       "standard_output0",
 *       "standard_output1",
 *       "standard_output2",
 *       "standard_output3",
 *       "standard_output4",
 *       "standard_output5",
 *       "standard_output6",
 *       "standard_output7",
 *       "standard_output8",
 *       "standard_output9"
 *     ]
 *   },
 *   "compilers": {
 *     "c": {
 *       "command": "gcc CODE_FILES -g -w -lm -o OUTPUT_PROGRAM",
 *       "version": "default"
 *     },
 *     "c++": {
 *       "command": "g++ CODE_FILES -g -w -lm -std=c++14 -o OUTPUT_PROGRAM",
 *       "version": "default"
 *     },
 *     "clang": {
 *       "command": "clang CODE_FILES -g -w -lm -o OUTPUT_PROGRAM",
 *       "version": "default"
 *     },
 *     "clang++": {
 *       "command": "clang++ CODE_FILES -g -w -lm -std=c++11 -o OUTPUT_PROGRAM",
 *       "version": "default"
 *     }
 *   },
 *   "exec_flag": "--gtest_output=xml:/tmp/Result.xml",
 *   "submission": [
 *     "source.cpp"
 *   ],
 *   "entry_point": "standard_main.exe",
 *   "output_program": "main.exe",
 *   "standard_score": 100,
 *   "google tests info": {},
 *   "standard_language": "c++"
 * }
 * @endcode
 * 
 * detail:
 * {"language": "c++"} 或者是 {"is_standard": 1}
 */
void from_json_programming(const json &config, const json &detail, judge_request::submission_type sub_type, configuration &server, programming_submission &submit) {
    unique_ptr<source_code> submission = make_unique<source_code>(server.exec_mgr);
    unique_ptr<source_code> standard = make_unique<source_code>(server.exec_mgr);

    // compiler 项因为我们忽略课程系统的编译参数，因此也忽略掉
    // standard_score 看起来像满分，但我们可以根据 grading 把满分算出来，所以不知道这个有啥用
    // FIXME: exec_flag, entry_point, output_program, standard_score 项暂时忽略掉

    // 对于老师提交，我们通过将 submission 和 standard 设置完全一样来进行测试
    // 这样标准测试可以正常测试数据，随机测试也可以因为 RANDOM_GEN_ERROR 返回错误

    standard->language = get_value<string>(config, "standard_language");
    if (standard->language == "c++")
        standard->language = "cpp";

    // 对于学生提交，我们从 detail 中获得当前学生提交的编程语言
    if (sub_type == judge_request::submission_type::student) {
        if (detail.count("language")) {
            submission->language = get_value<string>(detail, "language");
        } else {
            auto language = get_value<vector<string>>(config, "language");
            if (language.empty()) throw invalid_argument("submission.config.language is empty");
            submission->language = language[0];
        }

        if (submission->language == "c++")
            submission->language = "cpp";
    } else {
        submission->language = standard->language;
    }

    int memory_limit = get_value<int>(config, "limits", "memory") << 10;
    double time_limit = get_value<int>(config, "limits", "time") / 1000;
    int proc_limit = 10;
    int file_limit = 1 << 18;  // 256M

    auto &standard_json = config.at("standard");

    if (standard_json.count("support")) {
        auto support = standard_json.at("support").get<vector<string>>();
        // 依赖文件的下载地址：FILE_API/problem/<prob_id>/support/<filename>
        append(submission->source_files, support, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));
        append(standard->source_files, support, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));
    }

    if (standard_json.count("hidden_support")) {
        auto hidden_support = standard_json.at("hidden_support").get<vector<string>>();
        // 依赖文件的下载地址：FILE_API/problem/<prob_id>/support/<filename>
        append(submission->source_files, hidden_support, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));
        append(standard->source_files, hidden_support, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));
    }

    {  // submission
        auto src_url = config.at("submission").get<vector<string>>();
        // 选手提交的下载地址：FILE_API/submission/<sub_id>/<filename>
        if (sub_type == judge_request::submission_type::student)
            append(submission->source_files, src_url, moj_url_to_remote_file(server, fmt::format("submission/{}", submit.sub_id)));
        else
            append(submission->source_files, src_url, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));
        // 标准程序的下载地址：FILE_API/problem/<prob_id>/support/<filename>
        append(standard->source_files, src_url, moj_url_to_remote_file(server, fmt::format("problem/{}/support", submit.prob_id)));
    }

    if (standard_json.count("standard_input") && standard_json.count("standard_output")) {
        auto input_url = standard_json.at("standard_input").get<vector<string>>();
        auto output_url = standard_json.at("standard_output").get<vector<string>>();
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

    submit.submission = move(submission);
    submit.standard = move(standard);

    int random_test_times = 0;

    const json &grading = config.at("grading");
    if (config.count("random") && get_value_def(grading, 0, "random tests") > 0) {  // 随机数据生成器
        auto random_ptr = make_unique<source_code>(server.exec_mgr);

        auto &random_json = config.at("random");
        random_test_times = get_value<int>(random_json, "run_times");

        string compile_command;
        if (random_json.count("complie_command"))
            compile_command = random_json.at("complie_command").get<string>();
        else
            compile_command = random_json.at("compile_command").get<string>();

        if (boost::algorithm::starts_with(compile_command, "gcc") ||
            boost::algorithm::starts_with(compile_command, "g++") ||
            boost::algorithm::starts_with(compile_command, "clang"))
            random_ptr->language = "cpp";
        else
            random_ptr->language = compile_command;

        if (standard_json.count("random_source")) {
            auto src_url = standard_json.at("random_source").get<vector<string>>();
            // 随机数据生成器的下载地址：FILE_API/problem/<prob_id>/random_source/<filename>
            append(random_ptr->source_files, src_url, moj_url_to_remote_file(server, fmt::format("problem/{}/random_source", submit.prob_id)));
            submit.random = move(random_ptr);
        }
    }

    vector<int> standard_checks;  // 标准测试的评测点编号
    vector<int> random_checks;    // 随机测试的评测点编号

    {  // 编译测试
        judge_task testcase;
        testcase.score = get_value_def(grading, 0, "compile check");
        testcase.depends_on = -1;
        testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
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

    int random_full_grade = get_value_def(grading, 0, "random tests");
    if (random_full_grade > 0 && random_test_times > 0) {  // 随机测试
        judge_task testcase;
        testcase.score = random_full_grade / random_test_times;
        testcase.depends_on = 0;  // 依赖编译任务
        testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
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

    int standard_full_grade = get_value_def(grading, 0, "standard tests");
    if (!submit.test_data.empty() && standard_full_grade > 0) {  // 标准测试
        judge_task testcase;
        testcase.score = standard_full_grade / submit.test_data.size();
        testcase.depends_on = 0;  // 依赖编译任务
        testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
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

    int static_full_grade = get_value_def(grading, 0, "static check");
    if (static_full_grade > 0) {  // 静态测试
        judge_task testcase;
        testcase.score = static_full_grade;
        testcase.depends_on = 0;
        testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
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

    int gtest_full_grade = get_value_def(grading, 0, "google tests");
    if (gtest_full_grade > 0) {  // GTest 测试
        judge_task testcase;
        testcase.score = gtest_full_grade;
        testcase.depends_on = 0;  // 依赖编译任务
        testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
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

    int memory_full_grade = get_value_def(grading, 0, "memory check");
    if (memory_full_grade > 0) {  // 内存测试需要依赖标准测试或随机测试以便可以在标准或随机测试没有 AC 的情况下终止内存测试以加速评测速度
        judge_task testcase;
        testcase.score = memory_full_grade;
        testcase.check_type = MEMORY_CHECK_TYPE;
        testcase.check_script = "standard";
        testcase.run_script = "valgrind";
        testcase.compare_script = "valgrind";
        if (submit.submission)
            submit.submission->compile_command.push_back("-g");
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
                    testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
                    submit.judge_tasks.push_back(testcase);
                }
            } else if (submit.random) {  // 否则只能生成 10 组随机测试数据
                testcase.score /= 10;
                for (size_t i = 0; i < 10; ++i) {
                    testcase.testcase_id = i;
                    testcase.depends_on = 0;  // 依赖编译任务
                    testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
                    submit.judge_tasks.push_back(testcase);
                }
            }
        } else if (!standard_checks.empty()) {
            testcase.score /= standard_checks.size();
            for (int &i : standard_checks) {
                testcase.testcase_id = submit.judge_tasks[i].testcase_id;
                testcase.depends_on = i;
                testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
                submit.judge_tasks.push_back(testcase);
            }
        }
    }

    submit.sub_type = "programming";
}

void from_json_choice(const json &config, const json &detail, judge_request::submission_type, configuration &, choice_submission &submit) {
    auto &standard = config.at("questions");
    auto &answer = detail.at("answers");

    int id = 0;
    for (json::const_iterator qit = standard.begin(), ait = answer.begin();
         qit != standard.end() && ait != answer.end();
         ++qit, ++ait, ++id) {
        choice_question q;

        const json &qobj = *qit;
        const json &aobj = *ait;

        if (qobj.at("id").get<int>() != id) throw invalid_argument("question id is not continuous");
        if (aobj.at("question_id").get<int>() != id) throw invalid_argument("question id is not continuous");
        qobj.at("grading").at("max_grade").get_to(q.full_grade);
        if (qobj.at("grading").count("half_grade"))
            qobj.at("grading").at("half_grade").get_to(q.half_grade);
        qobj.at("choice_type").get_to(q.type);
        qobj.at("standard_answer").get_to(q.standard_answer);
        aobj.at("choice_id").get_to(q.student_answer);
        submit.questions.push_back(q);
    }

    submit.sub_type = "choice";
}

void from_json_program_output(const json &config, const json &detail, judge_request::submission_type, configuration &, program_output_submission &submit) {
    vector<string> standard_answers, student_answers;
    vector<int> scores;
    config.at("standard_answers").get_to(standard_answers);
    config.at("answer_scores").get_to(scores);
    detail.at("student_answers").get_to(student_answers);

    for (size_t i = 0; i < standard_answers.size() && i < scores.size() && i < student_answers.size(); ++i) {
        program_output_question q;
        q.full_grade = scores[i];
        q.standard_answer = standard_answers[i];
        q.student_answer = student_answers[i];
        submit.questions.push_back(q);
    }

    submit.sub_type = "output";
}

static bool report_to_server(configuration &server, bool is_complete, const judge_report &report) {
    try {
        LOG(INFO) << "Matrix Course Submission Reporter: updating submission " << report.sub_id;
        server.matrix_db.execute("UPDATE submission SET report=? WHERE sub_id=?",
                                 report.report.dump(), report.sub_id);
        if (is_complete) {
            LOG(INFO) << "Matrix Course Submission Reporter: updating database record of submission " << report.sub_id;
            if (!server.matrix_db.ping()) connect_database(server.matrix_db, server.matrix_dbcfg);
            server.matrix_db.execute("UPDATE submission SET grade=?, report=? WHERE sub_id=?",
                                     report.grade, report.report.dump(), report.sub_id);
        }
        return true;
    } catch (std::exception &ex) {
        LOG(ERROR) << "Matrix Course Submission Reporter: unable to report to server: " << ex.what();
        return false;
    }
}

static void from_json_mcourse(AmqpClient::Envelope::ptr_t envelope, const json &j, configuration &server, unique_ptr<submission> &submit) {
    DLOG(INFO) << "Receive matrix course submission: " << j.dump(4);

    judge_request request = j;

    try {
        switch (request.prob_type) {
            case judge_request::programming: {
                auto prog_submit = make_unique<programming_submission>();
                prog_submit->category = server.category();
                prog_submit->sub_id = to_string(request.sub_id);
                prog_submit->prob_id = to_string(request.prob_id);
                prog_submit->envelope = envelope;
                prog_submit->config = request.config;

                from_json_programming(request.config, request.detail, request.sub_type, server, *prog_submit.get());
                submit = move(prog_submit);
            } break;
            case judge_request::choice: {
                auto choice_submit = make_unique<choice_submission>();
                choice_submit->category = server.category();
                choice_submit->sub_id = to_string(request.sub_id);
                choice_submit->prob_id = to_string(request.prob_id);
                choice_submit->envelope = envelope;
                choice_submit->config = request.config;

                from_json_choice(request.config, request.detail, request.sub_type, server, *choice_submit.get());
                submit = move(choice_submit);
            } break;
            case judge_request::program_blank_filling: {
                // TODO:
            } break;
            case judge_request::output: {
                auto prog_submit = make_unique<program_output_submission>();
                prog_submit->category = server.category();
                prog_submit->sub_id = to_string(request.sub_id);
                prog_submit->prob_id = to_string(request.prob_id);
                prog_submit->envelope = envelope;
                prog_submit->config = request.config;

                from_json_program_output(request.config, request.detail, request.sub_type, server, *prog_submit.get());
                submit = move(prog_submit);
            } break;
            default: {
                throw runtime_error("Unrecognized problem type");
            } break;
        }

        // 标记当前 submission 正在评测
        server.matrix_db.execute("UPDATE submission SET grade=-1 WHERE sub_id=?", request.sub_id);
    } catch (exception &e) {
        judge_report report;
        report.sub_id = to_string(request.sub_id);
        report.prob_id = to_string(request.prob_id);
        report.grade = 0;
        report.report = "Invalid submission";
        report_to_server(server, true, report);
        throw e;
    }
}

bool configuration::fetch_submission(unique_ptr<submission> &submit) {
    AmqpClient::Envelope::ptr_t envelope;
    if (sub_fetcher->fetch(envelope, 0)) {
        try {
            from_json_mcourse(envelope, json::parse(envelope->Message()->Body()), *this, submit);
            return true;
        } catch (exception &ex) {
            sub_fetcher->ack(envelope);
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
    int full_grade = (int)round(boost::rational_cast<double>(submit.judge_tasks[0].score));
    compile_check.cont = false;
    compile_check.message = submit.results[0].error_log;
    if (!utf8_check_is_valid(compile_check.message))
        compile_check.message = "Not UTF-8 encoding";
    if (submit.results[0].status == status::ACCEPTED) {
        compile_check.cont = true;
        compile_check.grade = full_grade;
        compile_check.message = "pass";
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
    boost::rational<int> score;
    for (size_t i = 0; i < submit.results.size(); ++i) {
        auto &task_result = submit.results[i];
        if (submit.judge_tasks[i].check_type == RANDOM_CHECK_TYPE) {
            if (task_result.status == status::PENDING || task_result.status == status::DEPENDENCY_NOT_SATISFIED) continue;
            exists = true;

            if (task_result.status == status::ACCEPTED) {
                score += submit.judge_tasks[i].score;
                continue;
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
            kase.timeused = task_result.run_time * 1000;
            kase.memoryused = task_result.memory_used >> 10;
            kase.stdin = read_file_content(task_result.data_dir / "input" / "testdata.in");
            kase.standard_stdout = read_file_content(task_result.data_dir / "output" / "testdata.out");
            kase.stdout = read_file_content(task_result.run_dir / "run" / "program.out");
            random_check.cases.push_back(kase);
        }
    }
    random_check.grade = (int)round(boost::rational_cast<double>(score));
    random_check_json = random_check;
    total_score += score;
    return exists;
}

static bool summarize_standard_check(boost::rational<int> &total_score, programming_submission &submit, json &standard_check_json) {
    bool exists = false;
    standard_check_report standard_check;
    boost::rational<int> score;
    for (size_t i = 0; i < submit.results.size(); ++i) {
        auto &task_result = submit.results[i];
        if (submit.judge_tasks[i].check_type == STANDARD_CHECK_TYPE) {
            if (task_result.status == status::PENDING || task_result.status == status::DEPENDENCY_NOT_SATISFIED) continue;
            exists = true;

            if (task_result.status == status::ACCEPTED) {
                score += submit.judge_tasks[i].score;
                continue;
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
            kase.timeused = task_result.run_time * 1000;
            kase.memoryused = task_result.memory_used >> 10;
            kase.stdin = read_file_content(task_result.data_dir / "input" / "testdata.in");
            kase.standard_stdout = read_file_content(task_result.data_dir / "output" / "testdata.out");
            kase.stdout = read_file_content(task_result.run_dir / "run" / "program.out");
            standard_check.cases.push_back(kase);
        }
    }
    standard_check.grade = (int)round(boost::rational_cast<double>(score));
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
    static_check.cont = false;
    boost::rational<int> score;
    for (size_t i = 0; i < submit.results.size(); ++i) {
        auto &task_result = submit.results[i];
        if (submit.judge_tasks[i].check_type == STATIC_CHECK_TYPE) {
            if (task_result.status == status::PENDING || task_result.status == status::DEPENDENCY_NOT_SATISFIED) continue;
            exists = true;

            try {
                static_check.report = json::parse(task_result.report);
            } catch (std::exception &e) {  // 非法 json 文件
                task_result.status = status::SYSTEM_ERROR;
                static_check.report = task_result.error_log = boost::diagnostic_information(e) + "\n" + task_result.report;
            }

            static_check.cont = true;
            if (task_result.status == status::ACCEPTED) {
                score += submit.judge_tasks[i].score;
            } else if (task_result.status == status::PARTIAL_CORRECT) {
                score += task_result.score * submit.judge_tasks[i].score;
            } else if (task_result.status == status::SYSTEM_ERROR ||
                       task_result.status == status::RANDOM_GEN_ERROR ||
                       task_result.status == status::EXECUTABLE_COMPILATION_ERROR ||
                       task_result.status == status::COMPARE_ERROR) {
                static_check.grade = 0;
                static_check.cont = false;
                static_check.report = task_result.error_log;
                static_check_json = static_check;
                return true;
            }
        }
    }
    static_check.grade = (int)round(boost::rational_cast<double>(score));
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
    boost::rational<int> score, full_score;
    for (size_t i = 0; i < submit.results.size(); ++i) {
        auto &task_result = submit.results[i];
        if (submit.judge_tasks[i].check_type == MEMORY_CHECK_TYPE) {
            full_score += submit.judge_tasks[i].score;
            if (task_result.status == status::PENDING || task_result.status == status::DEPENDENCY_NOT_SATISFIED) continue;
            exists = true;

            if (task_result.status == status::ACCEPTED) {
                score += submit.judge_tasks[i].score;
            } else if (task_result.status == status::WRONG_ANSWER) {
                memory_check_error_report kase;
                try {
                    kase.valgrindoutput = json::parse(task_result.report);
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
    memory_check_json = memory_check;
    total_score += score;
    return exists;
}

static bool summarize_gtest_check(boost::rational<int> &total_score, programming_submission &submit, json &gtest_check_json) {
    bool exists = false;
    gtest_check_report gtest_check;
    boost::rational<int> score;
    json config = any_cast<json>(submit.config);
    gtest_check.info = config.at("google tests info");
    for (size_t i = 0; i < submit.results.size(); ++i) {
        auto &task_result = submit.results[i];
        if (submit.judge_tasks[i].check_type == GTEST_CHECK_TYPE) {
            if (task_result.status == status::PENDING || task_result.status == status::DEPENDENCY_NOT_SATISFIED) continue;
            exists = true;

            score = submit.judge_tasks[i].score;

            try {
                // report 格式参见 exec/compare/gtest/run
                json report = json::parse(task_result.report);
                if (report.count("report") && report.at("report").is_array()) {
                    json cases = report.at("report");
                    for (const json &testcase : cases) {
                        if (!testcase.count("case") || !config.at("grading").count("google tests detail") || !config.at("grading").at("google tests detail").count(testcase.at("case").get<string>())) continue;
                        int grade = config.at("grading").at("google tests detail").at(testcase.at("case").get<string>()).get<int>();
                        score -= grade;
                        gtest_check.failed_cases[testcase.at("case").get<string>()] = grade;
                    }
                }
            } catch (std::exception &e) {  // 非法 json 文件
                score = 0;
                gtest_check.error_message = boost::diagnostic_information(e) + "\n" + task_result.report;
                break;
            }

            if (task_result.status == status::ACCEPTED ||
                task_result.status == status::PARTIAL_CORRECT ||
                task_result.status == status::WRONG_ANSWER) {
            } else if (task_result.status == status::SYSTEM_ERROR ||
                       task_result.status == status::RANDOM_GEN_ERROR ||
                       task_result.status == status::EXECUTABLE_COMPILATION_ERROR ||
                       task_result.status == status::COMPARE_ERROR) {
                score = 0;
                gtest_check.error_message = task_result.error_log;
                break;
            } else {
                score = 0;
                gtest_check.error_message = status_string.at(task_result.status);
            }
        }
    }
    gtest_check.grade = (int)round(boost::rational_cast<double>(score));
    gtest_check_json = gtest_check;
    total_score += score;
    return exists;
}

void summarize_programming(configuration &server, programming_submission &submit) {
    judge_report report;
    report.sub_id = submit.sub_id;
    report.prob_id = submit.prob_id;
    report.is_complete = submit.finished == submit.judge_tasks.size();
    report.report = {};

    boost::rational<int> total_score;

    if (json compile_check_json; summarize_compile_check(total_score, submit, compile_check_json))
        report.report["compile check"] = compile_check_json;

    if (json memory_check_json; summarize_memory_check(total_score, submit, memory_check_json))
        report.report["memory check"] = memory_check_json;

    if (json random_check_json; summarize_random_check(total_score, submit, random_check_json))
        report.report["random tests"] = random_check_json;

    if (json standard_check_json; summarize_standard_check(total_score, submit, standard_check_json))
        report.report["standard tests"] = standard_check_json;

    if (json static_check_json; summarize_static_check(total_score, submit, static_check_json))
        report.report["static check"] = static_check_json;

    if (json gtest_check_json; summarize_gtest_check(total_score, submit, gtest_check_json))
        report.report["google tests"] = gtest_check_json;

    report.grade = (int)round(boost::rational_cast<double>(total_score));

    if (report_to_server(server, report.is_complete, report))
        server.sub_fetcher->ack(any_cast<AmqpClient::Envelope::ptr_t>(submit.envelope));

    // DLOG(INFO) << "Matrix Course submission report: " << report_json.dump(4);
}

void summarize_choice(configuration &server, choice_submission &submit) {
    judge_report report;
    report.grade = 0;
    report.sub_id = submit.sub_id;
    report.prob_id = submit.prob_id;
    report.is_complete = true;
    json detail = json::array();
    int qid = 0;
    for (auto &q : submit.questions) {
        report.grade += q.grade;
        detail += {{"grade", q.grade},
                   {"question_id", qid++},
                   {"student_answer", q.student_answer},
                   {"standard_answer", q.standard_answer}};
    }

    report.report = {{"report", detail}};

    if (report_to_server(server, report.is_complete, report))
        server.sub_fetcher->ack(any_cast<AmqpClient::Envelope::ptr_t>(submit.envelope));

    // DLOG(INFO) << "Matrix Course choice submission report: " << report_json.dump(4);
}

void summarize_program_output(configuration &server, program_output_submission &submit) {
    judge_report report;
    report.grade = 0;
    report.sub_id = submit.sub_id;
    report.prob_id = submit.prob_id;
    report.is_complete = true;

    vector<double> grade_detail;
    for (auto &q : submit.questions) {
        report.grade += q.grade;
        grade_detail.push_back(q.grade);
    }

    report.report = {{"grade", report.grade},
                     {"grade_detail", grade_detail}};

    if (report_to_server(server, report.is_complete, report))
        server.sub_fetcher->ack(any_cast<AmqpClient::Envelope::ptr_t>(submit.envelope));

    // DLOG(INFO) << "Matrix Course program output submission report: " << report_json.dump(4);
}

void configuration::summarize(submission &submit) {
    if (submit.sub_type == "programming") {
        summarize_programming(*this, dynamic_cast<programming_submission &>(submit));
    } else if (submit.sub_type == "choice") {
        summarize_choice(*this, dynamic_cast<choice_submission &>(submit));
    } else if (submit.sub_type == "program_blank_filling") {
    } else if (submit.sub_type == "output") {
        summarize_program_output(*this, dynamic_cast<program_output_submission &>(submit));
    } else {
        throw runtime_error("Unrecognized submission type " + submit.sub_type);
    }
}

void configuration::report_worker_state(int worker_id, worker_state state) {
    switch (state) {
        case worker_state::START:
            monitor_db.execute("INSERT INTO judge_worker_status (host, worker_id, worker_type, is_running, worker_stage) VALUES (?, ?, 'Universal', true, 'idle') ON DUPLICATE KEY UPDATE is_running=true, worker_stage='idle', worker_type='Universal'",
                               host, worker_id);
            monitor_db.execute("UPDATE judge_node_config SET load_factor=load_factor+1 WHERE host=?",
                               host);
            break;
        case worker_state::JUDGING:
            monitor_db.execute("UPDATE judge_worker_status SET worker_stage='judging' WHERE host=? AND worker_id=?",
                               host, worker_id);
            monitor_db.execute("UPDATE judge_node_config SET thread_number=thread_number+1 WHERE host=?",
                               host);
            break;
        case worker_state::IDLE:
            monitor_db.execute("UPDATE judge_worker_status SET worker_stage='idle' WHERE host=? AND worker_id=?",
                               host, worker_id);
            monitor_db.execute("UPDATE judge_node_config SET thread_number=thread_number-1 WHERE host=?",
                               host);
            break;
        case worker_state::STOPPED:
            monitor_db.execute("UPDATE judge_worker_status SET is_running=false, worker_stage='down' WHERE host=? AND worker_id=?",
                               host, worker_id);
            monitor_db.execute("UPDATE judge_node_config SET load_factor=load_factor-1 WHERE host=?",
                               host);
            break;
        case worker_state::CRASHED:
            monitor_db.execute("UPDATE judge_worker_status SET is_running=false, worker_stage='crashed' WHERE host=? AND worker_id=?",
                               host, worker_id);
            monitor_db.execute("UPDATE judge_node_config SET thread_number=thread_number-1, load_factor=load_factor-1 WHERE host=?",
                               host);
            break;
    }
}

}  // namespace judge::server::mcourse
