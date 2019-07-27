#include <nlohmann/json.hpp>
#include "env.hpp"
#include "gtest/gtest.h"
#include "judge/programming.hpp"
#include "test/mock_judge_server.hpp"
#include "test/worker.hpp"

using namespace std;
using namespace std::filesystem;
using namespace judge;
using namespace nlohmann;

static path code_files("unit-test/gtest/test-code");
static path execdir("exec");
static path cachedir("/tmp");

class GTestCheckerTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        setup_test_environment();
    }

    static void TearDownTestCase() {
    }

    void prepare(programming_submission &prog, executable_manager &exec_mgr, const path &source) {
        prog.category = "mock";
        prog.prob_id = "1234";
        prog.sub_id = "12340";
        prog.updated_at = chrono::system_clock::to_time_t(chrono::system_clock::now());
        prog.submission = make_unique<source_code>(exec_mgr);
        prog.submission->language = "cpp";
        prog.submission->source_files.push_back(make_unique<local_asset>("test.cpp", source / "test.cpp"));
        prog.submission->source_files.push_back(make_unique<local_asset>("adder.cpp", source / "adder.cpp"));
        prog.submission->assist_files.push_back(make_unique<local_asset>("adder.hpp", source / "adder.hpp"));

        {  // 编译测试
            judge_task testcase;
            testcase.score = 20;
            testcase.check_script = "compile";
            prog.judge_tasks.push_back(testcase);
        }

        {  // Google Test
            judge_task testcase;
            testcase.score = 20;
            testcase.depends_on = 0;  // 依赖编译任务
            testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
            testcase.check_script = "standard";
            testcase.run_script = "gtest";
            testcase.compare_script = "gtest";
            testcase.time_limit = 1;
            testcase.memory_limit = 32768;
            testcase.file_limit = 32768;
            testcase.proc_limit = 3;
            prog.judge_tasks.push_back(testcase);
        }
    }
};

#define CHECK_NORMAL_KEY(result)                          \
    do {                                                  \
        ASSERT_FALSE(result["pass_cases"].is_null());     \
        ASSERT_FALSE(result["total_cases"].is_null());    \
        ASSERT_FALSE(result["disabled_cases"].is_null()); \
        ASSERT_FALSE(result["error_cases"].is_null());    \
        ASSERT_FALSE(result["disabled_cases"].is_null()); \
        ASSERT_FALSE(result["time"].is_null());           \
                                                          \
        ASSERT_TRUE(result["error"].is_null());           \
    } while (0)

#define CHECK_ABNORMAL_KEY(result)                       \
    do {                                                 \
        ASSERT_TRUE(result["pass_cases"].is_null());     \
        ASSERT_TRUE(result["total_cases"].is_null());    \
        ASSERT_TRUE(result["disabled_cases"].is_null()); \
        ASSERT_TRUE(result["error_cases"].is_null());    \
        ASSERT_TRUE(result["disabled_cases"].is_null()); \
        ASSERT_TRUE(result["time"].is_null());           \
                                                         \
        ASSERT_TRUE(result["error"].is_null());          \
    } while (0)

TEST_F(GTestCheckerTest, AbnormalTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, code_files / "abnormal");
    prog.judge_tasks[1].run_args.push_back("--gtest_filter=AdderTest.addTest");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::WRONG_ANSWER);
}

TEST_F(GTestCheckerTest, FailureTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, code_files / "failure");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::PARTIAL_CORRECT);

    EXPECT_NO_THROW(json::parse(prog.results[1].report));
    json report = json::parse(prog.results[1].report);
    CHECK_NORMAL_KEY(report);
    EXPECT_EQ(1, report["pass_cases"].get<int>());
    EXPECT_EQ(19, report["total_cases"].get<int>());
    EXPECT_EQ(1, report["disabled_cases"].get<int>());
    EXPECT_EQ(18, report["error_cases"].get<int>());
}

TEST_F(GTestCheckerTest, PassTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, code_files / "pass");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);

    EXPECT_NO_THROW(json::parse(prog.results[1].report));
    json report = json::parse(prog.results[1].report);
    CHECK_NORMAL_KEY(report);
    EXPECT_EQ(10, report["pass_cases"].get<int>());
    EXPECT_EQ(10, report["total_cases"].get<int>());
    EXPECT_EQ(9, report["disabled_cases"].get<int>());
    EXPECT_EQ(0, report["error_cases"].get<int>());
}

TEST_F(GTestCheckerTest, PassTestWithDisabledTests) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, code_files / "pass");
    prog.judge_tasks[1].run_args.push_back("--gtest_also_run_disabled_tests");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);

    EXPECT_NO_THROW(json::parse(prog.results[1].report));
    json report = json::parse(prog.results[1].report);
    CHECK_NORMAL_KEY(report);
    EXPECT_EQ(19, report["pass_cases"].get<int>());
    EXPECT_EQ(19, report["total_cases"].get<int>());
    EXPECT_EQ(0, report["disabled_cases"].get<int>());
    EXPECT_EQ(0, report["error_cases"].get<int>());
}

TEST_F(GTestCheckerTest, FilteredPassTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, code_files / "pass");
    prog.judge_tasks[1].run_args.push_back("--gtest_filter=AdderTest.addTest");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);

    EXPECT_NO_THROW(json::parse(prog.results[1].report));
    json report = json::parse(prog.results[1].report);
    CHECK_NORMAL_KEY(report);
    EXPECT_EQ(1, report["pass_cases"].get<int>());
    EXPECT_EQ(1, report["total_cases"].get<int>());
    EXPECT_EQ(0, report["disabled_cases"].get<int>());
    EXPECT_EQ(0, report["error_cases"].get<int>());
}

TEST_F(GTestCheckerTest, NoCaseTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, code_files / "pass");
    prog.judge_tasks[1].run_args.push_back("--gtest_filter= ");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);

    EXPECT_NO_THROW(json::parse(prog.results[1].report));
    json report = json::parse(prog.results[1].report);
    CHECK_NORMAL_KEY(report);
    EXPECT_EQ(0, report["pass_cases"].get<int>());
    EXPECT_EQ(0, report["total_cases"].get<int>());
    EXPECT_EQ(0, report["disabled_cases"].get<int>());
    EXPECT_EQ(0, report["error_cases"].get<int>());
}

TEST_F(GTestCheckerTest, TimeLimitTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, code_files / "failure");
    prog.judge_tasks[1].run_args.push_back("--gtest_also_run_disabled_tests");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::TIME_LIMIT_EXCEEDED);
}
