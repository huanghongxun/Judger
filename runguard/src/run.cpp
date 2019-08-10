#include "run.hpp"
#include <fcntl.h>
#include <fmt/core.h>
#include <glog/logging.h>
#include <math.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>
#include <fstream>
#include <iostream>
#include <system_error>
#include "cgroup.hpp"
#include "limits.hpp"
#include "runguard_options.hpp"
#include "system.hpp"
#include "utils.hpp"

using namespace std;

const struct timespec killdelay = {0, 100000000L};  // 0.1s

const int BUF_SIZE = 4096;

const int PIPE_IN = 1;
const int PIPE_OUT = 0;

const int TIMELIMIT_SOFT = 1;
const int TIMELIMIT_HARD = 2;
const int TIMELIMIT_ALL = 3;
int walllimit = 0, cpulimit = 0;

ofstream metafile;
int child_pid = -1;
int efd = -1;
static volatile sig_atomic_t received_SIGCHLD = 0;
static volatile sig_atomic_t received_signal = -1;

template <typename... Args>
void error(int err, Args&&... args) {
    throw system_error(err, system_category(), fmt::format(args...));
}

template <typename T>
void append_meta(const char* key, T message) {
    if (!metafile) return;
    metafile << key << ": " << message << endl;
}

void runguard_terminate_handler() {
    sigset_t sigs;
    /*
	 * Make sure the signal handler for these (terminate()) does not
	 * interfere, we are exiting now anyway.
	 */
    sigaddset(&sigs, SIGALRM);
    sigaddset(&sigs, SIGTERM);
    sigprocmask(SIG_BLOCK, &sigs, nullptr);

    exception_ptr cur = current_exception();
    try {
        if (cur) {
            rethrow_exception(cur);
        }
    } catch (const exception& e) {
        cerr << e.what() << endl;

        append_meta("internal-error", e.what());
    } catch (...) {
        cerr << "Unknown exception occurred" << endl;
    }

    /* Make sure that all children are killed before terminating */
    if (child_pid > 0) {
        LOG(INFO) << "sending SIGKILL";
        if (kill(-child_pid, SIGKILL) != 0 && errno != ESRCH) {
            LOG(ERROR) << "unable to send SIGKILL to children while terminating due to previous error: "
                       << strerror(errno);
            /*
            * continue, there is not much we can do here.
            * In the worst case, this will trigger an error
            * in testcase_run.sh, as the runuser may still be
            * running processes
            */
        }

        /* Wait a while to make sure the process is killed by now. */
        nanosleep(&killdelay, nullptr);
    }

    exit(EXIT_FAILURE);
}

