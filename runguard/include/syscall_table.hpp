#pragma once

#include <sys/user.h>

const int SYSCALL_TABLE_32 = 0;
const int SYSCALL_TABLE_64 = 1;

extern int syscall_used[2][512];
void init_syscall_table();

/**
 * Find out if the syscall is 32-bit or 64-bit
 * @return System call type
 *         SYSCALL_TABLE_32 if is 32-bit
 *         SYSCALL_TABLE_64 if is 64-bit
 */
int syscall_type(int pid, struct user_regs_struct regs);
