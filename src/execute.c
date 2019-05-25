#define __EXECUTE_C__

#include "execute.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "syscall_table.h"

static void run_user_program(const char *input_file_name, const char *output_file_name, const char *execute_file_name, int time_limit, int output_limit)
{
    struct rlimit limit;
    printf("[child]: ready.\n");

    if (freopen(input_file_name, "r", stdin) == NULL)
    {
        fprintf(stderr, "[child]: ERROR open data file %s", input_file_name);
        perror(" ");
        _exit(1);
    }

    if (freopen(output_file_name, "w", stdout) == NULL)
    {
        fprintf(stderr, "[child]: ERROR open output file %s", output_file_name);
        perror(" ");
        _exit(1);
    }

    // put limits on myself
    limit.rlim_cur = time_limit;
    limit.rlim_max = time_limit + 1;

    if (setrlimit(RLIMIT_CPU, &limit))
    {
        perror("[child]: setting time limit: ");
        _exit(1);
    }

    limit.rlim_cur = output_limit - 1;
    limit.rlim_max = output_limit;

    if (setrlimit(RLIMIT_FSIZE, &limit))
    {
        perror("[child]: setting output limit: ");
        _exit(1);
    }

    limit.rlim_cur = 0;
    limit.rlim_max = 0;

    if (setrlimit(RLIMIT_CORE, &limit))
    {
        perror("[child]: setting core limit: ");
        _exit(1);
    }

    ptrace(PTRACE_TRACEME, 0, 0, 0);

    execve(execute_file_name, 0, 0);
    fprintf(stderr, "[child]: ERROR executing %s", execute_file_name);
    perror(" ");
    _exit(1);
}