void summarize_cgroup(const runguard_options& opt, int exitcode,
                      struct timeval starttime, struct timeval endtime,
                      struct tms startticks, struct tms endticks,
                      size_t data_passed[3], size_t data_read[3]) {
    static const char output_timelimit_str[4][16] = {
        "",
        "soft-timelimit",
        "hard-timelimit",
        "hard-timelimit"};
    double cpudiff;

    cgroup_guard guard(opt.cgroupname);
    guard.get_cgroup();  // prepare for get_controller

    {
        cgroup_ctrl ctrl = guard.get_controller("memory");
        int64_t max_usage = ctrl.get_value_int64("memory.memsw.max_usage_in_bytes");

        LOG(INFO) << "total memory used: " << max_usage / 1024 << "kB";
        append_meta("memory-bytes", to_string(max_usage));
    }
    {
        cgroup_ctrl ctrl = guard.get_controller("cpuacct");
        int64_t cpu_time = ctrl.get_value_int64("cpuacct.usage");  // in ns
        cpudiff = (double)cpu_time / 1e9;
    }

    bool is_oom = false;
    {
        ifstream fin("/sys/fs/cgroup/memory" + opt.cgroupname + "/memory.oom_control");
        while (fin.good()) {
            string token;
            fin >> token;
            if (token == "oom_kill")
                fin >> is_oom;
        }
    }

    // 另一种实现读取是否发生 OOM 的方法：
    // uint64_t u;
    // assert(is_oom == (read(efd, &u, sizeof(uint64_t)) == sizeof(uint64_t)));

    if (is_oom)
        append_meta("memory-result", "oom");
    else
        append_meta("memory-result", "");

    // 杀死 cgroup 内所有的进程，以确保父进程结束后不会有
    // so our timing is correct: no child processes can survive longer than
    // our monitored process. Run time of the monitored process is actually
    // the runtime of the whole process group.
    cgroup_kill(opt);
    cgroup_delete(opt);

    unsigned long tps = sysconf(_SC_CLK_TCK);
    append_meta("exitcode", exitcode);

    if (received_signal != -1) {
        append_meta("signal", received_signal);
    }

    double walldiff = (endtime.tv_sec - starttime.tv_sec) +
                      (endtime.tv_usec - starttime.tv_usec) * 1E-6;
    double userdiff = (double)(endticks.tms_cutime - startticks.tms_cutime) / tps;
    double sysdiff = (double)(endticks.tms_cstime - startticks.tms_cstime) / tps;

    append_meta("wall-time", fmt::format("{:.3f}", walldiff));
    append_meta("user-time", fmt::format("{:.3f}", userdiff));
    append_meta("sys-time", fmt::format("{:.3f}", sysdiff));
    append_meta("cpu-time", fmt::format("{:.3f}", cpudiff));

    LOG(INFO) << fmt::format("run time: real {:.3f}, user {:.3f}, sys {:.3f}", walldiff, userdiff, sysdiff);

    if (opt.use_wall_limit && walldiff > opt.wall_limit.soft) {
        walllimit |= TIMELIMIT_SOFT;
        LOG(WARNING) << "Time Limit Exceeded (soft wall time)";
    }

    if (opt.use_cpu_limit && cpudiff > opt.cpu_limit.soft) {
        cpulimit |= TIMELIMIT_SOFT;
        LOG(WARNING) << "Time Limit Exceeded (soft cpu time)";
    }

    append_meta("time-result", output_timelimit_str[walllimit | cpulimit]);

    if (opt.stream_size >= 0) {
        using namespace boost::assign;
        vector<string> output_truncated;
        if (data_passed[STDOUT_FILENO] < data_read[STDOUT_FILENO])
            output_truncated += "stdout";
        if (data_passed[STDERR_FILENO] < data_read[STDERR_FILENO])
            output_truncated += "stderr";
        append_meta("output-truncated", boost::algorithm::join(output_truncated, ","));
    }

    append_meta("stdin-bytes", data_read[STDIN_FILENO]);
    append_meta("stdout-bytes", data_read[STDOUT_FILENO]);
    append_meta("stderr-bytes", data_read[STDERR_FILENO]);
}

void terminate(int sig) {
    struct sigaction sigact;

    /* Reset signal handlers to default */
    sigact.sa_handler = SIG_DFL;
    sigact.sa_flags = 0;
    if (sigemptyset(&sigact.sa_mask) != 0)
        LOG(WARNING) << "could not initialize signal mask";
    if (sigaction(SIGTERM, &sigact, NULL) != 0)
        LOG(WARNING) << "could not restore signal handler";
    if (sigaction(SIGALRM, &sigact, NULL) != 0)
        LOG(WARNING) << "could not restore signal handler";

    if (sig == SIGALRM) {
        walllimit |= TIMELIMIT_HARD;
        LOG(WARNING) << "timelimit exceeded (hard wall time): aborting command";
    } else {
        LOG(WARNING) << "received signal " << sig << ": aborting command";
    }

    received_signal = sig;

    /* First try to kill graciously, then hard.
	   Don't report an already exited process as error. */
    LOG(INFO) << "sending SIGTERM";
    if (kill(-child_pid, SIGTERM) != 0 && errno != ESRCH) {
        error(errno, "sending SIGTERM to command");
    }

    /* Prefer nanosleep over sleep because of higher resolution and
	   it does not interfere with signals. */
    nanosleep(&killdelay, NULL);

    LOG(INFO) << "sending SIGKILL";
    if (kill(-child_pid, SIGKILL) != 0 && errno != ESRCH) {
        error(errno, "sending SIGKILL to command");
    }

    /* Wait another while to make sure the process is killed by now. */
    nanosleep(&killdelay, NULL);
}

static void child_handler(int /* signal */) {
    received_SIGCHLD = true;
}

