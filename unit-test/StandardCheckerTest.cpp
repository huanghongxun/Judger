#include "gtest/gtest.h"
#include "judge/programming.hpp"
#include "test/worker.hpp"

using namespace std;
using namespace std::filesystem;
using namespace judge;

path execdir("exec");
path cachedir("/tmp");

class StandardCheckerTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
    }

    static void TearDownTestCase() {
    }

    void prepare(programming_submission &prog, executable_manager &exec_mgr, const string &source) {
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

        {  // 标准测试
            judge_task testcase;
            testcase.depends_on = 0;  // 依赖编译任务
            testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
            testcase.check_script = "standard";
            testcase.run_script = "standard";
            testcase.compare_script = "diff-all";
            testcase.time_limit = 0.1;
            testcase.memory_limit = 32768;
            testcase.file_limit = 32768;
            testcase.proc_limit = 3;

            for (size_t i = 0; i < 2; ++i) {
                testcase.testcase_id = i;
                prog.judge_tasks.push_back(testcase);
            }
        }
    }
};

TEST_F(StandardCheckerTest, AcceptedTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    programming_submission prog;
    prepare(prog, exec_mgr, R"(
#include <iostream>
using namespace std;

int main() {
    int a;
    cin >> a;
    cout << a;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[2].status, status::ACCEPTED);
}

TEST_F(StandardCheckerTest, WrongAnswerTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    programming_submission prog;
    prepare(prog, exec_mgr, R"(
#include <iostream>
using namespace std;

int main () {
    int a;
    cin >> a;
    cout << a + 1;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::WRONG_ANSWER);
    EXPECT_EQ(prog.results[2].status, status::WRONG_ANSWER);
}

TEST_F(StandardCheckerTest, PresentationErrorTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    programming_submission prog;
    prepare(prog, exec_mgr, R"(
#include <iostream>
using namespace std;

int main () {
    int a;
    cin >> a;
    cout << a << endl;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::PRESENTATION_ERROR);
    EXPECT_EQ(prog.results[2].status, status::PRESENTATION_ERROR);
}

TEST_F(StandardCheckerTest, CompilationErrorTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    programming_submission prog;
    prepare(prog, exec_mgr, R"(
int main() {
    cin >> a;
    cout << a;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::COMPILATION_ERROR);
    EXPECT_EQ(prog.results[1].status, status::DEPENDENCY_NOT_SATISFIED);
    EXPECT_EQ(prog.results[2].status, status::DEPENDENCY_NOT_SATISFIED);
}

TEST_F(StandardCheckerTest, TimeLimitExceededTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    programming_submission prog;
    prepare(prog, exec_mgr, R"(
int main () {
    int a;
    while (1) a++;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::TIME_LIMIT_EXCEEDED);
    EXPECT_EQ(prog.results[2].status, status::TIME_LIMIT_EXCEEDED);
}

TEST_F(StandardCheckerTest, MemoryLimitExceededTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    programming_submission prog;
    prepare(prog, exec_mgr, R"(
#include <iostream>
using namespace std;

int main () {
    int a = 0;
    double t[1000][1000][1000];
    cin >> a;
    cout << a;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::RUNTIME_ERROR);
    EXPECT_EQ(prog.results[2].status, status::RUNTIME_ERROR);
}

TEST_F(StandardCheckerTest, FloatingPointErrorTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    programming_submission prog;
    prepare(prog, exec_mgr, R"(
#include <iostream>
using namespace std;

int main() {
    int a;
    cin >> a;
    cout << a / (a - a);
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::FLOATING_POINT_ERROR);
    EXPECT_EQ(prog.results[2].status, status::FLOATING_POINT_ERROR);
}

TEST_F(StandardCheckerTest, SegmentationFaultTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    programming_submission prog;
    prepare(prog, exec_mgr, R"(
#include <iostream>
using namespace std;

int main() {
    int *a = nullptr;
    cin >> a;
    cout << a;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::SEGMENTATION_FAULT);
    EXPECT_EQ(prog.results[2].status, status::SEGMENTATION_FAULT);
}

TEST_F(StandardCheckerTest, RuntimeErrorTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    programming_submission prog;
    prepare(prog, exec_mgr, R"(
#include <exception>
int main() {
    throw std::exception("???");
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::RUNTIME_ERROR);
    EXPECT_EQ(prog.results[2].status, status::RUNTIME_ERROR);
}

TEST_F(StandardCheckerTest, RestrictFunctionTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    programming_submission prog;
    prepare(prog, exec_mgr, R"(
#include <cstdlib>
int main() {
    system("cat -");
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[2].status, status::ACCEPTED);
}
