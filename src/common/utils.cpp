#include "common/utils.hpp"
#include <sys/wait.h>
#include <unistd.h>
using namespace std;

int exec_program(const map<string, string> &env, const char **argv) {
    // 使用 POSIX 提供的函数来实现外部程序调用
    pid_t pid;
    switch (pid = fork()) {
        case -1:  // fork 失败
            throw system_error();
        case 0:  // 子进程
            // 避免子进程被终止，要求父进程处理中断信号
            signal(SIGINT, SIG_IGN);
            for (auto &[key, value] : env)
                set_env(key, value);
            execvp(argv[0], (char **)argv);
            _exit(EXIT_FAILURE);
        default:  // 父进程
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status))
                return WEXITSTATUS(status);
            else
                return -1;
    }
    return 0;
}

string get_env(const string &key, const string &def_value) {
    char *result = getenv(key.c_str());
    return !result ? def_value : string(result);
}

void set_env(const string &key, const string &value, bool replace) {
    setenv(key.c_str(), value.c_str(), replace);
}

elapsed_time::elapsed_time() {
    start = chrono::system_clock::now();
}
