#ifndef _SYSCALL_TABLE_H_
#define _SYSCALL_TABLE_H_

#include <sys/user.h>
#include <sys/ptrace.h>

#define SYSCALL_TABLE_32 0
#define SYSCALL_TABLE_64 1

extern int syscall_used[2][512];
void init_syscall_table();

/**
 * Find out if the syscall is 32-bit or 64-bit
 * @return System call type
 *         SYSCALL_TABLE_32 if is 32-bit
 *         SYSCALL_TABLE_64 if is 64-bit
 */
int syscall_type(int pid, struct user_regs_struct regs);

#endif
