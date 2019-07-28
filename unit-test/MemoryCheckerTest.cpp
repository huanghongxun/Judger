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

static const char *RANDOM_AC = R"(#include <iostream>
#include <cmath>
#include <random>

using namespace std;

int main() {
    random_device r;

    default_random_engine e(r());
    uniform_int_distribution<int> uniform_dist(1, 10);
    cout << uniform_dist(e) << " " << uniform_dist(e) << endl;
    return 0;
})";

static const char *STANDARD_AC = R"(#include <iostream>
int main() {
    int *a = new int;
    std::cin >> *a;
    std::cout << *a << std::endl;
    delete a;
    return 0;
})";

static const char *STANDARD_ONELEAK = R"(#include <iostream>
int main() {
    int *a = new int;
    std::cin >> *a;
    std::cout << *a << std::endl;
    a = nullptr;
    return 0;
})";

static const char *STANDARD_MULTILEAKS = R"(#include <iostream>
#include <algorithm>
int main() {
    int *a = new int;
    int *b = new int;
    std::cin >> *a;
    std::swap(*a, *b);
    std::cout << *b << std::endl;
    a = nullptr;
    b = nullptr;
    return 0;
})";

static const char *STANDARD_TLE = R"(#include <iostream>
#include <thread>
int main() {
    std::this_thread::sleep_for(std::chrono::seconds(6));
    int a;
    std::cin >> a;
    std::cout << a;
    return 0;
})";

static const char *STANDARD_WA = R"(#include <iostream>
int main() {
    int a;
    std::cin >> a;
    std::cout << a + 1;
    return 0;
})";

class MemoryCheckerTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        setup_test_environment();
    }

    static void TearDownTestCase() {
    }

    void prepare_only_memory_check(programming_submission &prog, executable_manager &exec_mgr, const string &source) {
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

    void prepare_with_standard(programming_submission &prog, executable_manager &exec_mgr, const string &source) {
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
            testcase.compare_script = "diff-ign-space";
            testcase.time_limit = 1;
            testcase.memory_limit = 32768;
            testcase.file_limit = 32768;
            testcase.proc_limit = 3;
            testcase.testcase_id = 0;
            prog.judge_tasks.push_back(testcase);
        }

        {  // 内存测试
            judge_task testcase;
            testcase.depends_on = 1;  // 依赖标准测试
            testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
            testcase.check_script = "standard";
            testcase.run_script = "valgrind";
            testcase.compare_script = "valgrind";
            testcase.time_limit = 5;
            testcase.memory_limit = 524288;
            testcase.file_limit = 524288;
            testcase.proc_limit = 3;
            prog.judge_tasks.push_back(testcase);
        }
    }

    void prepare_with_random(programming_submission &prog, executable_manager &exec_mgr, const string &source) {
        prog.category = "mock";
        prog.prob_id = "1234";
        prog.sub_id = "12340";
        prog.updated_at = chrono::system_clock::to_time_t(chrono::system_clock::now());
        prog.submission = make_unique<source_code>(exec_mgr);
        prog.submission->language = "cpp";
        prog.submission->source_files.push_back(make_unique<text_asset>("main.cpp", source));
        auto standard = make_unique<source_code>(exec_mgr);
        standard->language = "cpp";
        standard->source_files.push_back(make_unique<text_asset>("main.cpp", STANDARD_AC));
        prog.standard = move(standard);
        auto random = make_unique<source_code>(exec_mgr);
        random->language = "cpp";
        random->source_files.push_back(make_unique<text_asset>("main.cpp", RANDOM_AC));
        prog.random = move(random);

        {  // 编译测试
            judge_task testcase;
            testcase.check_script = "compile";
            prog.judge_tasks.push_back(testcase);
        }

        {  // 随机测试
            judge_task testcase;
            testcase.depends_on = 0;  // 依赖编译任务
            testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
            testcase.check_script = "standard";
            testcase.run_script = "standard";
            testcase.compare_script = "diff-ign-space";
            testcase.time_limit = 1;
            testcase.memory_limit = 32768;
            testcase.file_limit = 32768;
            testcase.proc_limit = 3;
            testcase.testcase_id = -1;
            testcase.is_random = true;
            prog.judge_tasks.push_back(testcase);
        }

        {  // 内存测试
            judge_task testcase;
            testcase.depends_on = 1;  // 依赖标准测试
            testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
            testcase.check_script = "standard";
            testcase.run_script = "valgrind";
            testcase.compare_script = "valgrind";
            testcase.time_limit = 5;
            testcase.memory_limit = 524288;
            testcase.file_limit = 524288;
            testcase.proc_limit = 3;
            testcase.is_random = true;
            prog.judge_tasks.push_back(testcase);
        }
    }
};

