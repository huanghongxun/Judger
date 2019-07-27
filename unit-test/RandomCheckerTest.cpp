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
static const char *RANDOM_CE = R"(int main() { return a; })";
static const char *RANDOM_CTL = "R(#include </dev/random>)";
static const char *RANDOM_TL = R"(int main() { while (1); })";
static const char *RANDOM_RE = R"(#include <iostream>
#include <cmath>
#include <random>
using namespace std;
int main() {
  random_device r;

  default_random_engine e(r());
  uniform_int_distribution<int> uniform_dist(1, 10);
  cout << uniform_dist(e) << " " << uniform_dist(e) << endl;
  int a = 1;
  cout << a / (a - 1) << endl;
  return 0;
})";
static const char *STANDARD_AC = R"(#include <iostream>
using namespace std;
int main() {
    int a, b;
    cin >> a >> b;
    cout << a + b << endl;
    return 0;
})";
static const char *STANDARD_CE = R"(int main() { return a; })";
static const char *STANDARD_CTL = "R(#include </dev/random>)";
static const char *STANDARD_TL = R"(#include <iostream>
using namespace std;
int main() {
    int a, b, ans;
    cin >> a >> b;
    while (ans < 1e9) {
        ans = ans ++;
    }
    cout << a + b << endl;
    return 0;
})";
static const char *STANDARD_RE = R"(#include <stdexcept>
using namespace std;
int main() {
    throw std::invalid_argument("123");
    return 0;
})";

class RandomCheckerTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        setup_test_environment();
    }

    static void TearDownTestCase() {
    }

    void prepare(programming_submission &prog, executable_manager &exec_mgr, const string &random, const string &standard, const string &source) {
        prog.category = "mock";
        prog.prob_id = "1234";
        prog.sub_id = "12340";
        prog.updated_at = chrono::system_clock::to_time_t(chrono::system_clock::now());
        prog.submission = make_unique<source_code>(exec_mgr);
        prog.submission->language = "cpp";
        prog.submission->source_files.push_back(make_unique<text_asset>("main.cpp", source));

        auto standard_ptr = make_unique<source_code>(exec_mgr);
        standard_ptr->language = "cpp";
        standard_ptr->source_files.push_back(make_unique<text_asset>("main.cpp", standard));
        prog.standard = move(standard_ptr);

        auto random_ptr = make_unique<source_code>(exec_mgr);
        random_ptr->language = "cpp";
        random_ptr->source_files.push_back(make_unique<text_asset>("main.cpp", random));
        prog.random = move(random_ptr);

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
            testcase.compare_script = "diff-all";
            testcase.is_random = true;
            testcase.time_limit = 1;
            testcase.memory_limit = 32768;
            testcase.file_limit = 32768;
            testcase.proc_limit = 3;
            prog.judge_tasks.push_back(testcase);
        }
    }
};

#define TEST_TASK(random_source, standard_source, submission_source, compilation_stage, random_stage) \
    do {                                                                                              \
        concurrent_queue<message::client_task> task_queue;                                            \
        local_executable_manager exec_mgr(cachedir, execdir);                                         \
        judge::server::mock::configuration mock_judge_server;                                         \
        programming_submission prog;                                                                  \
        prog.judge_server = &mock_judge_server;                                                       \
        prepare(prog, exec_mgr, random_source, standard_source, submission_source);                   \
        programming_judger judger;                                                                    \
        push_submission(judger, task_queue, prog);                                                    \
        worker_loop(judger, task_queue);                                                              \
        EXPECT_EQ(prog.results[0].status, compilation_stage);                                         \
        EXPECT_EQ(prog.results[1].status, random_stage);                                              \
    } while (0)

TEST_F(RandomCheckerTest, RandomCETest) {
    TEST_TASK(RANDOM_CE, STANDARD_AC, STANDARD_AC, status::EXECUTABLE_COMPILATION_ERROR, status::DEPENDENCY_NOT_SATISFIED);
}

TEST_F(RandomCheckerTest, RandomRETest) {
    TEST_TASK(RANDOM_RE, STANDARD_AC, STANDARD_AC, status::ACCEPTED, status::RANDOM_GEN_ERROR);
}

TEST_F(RandomCheckerTest, RandomTLTest) {
    TEST_TASK(RANDOM_TL, STANDARD_AC, STANDARD_AC, status::ACCEPTED, status::RANDOM_GEN_ERROR);
}

TEST_F(RandomCheckerTest, RandomCTLTest) {
    TEST_TASK(RANDOM_CTL, STANDARD_AC, STANDARD_AC, status::EXECUTABLE_COMPILATION_ERROR, status::DEPENDENCY_NOT_SATISFIED);
}

TEST_F(RandomCheckerTest, StandardCETest) {
    TEST_TASK(RANDOM_AC, STANDARD_CE, STANDARD_AC, status::EXECUTABLE_COMPILATION_ERROR, status::DEPENDENCY_NOT_SATISFIED);
}

TEST_F(RandomCheckerTest, StandardRETest) {
    TEST_TASK(RANDOM_AC, STANDARD_RE, STANDARD_AC, status::ACCEPTED, status::RANDOM_GEN_ERROR);
}

TEST_F(RandomCheckerTest, StandardTLTest) {
    TEST_TASK(RANDOM_AC, STANDARD_TL, STANDARD_AC, status::ACCEPTED, status::RANDOM_GEN_ERROR);
}

TEST_F(RandomCheckerTest, StandardCTLTest) {
    TEST_TASK(RANDOM_AC, STANDARD_CTL, STANDARD_AC, status::EXECUTABLE_COMPILATION_ERROR, status::DEPENDENCY_NOT_SATISFIED);
}

TEST_F(RandomCheckerTest, SubmissionCETest) {
    TEST_TASK(RANDOM_AC, STANDARD_AC, STANDARD_CE, status::COMPILATION_ERROR, status::DEPENDENCY_NOT_SATISFIED);
}

TEST_F(RandomCheckerTest, SubmissionRETest) {
    TEST_TASK(RANDOM_AC, STANDARD_AC, STANDARD_RE, status::ACCEPTED, status::RUNTIME_ERROR);
}

TEST_F(RandomCheckerTest, SubmissionTLTest) {
    TEST_TASK(RANDOM_AC, STANDARD_AC, STANDARD_TL, status::ACCEPTED, status::TIME_LIMIT_EXCEEDED);
}

TEST_F(RandomCheckerTest, SubmissionCTLTest) {
    TEST_TASK(RANDOM_AC, STANDARD_AC, STANDARD_CTL, status::COMPILATION_ERROR, status::DEPENDENCY_NOT_SATISFIED);
}

TEST_F(RandomCheckerTest, AcceptedTest) {
    TEST_TASK(RANDOM_AC, STANDARD_AC, STANDARD_AC, status::ACCEPTED, status::ACCEPTED);
}