static void pump_pipes(struct runguard_options& opt, fd_set* readfds, bool& use_splice, int child_pipefd[3][2], int child_redirfd[3], size_t data_read[], size_t data_passed[]) {
    char buf[BUF_SIZE];
    ssize_t nread, nwritten;
    size_t to_read, to_write;
    int i;

    /* Check to see if data is available and pass it on */
    for (i = 1; i <= 2; i++) {
        if (child_pipefd[i][PIPE_OUT] != -1 &&
            FD_ISSET(child_pipefd[i][PIPE_OUT], readfds)) {
            if (opt.stream_size >= 0 && data_passed[i] == (size_t)opt.stream_size) {
                /* Throw away data if we're at the output limit, but
				   still count how much data we consumed  */
                nread = read(child_pipefd[i][PIPE_OUT], buf, BUF_SIZE);
            } else {
                /* Otherwise copy the output to a file */
                to_read = BUF_SIZE;
                if (opt.stream_size >= 0) {
                    to_read = min(BUF_SIZE, opt.stream_size - (int)data_passed[i]);
                }

                if (use_splice) {
                    nread = splice(child_pipefd[i][PIPE_OUT], NULL,
                                   child_redirfd[i], NULL,
                                   to_read, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);

                    if (nread == -1 && errno == EINVAL) {
                        use_splice = false;
                        LOG(ERROR) << "splice failed, switching to read/write";
                        /* Setting errno here to repeat the copy. */
                        errno = EAGAIN;
                    }
                } else {
                    nread = read(child_pipefd[i][PIPE_OUT], buf, to_read);
                    if (nread > 0) {
                        to_write = nread;
                        while (to_write > 0) {
                            nwritten = write(child_redirfd[i], buf, to_write);
                            if (nwritten == -1) {
                                nread = -1;
                                break;
                            }
                            to_write -= nwritten;
                        }
                    }
                }

                if (nread > 0) data_passed[i] += nread;

                /* print message if we're at the streamsize limit */
                if (opt.stream_size >= 0 && data_passed[i] == (size_t)opt.stream_size) {
                    LOG(INFO) << "child fd " << i << " limit reached";
                }
            }
            if (nread == -1) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                error(errno, "copying data fd {}", i);
            }
            if (nread == 0) {
                /* EOF detected: close fd and indicate this with -1 */
                if (close(child_pipefd[i][PIPE_OUT]) != 0) {
                    error(errno, "closing pipe for fd {}", i);
                }
                child_pipefd[i][PIPE_OUT] = -1;
                continue;
            }
            data_read[i] += nread;
        }
    }
}

