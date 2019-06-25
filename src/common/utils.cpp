#include "common/utils.hpp"
#include <sys/wait.h>
#include <unistd.h>
using namespace std;

namespace std {

std::string to_string(const std::filesystem::path &path) {
    return path.string();
}

}  // namespace std

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

void set_env(const string &key, const string &value, bool replace) {
    setenv(key.c_str(), value.c_str(), replace);
}