#define TEST_TASK(source, func, stage1, stage2, check)                               \
    do {                                                                             \
        concurrent_queue<message::client_task> task_queue;                           \
        local_executable_manager exec_mgr(cachedir, execdir);                        \
        judge::server::mock::configuration mock_judge_server;                        \
        programming_submission prog;                                                 \
        prog.judge_server = &mock_judge_server;                                      \
        func(prog, exec_mgr, source);                                                \
        programming_judger judger;                                                   \
        push_submission(judger, task_queue, prog);                                   \
        worker_loop(judger, task_queue);                                             \
        EXPECT_EQ(prog.results[0].status, status::ACCEPTED);                         \
        EXPECT_EQ(prog.results[1].status, stage1);                                   \
        EXPECT_EQ(prog.results[2].status, stage2);                                   \
        EXPECT_TRUE(!check || prog.results[1].data_dir == prog.results[2].data_dir); \
    } while (0)

TEST_F(MemoryCheckerTest, MemoryOnlyAcceptedTest) {
    TEST_TASK(STANDARD_AC, prepare_only_memory_check, status::ACCEPTED, status::ACCEPTED, false);
}

TEST_F(MemoryCheckerTest, MemoryOnlyOneLeakTest) {
    TEST_TASK(STANDARD_ONELEAK, prepare_only_memory_check, status::WRONG_ANSWER, status::WRONG_ANSWER, false);
}

TEST_F(MemoryCheckerTest, MemoryOnlyMultiLeaksTest) {
    TEST_TASK(STANDARD_MULTILEAKS, prepare_only_memory_check, status::WRONG_ANSWER, status::WRONG_ANSWER, false);
}

TEST_F(MemoryCheckerTest, MemoryOnlyTimeLimitExceededTest) {
    TEST_TASK(STANDARD_TLE, prepare_only_memory_check, status::TIME_LIMIT_EXCEEDED, status::TIME_LIMIT_EXCEEDED, false);
}

TEST_F(MemoryCheckerTest, DependsOnStandardAcceptedTest) {
    TEST_TASK(STANDARD_AC, prepare_with_standard, status::ACCEPTED, status::ACCEPTED, true);
}

TEST_F(MemoryCheckerTest, DependsOnStandardOneLeakTest) {
    TEST_TASK(STANDARD_ONELEAK, prepare_with_standard, status::ACCEPTED, status::WRONG_ANSWER, true);
}

TEST_F(MemoryCheckerTest, DependsOnStandardMultiLeaksTest) {
    TEST_TASK(STANDARD_MULTILEAKS, prepare_with_standard, status::ACCEPTED, status::WRONG_ANSWER, true);
}

TEST_F(MemoryCheckerTest, DependsOnStandardTimeLimitExceededTest) {
    TEST_TASK(STANDARD_TLE, prepare_with_standard, status::TIME_LIMIT_EXCEEDED, status::DEPENDENCY_NOT_SATISFIED, true);
}

TEST_F(MemoryCheckerTest, DependsOnStandardWrongAnswerTest) {
    TEST_TASK(STANDARD_WA, prepare_with_standard, status::WRONG_ANSWER, status::DEPENDENCY_NOT_SATISFIED, true);
}

TEST_F(MemoryCheckerTest, DependsOnRandomAcceptedTest) {
    TEST_TASK(STANDARD_AC, prepare_with_random, status::ACCEPTED, status::ACCEPTED, true);
}

TEST_F(MemoryCheckerTest, DependsOnRandomOneLeakTest) {
    TEST_TASK(STANDARD_ONELEAK, prepare_with_random, status::ACCEPTED, status::WRONG_ANSWER, true);
}

TEST_F(MemoryCheckerTest, DependsOnRandomMultiLeaksTest) {
    TEST_TASK(STANDARD_MULTILEAKS, prepare_with_random, status::ACCEPTED, status::WRONG_ANSWER, true);
}

TEST_F(MemoryCheckerTest, DependsOnRandomTimeLimitExceededTest) {
    TEST_TASK(STANDARD_TLE, prepare_with_random, status::TIME_LIMIT_EXCEEDED, status::DEPENDENCY_NOT_SATISFIED, true);
}

TEST_F(MemoryCheckerTest, DependsOnRandomWrongAnswerTest) {
    TEST_TASK(STANDARD_WA, prepare_with_random, status::WRONG_ANSWER, status::DEPENDENCY_NOT_SATISFIED, true);
}