int runit(struct runguard_options opt) {
    set_terminate(runguard_terminate_handler);
    metafile.open(opt.metafile_path.c_str(), ofstream::out);

    int child_pipefd[3][2];
    int child_redirfd[3];

    /* Setup pipes connecting to child stdout/err streams (ignore stdin). */
    for (int i = 1; i <= 2; i++) {
        if (pipe(child_pipefd[i]) != 0) error(errno, "creating pipe for fd %d", i);
    }

    {
        struct sigaction sigact;
        sigset_t sigmask, emptymask;
        if (sigemptyset(&emptymask) != 0) error(errno, "creating empty signal mask");

        /* unmask all signals, except SIGCHLD: detected in pselect() below */
        sigmask = emptymask;
        if (sigaddset(&sigmask, SIGCHLD) != 0) error(errno, "setting signal mask");
        if (sigprocmask(SIG_SETMASK, &sigmask, NULL) != 0) {
            error(errno, "unmasking signals");
        }

        /* Construct signal handler for SIGCHLD detection in pselect(). */
        received_SIGCHLD = 0;
        sigact.sa_handler = child_handler;
        sigact.sa_flags = 0;
        sigact.sa_mask = emptymask;
        if (sigaction(SIGCHLD, &sigact, NULL) != 0) {
            error(errno, "installing signal handler");
        }
    }

    cgroup_guard::init();

    opt.cgroupname = fmt::format("/judger/cgroup_{}_{}", getpid(), (int)time(NULL));

    cgroup_create(opt);

    /*
     * unshare 函数可以用来进行进程隔离。通常情况下，POSIX 系统的 fork 或 clone 函数
     * 在产生子进程时会共享父进程的资源，比如打开的文件描述符表等。我们通过 unshare 来
     * 避免 runguard 程序及受控程序能访问到调用 runguard 的 bash 脚本打开的文件以及
     * 评测系统打开的文件。
     * 
     * CLONE_FILES：隔离文件描述符表，阻止子进程打开调用 runguard 的父进程打开过的文件（比如父
     *              进程打开过标准输入数据文件）
     * CLONE_FS：
     * CLONE_NEWIPC：隔离 IPC 命名空间。因此 runguad 及受控程序将被移入一个新的 IPC 命名空间，
     *              这样 runguard 及受控程序就无法再主动与主机程序进行进程间通信
     * CLONE_NEWNET：隔离网络命名空间。因此 runguard 及受控程序将被移入一个新的网络命名空间，
     *              这样 runguard 及受控程序就无法再访问主机网络
     * CLONE_NEWNS: 隔离进程关系。因此 runguard 及受控程序将被移入一个新的进程命名空间，
     *              这样 runguard 及受控程序无法获得主机程序的进程列表
     * CLONE_NEWUTS: 隔离主机和受控程序的 hostname 和 NIS，避免利用 NIS 来进行通信
     * CLONE_SYSVSEM：
     */
    unshare(CLONE_FILES | CLONE_FS | CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWUTS | CLONE_SYSVSEM);

    {
        /* Check if any Linux Out-Of-Memory killer adjustments have to
         * be made. The oom_adj or oom_score_adj is inherited by child
         * processes, and at least older versions of sshd seemed to set
         * it, leading to processes getting a timelimit instead of memory
         * exceeded, when running via SSH. */
        const char* OOM_PATH_NEW = "/proc/self/oom_score_adj";
        const char* OOM_PATH_OLD = "/proc/self/oom_adj";
        const int OOM_RESET_VALUE = 0;

        FILE* fp = nullptr;
        string oom_path;
        int ret;
        if (!fp && (fp = fopen(OOM_PATH_NEW, "r+"))) oom_path = OOM_PATH_NEW;
        if (!fp && (fp = fopen(OOM_PATH_OLD, "r+"))) oom_path = OOM_PATH_OLD;
        if (fp) {
            if (fscanf(fp, "%d", &ret) != 1) error(errno, "cannot read from '{}'", oom_path);
            if (ret < 0) {
                LOG(INFO) << "resetting '" << oom_path << "' from " << ret << " to " << OOM_RESET_VALUE;
                rewind(fp);
                if (fprintf(fp, "%d\n", OOM_RESET_VALUE) <= 0) {
                    error(errno, "cannot write to '{}'", oom_path);
                }
            }
            if (fclose(fp) != 0) error(errno, "closing file '{}'", oom_path);
        }
    }

    // 另一种实现读取是否发生 OOM 的方法：
    // if ((efd = eventfd(0, EFD_NONBLOCK)) < 0) error(errno, "requesting event fd");
    // int cfd = open(("/sys/fs/cgroup/memory" + opt.cgroupname + "/cgroup.event_control").c_str(), O_WRONLY);
    // if (cfd < 0) error(errno, "opening cgroup.event_control");
    // int ofd = open(("/sys/fs/cgroup/memory" + opt.cgroupname + "/memory.oom_control").c_str(), O_RDONLY);
    // if (ofd < 0) error(errno, "opening memory.oom_control");
    // string oom = fmt::format("{} {}", efd, ofd);
    // if (write(cfd, oom.data(), oom.size()) < 0) error(errno, "writing cgroup.event_control");
    // if (close(cfd) < 0) error(errno, "closing cgroup.event_control");

    switch (child_pid = fork()) {
        case -1:
            throw system_error(errno, system_category(), "unable to fork");
        case 0: {  // child process, run the command
            if (opt.stdin_filename.size())
                freopen(opt.stdin_filename.c_str(), "r", stdin);

            set_restrictions(opt);

            // 将管道连接到 (stdin/)stdout/stderr。
            for (int i = 1; i <= 2; ++i) {
                if (dup2(child_pipefd[i][PIPE_IN], i) < 0) {
                    error(errno, "redirecting child fd {}", i);
                }
                if (close(child_pipefd[i][PIPE_IN]) != 0 ||
                    close(child_pipefd[i][PIPE_OUT]) != 0) {
                    error(errno, "closing pipe for fd {}", i);
                }
            }

            auto& cmd = opt.command;
            char** args = new char*[cmd.size() + 1];
            for (size_t i = 0; i < cmd.size(); ++i) args[i] = cmd[i].data();
            args[cmd.size()] = 0;

            execvp(args[0], args);
            error(errno, "unable to start command {}", cmd[0]);
        } break;
        default: {  // watchdog
            if (opt.user_id < 0) {
                /*
                 * Shed privileges, only if not using a separate child uid,
                 * because in that case we may need root privileges to kill
                 * the child process. Do not use Linux specific setresuid()
                 * call with saved set-user-ID.
                 */
                if (setuid(getuid()) != 0) throw system_error(errno, system_category(), "setting watchdog uid");
            }

            // Wait for child data or exit.
            // Initialize status here to quelch clang++ warning about
            // uninitialized value; it is set by the wait() call.
            int status = 0, exitcode;
            struct tms startticks, endticks;
            struct timeval starttime, endtime;
            size_t data_read[3];
            size_t data_passed[3];
            size_t total_data;
            fd_set readfds;

            if (gettimeofday(&starttime, NULL))
                error(errno, "getting time");

            {
                /* Close unused file descriptors */
                for (int i = 1; i <= 2; i++) {
                    if (close(child_pipefd[i][PIPE_IN]) != 0) {
                        error(errno, "closing pipe for fd {}", i);
                    }
                }

                /* Redirect child stdout/stderr to file */
                for (int i = 1; i <= 2; i++) {
                    child_redirfd[i] = i;              /* Default: no redirects */
                    data_read[i] = data_passed[i] = 0; /* Reset data counters */
                }
                data_read[0] = 0;
                if (!opt.stdout_filename.empty()) {
                    child_redirfd[STDOUT_FILENO] = creat(opt.stdout_filename.c_str(), S_IRUSR | S_IWUSR);
                    if (child_redirfd[STDOUT_FILENO] < 0) {
                        error(errno, "opening file '{}'", opt.stdout_filename);
                    }
                }
                if (!opt.stderr_filename.empty()) {
                    if (opt.stderr_filename == opt.stdout_filename) {
                        child_redirfd[STDERR_FILENO] = child_redirfd[STDOUT_FILENO];
                    } else {
                        child_redirfd[STDERR_FILENO] = creat(opt.stderr_filename.c_str(), S_IRUSR | S_IWUSR);
                        if (child_redirfd[STDERR_FILENO] < 0) {
                            error(errno, "opening file '{}'", opt.stderr_filename);
                        }
                    }
                }
                LOG(INFO) << "redirection done in parent";
            }

            sigset_t emptymask;
            if (sigemptyset(&emptymask) != 0) error(errno, "creating empty signal mask");

            {
                sigset_t sigmask;
                struct sigaction sigact;

                /* Construct one-time signal handler to terminate() for TERM
		           and ALRM signals. */
                sigmask = emptymask;
                if (sigaddset(&sigmask, SIGALRM) != 0 || sigaddset(&sigmask, SIGTERM) != 0)
                    error(errno, "setting signal mask");

                sigact.sa_handler = terminate;
                sigact.sa_flags = SA_RESETHAND | SA_RESTART;
                sigact.sa_mask = sigmask;

                /* Kill child command when we receive SIGTERM */
                if (sigaction(SIGTERM, &sigact, NULL) != 0) {
                    error(errno, "installing signal handler");
                }

                if (opt.use_wall_limit) {
                    /* Kill child when we receive SIGALRM */
                    if (sigaction(SIGALRM, &sigact, NULL) != 0) {
                        error(errno, "installing signal handler");
                    }

                    double tmpd;
                    struct itimerval itimer;
                    /* Trigger SIGALRM via setitimer:  */
                    itimer.it_interval.tv_sec = 0;
                    itimer.it_interval.tv_usec = 0;
                    itimer.it_value.tv_sec = (int)opt.wall_limit.hard;
                    itimer.it_value.tv_usec = (int)(modf(opt.wall_limit.hard, &tmpd) * 1E6);

                    if (setitimer(ITIMER_REAL, &itimer, NULL) != 0) {
                        error(errno, "setting timer");
                    }
                    LOG(INFO) << fmt::format("setting hard wall-time limit to {:.3f} seconds", opt.wall_limit.hard);
                }
            }

            if (times(&startticks) == (clock_t)-1)
                error(errno, "getting start clock ticks");

            // We start using splice() to copy data from child to parent
            // I/O file descriptors. If that fails (not all I/O
            // source - dest combinations support it), then we revert to
            // using read()/write().
            bool use_splice = true;
            while (1) {
                FD_ZERO(&readfds);
                int nfds = -1;
                for (int i = 1; i <= 2; i++) {
                    if (child_pipefd[i][PIPE_OUT] >= 0) {
                        FD_SET(child_pipefd[i][PIPE_OUT], &readfds);
                        nfds = max(nfds, child_pipefd[i][PIPE_OUT]);
                    }
                }

                int r = pselect(nfds + 1, &readfds, NULL, NULL, NULL, &emptymask);
                if (r == -1 && errno != EINTR) error(errno, "waiting for child data");

                if (received_SIGCHLD || received_signal == SIGALRM) {
                    int pid;
                    if ((pid = wait(&status)) < 0) error(errno, "waiting on child");
                    if (pid == child_pid) break;
                }

                pump_pipes(opt, &readfds, use_splice, child_pipefd, child_redirfd, data_read, data_passed);
            }

            /* Reset pipe filedescriptors to use blocking I/O. */
            FD_ZERO(&readfds);
            for (int i = 1; i <= 2; i++) {
                if (child_pipefd[i][PIPE_OUT] >= 0) {
                    FD_SET(child_pipefd[i][PIPE_OUT], &readfds);
                    int r = fcntl(child_pipefd[i][PIPE_OUT], F_GETFL);
                    if (r == -1) {
                        error(errno, "fcntl, getting flags");
                    }
                    r = fcntl(child_pipefd[i][PIPE_OUT], F_SETFL, r ^ O_NONBLOCK);
                    if (r == -1) {
                        error(errno, "fcntl, setting flags");
                    }
                }
            }

            do {
                total_data = data_passed[1] + data_passed[2];
                pump_pipes(opt, &readfds, use_splice, child_pipefd, child_redirfd, data_read, data_passed);
            } while (data_passed[1] + data_passed[2] > total_data);

            /* Close the output files */
            if (close(child_redirfd[STDIN_FILENO]) != 0) error(errno, "closing output fd {}", STDIN_FILENO);
            if (child_redirfd[STDIN_FILENO] != child_redirfd[STDERR_FILENO] && close(child_redirfd[STDERR_FILENO]) != 0) error(errno, "closing output fd {}", STDERR_FILENO);

            if (times(&endticks) == (clock_t)-1)
                error(errno, "getting end clock ticks");
            if (gettimeofday(&endtime, NULL))
                error(errno, "getting time");

            if (WIFEXITED(status)) {
                exitcode = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                // In linux, exitcode is no larger than 127.
                received_signal = WTERMSIG(status);
                exitcode = received_signal + 128;
                switch (received_signal) {
                    case SIGXCPU:
                        cpulimit |= TIMELIMIT_HARD;
                        LOG(WARNING) << "Time Limit Exceeded (hard limit)";
                        break;
                    default:
                        LOG(WARNING) << "Command terminated with signal (" << received_signal << ", " << strsignal(received_signal) << ")";
                        break;
                }
            } else if (WIFSTOPPED(status)) {
                received_signal = WSTOPSIG(status);
                exitcode = received_signal + 128;
                LOG(WARNING) << "Command stopped with signal (" << received_signal << ", " << strsignal(received_signal) << ")";
            } else {
                throw runtime_error(fmt::format("unknown status: {:x}", status));
            }

            if (setuid(getuid()) != 0)
                error(errno, "dropping root privileges");

            summarize_cgroup(opt, exitcode, starttime, endtime, startticks, endticks, data_passed, data_read);

            return exitcode;
        } break;
    }

    throw runtime_error("unexpected");
}
