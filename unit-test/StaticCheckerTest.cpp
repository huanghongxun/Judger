#include "env.hpp"
#include "gtest/gtest.h"
#include "judge/programming.hpp"
#include "test/mock_judge_server.hpp"
#include "test/worker.hpp"

using namespace std;
using namespace std::filesystem;
using namespace judge;

static path execdir("exec");
static path cachedir("/tmp");

class StaticCheckerTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        setup_test_environment();
    }

    static void TearDownTestCase() {
    }

    void prepare(programming_submission &prog, executable_manager &exec_mgr, const string &source) {
        prog.category = "mock";
        prog.prob_id = "1234";
        prog.sub_id = "12340";
        prog.updated_at = chrono::system_clock::to_time_t(chrono::system_clock::now());
        prog.submission = make_unique<source_code>(exec_mgr);
        prog.submission->language = "cpp";
        prog.submission->source_files.push_back(make_unique<text_asset>("main.cpp", source));
        prog.submission->assist_files.push_back(make_unique<text_asset>("header.hpp", R"(
#ifndef HEADER_H
#define HEADER_H

#include <iostream>

inline void print() {
  std::cout << "Hello, world!" << std::endl;
}

#endif /* HEADER_H */
)"));

        {  // 编译测试
            judge_task testcase;
            testcase.check_script = "compile";
            prog.judge_tasks.push_back(testcase);
        }

        {  // 静态测试
            judge_task testcase;
            testcase.depends_on = 0;  // 依赖编译任务
            testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
            testcase.check_script = "static";
            testcase.run_script = "standard";
            testcase.compare_script = "diff-ign-space";
            testcase.time_limit = 10;
            testcase.memory_limit = 32768;
            testcase.file_limit = 32768;
            testcase.proc_limit = 3;
            prog.judge_tasks.push_back(testcase);
        }
    }
};

TEST_F(StaticCheckerTest, NoWarningTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, R"(#include "header.hpp"
int main() {
  print();
  return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(StaticCheckerTest, Priority3Test) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, R"(#include "header.hpp"
int main() {
  int a;
  print();
  return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(StaticCheckerTest, Priority2Test) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, R"(#include <iostream>
using namespace std;
int main(void) {
  return 0;
  int abcd = 0;
  cout << abcd << endl;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].score, boost::rational<int>(9, 10));
    EXPECT_EQ(prog.results[1].status, status::PARTIAL_CORRECT);
}
