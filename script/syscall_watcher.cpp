#include <iostream>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <set>
using namespace std;
int main(int argc, char **argv)
{
	int pid = fork();
	if (pid == 0)
	{
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		execvp(argv[1], argv + 1);
	}
	else
	{
		set<long long> s;
		bool incall = false;
		while (1)
		{
			int status;
			waitpid(pid, &status, 0);
			if (WIFEXITED(status) || WIFSIGNALED(status)) break;
			if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP)
			{
				if (incall)
				{
					struct user_regs_struct regs;
					ptrace(PTRACE_GETREGS, pid, NULL, &regs);
					s.insert(regs.orig_rax);
				}
				incall = !incall;
				ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
			}
		}
		puts("--------SYSCALLS--------");
		for (auto &i : s) printf("%lld ", i);
		puts("");
	}
}
