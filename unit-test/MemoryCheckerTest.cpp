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

class MemoryCheckerTest : public ::testing::Test {
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

        {
            test_case_data data;
            data.inputs.push_back(make_unique<text_asset>("testdata.in", "1"));
            data.outputs.push_back(make_unique<text_asset>("testdata.out", "1"));
            prog.test_data.push_back(move(data));
        }

        {
            test_case_data data;
            data.inputs.push_back(make_unique<text_asset>("testdata.in", "2"));
            data.outputs.push_back(make_unique<text_asset>("testdata.out", "2"));
            prog.test_data.push_back(move(data));
        }

        {  // 编译测试
            judge_task testcase;
            testcase.check_script = "compile";
            prog.judge_tasks.push_back(testcase);
        }

        {  // 内存测试
            judge_task testcase;
            testcase.depends_on = 0;  // 依赖编译任务
            testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
            testcase.check_script = "standard";
            testcase.run_script = "valgrind";
            testcase.compare_script = "valgrind";
            testcase.time_limit = 5;
            testcase.memory_limit = 524288;
            testcase.file_limit = 524288;
            testcase.proc_limit = 3;

            for (size_t i = 0; i < 2; ++i) {
                testcase.testcase_id = i;
                prog.judge_tasks.push_back(testcase);
            }
        }
    }
};

#define TEST_TASK(submission_source, standard_stage_1, standard_stage_2) \
    do {                                                                 \
        concurrent_queue<message::client_task> task_queue;               \
        local_executable_manager exec_mgr(cachedir, execdir);            \
        judge::server::mock::configuration mock_judge_server;            \
        programming_submission prog;                                     \
        prog.judge_server = &mock_judge_server;                          \
        prepare(prog, exec_mgr, submission_source);                      \
        programming_judger judger;                                       \
        push_submission(judger, task_queue, prog);                       \
        worker_loop(judger, task_queue);                                 \
        EXPECT_EQ(prog.results[0].status, status::ACCEPTED);             \
        EXPECT_EQ(prog.results[1].status, standard_stage_1);             \
        EXPECT_EQ(prog.results[2].status, standard_stage_2);             \
    } while (0)

TEST_F(MemoryCheckerTest, AcceptedTest) {
    TEST_TASK(R"(#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
  int *a = new int;
  cin >> *a;
  cout << *a << endl;
  delete a;
  return 0;
})",
              status::ACCEPTED, status::ACCEPTED);
}

TEST_F(MemoryCheckerTest, OneLeakTest) {
    TEST_TASK(R"(#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
  int *a = new int;
  cin >> *a;
  cout << *a << endl;
  a = nullptr;
  return 0;
})",
              status::WRONG_ANSWER, status::WRONG_ANSWER);
}

TEST_F(MemoryCheckerTest, MultiLeaksTest) {
    TEST_TASK(R"(#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
  int *a = new int;
  int *b = new int;
  b = nullptr;
  cin >> *a;
  cout << *a << endl;
  a = nullptr;
  return 0;
})",
              status::WRONG_ANSWER, status::WRONG_ANSWER);
}

TEST_F(MemoryCheckerTest, TimeLimitExceededTest) {
    TEST_TASK(R"(#include <iostream>
#include <thread>

using namespace std;

int main(void) {
  this_thread::sleep_for(chrono::seconds(6));
  int a;
  cin >> a;
  cout << a;
  return 0;
})",
              status::TIME_LIMIT_EXCEEDED, status::TIME_LIMIT_EXCEEDED);
}
