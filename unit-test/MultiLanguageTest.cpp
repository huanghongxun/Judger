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
            testcase.compare_script = "diff-all";
            testcase.time_limit = 1;
            testcase.memory_limit = 262144;
            testcase.file_limit = 262144;
            // Java, JavaScript, Go 这类有 GC 的语言会新建很多线程，导致超出 proc_limit，因此这里直接不限制 proc_limit 了
            testcase.proc_limit = -1;
            testcase.testcase_id = 0;
            prog.judge_tasks.push_back(testcase);
        }
    }

    void test(const string &lang, const string &filename, const string &source) {
        concurrent_queue<message::client_task> task_queue;
        local_executable_manager exec_mgr(cachedir, execdir);
        judge::server::mock::configuration mock_judge_server;
        programming_submission prog;
        prog.judge_server = &mock_judge_server;
        prepare(prog, exec_mgr, lang, filename, source);
        programming_judger judger;
        push_submission(judger, task_queue, prog);
        worker_loop(judger, task_queue);
        EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
        EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
    }
};

TEST_F(MultiLanguageTest, CTest) {
    test("c", "main.c", R"(
#include <stdio.h>
int main() { puts("hello world"); })");
}

TEST_F(MultiLanguageTest, CppTest) {
    test("cpp", "main.cpp", R"(
#include <iostream>
int main() { std::cout << "hello world" << std::endl; })");
}

TEST_F(MultiLanguageTest, Python2Test) {
    test("python2", "main.py", R"(print "hello world")");
}

TEST_F(MultiLanguageTest, Python3Test) {
    test("python3", "main.py", R"(print("hello world"))");
}

TEST_F(MultiLanguageTest, PascalTest) {
    test("pas", "main.pas", R"(
begin
  writeln('hello world');
end.)");
}

TEST_F(MultiLanguageTest, BashTest) {
    test("bash", "main.sh", R"(echo "hello world")");
}

TEST_F(MultiLanguageTest, GoTest) {
    test("go", "main.go", R"(package main
import "fmt"
func main() {
    fmt.Println("hello world")
})");
}

TEST_F(MultiLanguageTest, HaskellTest) {
    test("haskell", "main.hs", R"(main = putStrLn "hello world")");
}

TEST_F(MultiLanguageTest, AdaTest) {
    test("ada", "main.adb", R"(
with Ada.Text_IO;
procedure main is
begin
    Ada.Text_IO.Put_Line("hello world");
end main;)");
}

TEST_F(MultiLanguageTest, CSharpTest) {
    test("csharp", "main.cs", R"(using System;
class Program {
    static void Main(string[] args) {
        Console.WriteLine("hello world");
    }
})");
}

TEST_F(MultiLanguageTest, FSharpTest) {
    test("fsharp", "main.fs", R"(open System
[<EntryPoint>]
let main argv =
    printfn "hello world"
    0)");
}

TEST_F(MultiLanguageTest, VBNetTest) {
    test("vbnet", "main.vb", R"(Imports System
Module Program
    Sub Main(args As String())
        Console.WriteLine("Hello World!")
    End Sub
End Module)");
}

TEST_F(MultiLanguageTest, JavaTest) {
    test("java", "Main.java", R"(
public class Main {
    public static void main(String[] args) {
        System.out.println("hello world");
    }
})");
}

TEST_F(MultiLanguageTest, RustTest) {
    test("rust", "main.rs", R"(fn main() { println!("hello world"); })");
}

TEST_F(MultiLanguageTest, FortranTest) {
    test("fortran95", "main.f95", R"(
program helloworld
implicit none
write (*, '(a)') "hello world"
end program)");
}

TEST_F(MultiLanguageTest, JavaScriptTest) {
    test("js", "main.js", R"(console.log("hello world"))");
}

TEST_F(MultiLanguageTest, RubyTest) {
    test("ruby", "main.rb", R"(puts 'hello world')");
}
