#include "config.hpp"
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

class MultiLanguageTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        setup_test_environment();
    }

    static void TearDownTestCase() {
    }

    void prepare(programming_submission &prog, executable_manager &exec_mgr, const string &language, const string &filename, const string &source) {
        prog.category = "mock";
        prog.prob_id = "1234";
        prog.sub_id = "12340";
        prog.updated_at = chrono::system_clock::to_time_t(chrono::system_clock::now());
        prog.submission = make_unique<source_code>(exec_mgr);
        prog.submission->language = language;
        prog.submission->source_files.push_back(make_unique<text_asset>(filename, source));

        {
            test_case_data data;
            data.outputs.push_back(make_unique<text_asset>("testdata.out", "hello world\n"));
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
    }
};

TEST_F(MultiLanguageTest, CTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "c", "main.c", R"(
#include <stdio.h>
int main() { printf("hello world\n"); })");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, CppTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "cpp", "main.cpp", R"(
#include <iostream>
int main() { std::cout << "hello world" << std::endl; })");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, Python2Test) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "python2", "main.py", R"(print "hello world")");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, Python3Test) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "python3", "main.py", R"(print("hello world"))");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, PascalTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "pas", "main.pas", R"(
begin
  writeln('hello world');
end.)");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, BashTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "bash", "main.sh", R"(
echo "hello world")");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, GoTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "go", "main.go", R"(
package main
import "fmt"
func main() {
    fmt.Println("hello world")
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, HaskellTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "haskell", "main.hs", R"(
main = putStrLn "hello world")");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, AdaTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "ada", "main.adb", R"(
with Ada.Text_IO;
procedure main is
begin
    Ada.Text_IO.Put_Line("hello world");
end main;)");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, CSharpTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "csharp", "main.cs", R"(using System;
class Program {
    static void Main(string[] args) {
        Console.WriteLine("hello world");
    }
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, FSharpTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "fsharp", "main.fs", R"(open System
[<EntryPoint>]
let main argv =
    printfn "hello world"
    0)");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, VBNetTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "vbnet", "main.vb", R"(Imports System
Module Program
    Sub Main(args As String())
        Console.WriteLine("Hello World!")
    End Sub
End Module)");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, JavaTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "java", "Main.java", R"(
public class Main {
    public static void main(String[] args) {
        System.out.println("hello world");
    }
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, RustTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "rust", "main.rs", R"(
fn main() {
    println!("hello world");
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, FortranTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "fortran95", "main.f95", R"(
program helloworld
implicit none
write (*, '(a)') "hello world"
end program)");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, JavaScriptTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "js", "main.js", R"(console.log("hello world"))");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}

TEST_F(MultiLanguageTest, RubyTest) {
    concurrent_queue<message::client_task> task_queue;
    local_executable_manager exec_mgr(cachedir, execdir);
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, exec_mgr, "ruby", "main.rb", R"(puts 'hello world')");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
}