int execute(const char *input_file_name, const char *output_file_name, const char *execute_file_name, int time_limit,
            int memory_limit, int output_limit, int *run_memory, double *cputime_ptr)
{
    int pid, timer_pid, ret_pid;
    int sig;
    int mem_sum = 0,
        maxmem = 0;
    unsigned int cur_sum, start, end;
    int status;
    int exitcode = 1;
    char procmaps[256];
    char buffer[256];
    int syscall_cnt = 0;
    int syscall_enter = 1;
    double cputime = -1;
    int syscall_number;
    int inode;
    struct rusage ru;
    struct user_regs_struct regs;
    FILE *maps;

    // hack: force the time limit under 60 seconds
    time_limit = time_limit > 60 ? 60 : time_limit;

    if ((pid = fork()) < 0)
    {
        perror("PANIC: fork ERROR ");
        _exit(1);
    }

    // ========================== execute program ==================================
    if (pid == 0)
    {
        run_user_program(input_file_name, output_file_name, execute_file_name, time_limit, output_limit);
    }

    // ==============================  timer  ======================================
    fprintf(stderr, "[Judge]: child process id:%d\n", pid);
    fprintf(stderr, "[Judge]: creating it's brother as timer\n");

    timer_pid = fork();

    if (timer_pid < 0)
    {
        perror("[Judge]: ERROR creating timer");
        kill(pid, SIGKILL);
        _exit(1);
    }
    else if (timer_pid == 0)
    {
        // ==== timer
        fprintf(stderr, "[timer]: ready!\n");
        sleep(3 * time_limit); // should be enough time....
        fprintf(stderr, "[timer]: time up! exiting!\n");
        _exit(1);
    }

    // ==============================  judge  ======================================
    init_syscall_table();

    sprintf(procmaps, "/proc/%d/maps", pid);
    fprintf(stderr, "[Judge]: maps: %s\n", procmaps);
    wait3(&status, 0, &ru); // wait for pid STOP after execve(), hopefully it won't be the timer
    fprintf(stderr, "[JUDGE]: status_code = %d!\n", WEXITSTATUS(status));

    if (WIFEXITED(status))
    {
        fprintf(stderr, "[Judge]:  program exit before tracing\n");
        kill(timer_pid, SIGKILL);

        // ugly quickfix ...
        int exs = WEXITSTATUS(status);

        switch (exs)
        {
        case 0:
            fprintf(stderr, "[Judge]:  %d exited normally\n", pid);

            return 0;

        case 1:
            *cputime_ptr = time_limit;

            fprintf(stderr, "[Judge]:  timer exited\n");

            return -2;

        default:
            fprintf(stderr, "[Judge]:  unexpected signal caught \n");

            return -1;
        }
    }

    ptrace(PTRACE_SYSCALL, pid, 0, 0); // restart

    while ((ret_pid = wait3(&status, 0, &ru)) > 0)
    {
        if (ret_pid == timer_pid)
        { // time limit
            kill(pid, SIGKILL);

            cputime = time_limit; // set time to TLE
            exitcode = EXEC_TLE;

            fprintf(stderr, "[Judge]:  timer exited\n");

            continue;
        }

        // pid STOPPED
        if (WIFEXITED(status))
        {
            // pid exited normally
            fprintf(stderr, "[Judge]:  %d exited normally; status = %d \n", pid, WEXITSTATUS(status));

            exitcode = EXEC_OK; // =======success

            break;
        }

        if (WIFSIGNALED(status))
        {
            sig = WTERMSIG(status);

            fprintf(stderr, "[Judge]:  deadly signal caught %d(%s) by %d\n", sig, signal_names[sig], pid);

            break;
        }

        if (WIFSTOPPED(status))
        {
            sig = WSTOPSIG(status);

            if (sig != SIGTRAP)
            {
                fprintf(stderr, "[Judge]:  unexpected signal caught %d(%s) killing %d\n", sig, signal_names[sig], pid);
                ptrace(PTRACE_KILL, pid, 0, 0);

                switch (sig)
                {
                case SIGXCPU:
                    exitcode = EXEC_TLE;

                    break;

                case SIGXFSZ:
                    exitcode = EXEC_OLE;

                    break;

                case SIGSEGV:
                    exitcode = EXEC_SF;
                    break;

                case SIGFPE:
                    exitcode = EXEC_FPE;
                    break;

                default:
                    exitcode = EXEC_RE;

                    break;
                }

                continue;
            }

            // SIGTRAP
            ptrace(PTRACE_GETREGS, pid, 0, &regs);
#ifdef __i386__
            syscall_number = regs.orig_eax;
#else
            syscall_number = regs.orig_rax;
#endif

            if (syscall_enter)
            {
                syscall_cnt++;

                int systype = syscall_type(pid, regs);
                if (systype < 0)
                {
                    fprintf(stderr, "[trace]: unable to gain syscall type\n");
                    exitcode = EXEC_RF;
                    ptrace(PTRACE_KILL, pid, 0, 0);
                    continue;
                }

                fprintf(stderr, "[trace]: systype: %d\n", systype);

                // check if is SYS_brk, ugly fix.
                if (systype == SYSCALL_TABLE_32 && syscall_number == 45 ||
                    systype == SYSCALL_TABLE_64 && syscall_number == 12)
                {
#ifdef __i386__
                    fprintf(stderr, "[trace]: brk (ebx = 0x%08lx) %lu\n", regs.ebx, regs.ebx);
#else
                    fprintf(stderr, "[trace]: brk (rdi = 0x%08llx) %llu\n", regs.rdi, regs.rdi);
#endif
                }

                //
                // check before execute syscall
                // modify syscall for restricted function call
                if (!syscall_used[systype][syscall_number])
                {
                    // oh no~ kill it!!!!!
                    fprintf(stderr, "[trace]: syscall[%d][%d]: %s : Restrict function!\n", systype, syscall_number,
                            syscall_names[systype][syscall_number]);
                    fprintf(stderr, "[trace]: killing process %d  .\\/.\n", pid);

                    exitcode = EXEC_RF;

                    ptrace(PTRACE_KILL, pid, 0, 0); //

                    continue;
                }
            }
            else
            {
                if ((maps = fopen(procmaps, "r")) != NULL)
                {
                    // =================================check mem<<<
                    cur_sum = 0;

                    while (fgets(buffer, 256, maps))
                    {
                        sscanf(buffer, "%x-%x %*s %*s %*s %d", &start, &end, &inode);

                        if (inode == 0)
                        {
                            cur_sum += end - start; // possiblly all data/stack segments
                        }
                    }

                    fclose(maps);

                    cur_sum >>= 10; // bytes -> kb

                    if (cur_sum != mem_sum)
                    {
                        fprintf(stderr, "[Judge]: proc %d memory usage: %dk\n", pid, cur_sum);

                        mem_sum = cur_sum;

                        if (maxmem < mem_sum)
                        {
                            maxmem = mem_sum;
                        }

                        if (cur_sum > memory_limit)
                        {
                            fprintf(stderr, "[Judge]:  Memory Limit Exceed\n");
                            fprintf(stderr, "[Judge]:  killing process %d .\\/.\n", pid);
                            kill(pid, SIGKILL);

                            exitcode = EXEC_MLE;

                            continue;
                        }
                    }
                }

                // fprintf(stderr, ">\n");//leaving syscall\n");
            }

            syscall_enter = !syscall_enter;

            ptrace(PTRACE_SYSCALL, pid, 0, 0);
        }
    }

    fprintf(stderr, "[Judge]:  maximum memory used by %s: %dk\n", execute_file_name, maxmem);
    fprintf(stderr, "[Judge]:  utime sec %d, usec %06d\n", (int)ru.ru_utime.tv_sec, (int)ru.ru_utime.tv_usec);
    fprintf(stderr, "[Judge]:  stime sec %d, usec %06d\n", (int)ru.ru_stime.tv_sec, (int)ru.ru_stime.tv_usec);
    fprintf(stderr, "[Judge]:  mem usage %d \n", (int)ru.ru_maxrss);

    if (cputime < 0)
    {
        cputime = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec * 1e-6 + ru.ru_stime.tv_sec + ru.ru_stime.tv_usec * 1e-6;
    }

    *cputime_ptr = cputime;
    *run_memory = maxmem;

    fprintf(stderr, "[Judge]: cputime used %.4f\n", cputime);

    // kill(timer_pid, SIGINT);
    kill(timer_pid, SIGKILL);
    wait(&status);
    wait(&status);
    fprintf(stderr, "[Judge]: exiting   Total syscall %d\n", syscall_cnt);

    return exitcode;
}
