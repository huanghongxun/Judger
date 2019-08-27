#pragma once

extern char *(signal_names[32]);

/*
 *  * syscall names for i386 under 2.5.51, taken from <asm/unistd.h>
 *   */
extern char *(syscall_names_32[512]);

/*
 * x86_64 syscall table: taken from <asm/unistd_64.h>
 *      leasunhy
 */
extern char *(syscall_names_64[512]);

extern char **syscall_names[2];
