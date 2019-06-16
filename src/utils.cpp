#include "utils.hpp"
#include <sys/wait.h>
#include <unistd.h>
#include <fstream>
using namespace std;

string read_file_content(filesystem::path const &path) {
    ifstream fin(path.string());
    string str((istreambuf_iterator<char>(fin)),
               (istreambuf_iterator<char>()));
    return str;
}

int exec_program(const char **argv) {
    // 使用 POSIX 提供的函数来实现外部程序调用
    pid_t pid;
    switch (pid = fork()) {
        case -1:  // fork 失败
            throw system_error();
        case 0:  // 子进程
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
