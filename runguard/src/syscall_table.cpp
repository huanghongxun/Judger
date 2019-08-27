#include "syscall_table.hpp"
#include <errno.h>
#include <sys/ptrace.h>

int syscall_used[2][512];

static void init_syscall_table32() {
    int *s = syscall_used[SYSCALL_TABLE_32];
    s[1] = 1;   // exit
    s[3] = 1;   // read
    s[4] = 1;   // write
    s[5] = 1;   // open
    s[6] = 1;   // close
    s[13] = 1;  // time
    s[33] = 1;  // access
    s[45] = 1;  // brk
    s[54] = 1;  // ioctl
    // s[67] = 1; // sigaction
    s[85] = 1;   // readlink
    s[90] = 1;   // mmap
    s[91] = 1;   // munmap
    s[122] = 1;  // uname
    // s[125] = 1; // mprotect
    s[140] = 1;  // llseek
    s[145] = 1;  // readv
    s[146] = 1;  // writev
    s[174] = 1;  // sys_rt_sigaction
    // s[186] = 1; // sigaltsatck
    s[191] = 1;  // getrlimit
    s[192] = 1;  // mmap2
    s[197] = 1;  // fstat64
    s[199] = 1;  // getuid32    Get user identity
    s[200] = 1;  // getegid32   Get group identity
    s[201] = 1;  // geteuid32   Get user identity
    s[202] = 1;  // getgid32    Get group identity
    s[221] = 1;  // getdents64  Get directory entrys
    s[252] = 1;  // exit group
}

static void init_syscall_table64() {
    int *s = syscall_used[SYSCALL_TABLE_64];
    s[60] = 1;  // sys_exit
    s[0] = 1;   // sys_read
    s[1] = 1;   // sys_write
    s[2] = 1;   // sys_open
    s[3] = 1;   // sys_close
    s[5] = 1;   // sys_fstat
    s[8] = 1;   // sys_lseek
    s[9] = 1;   // sys_mmap
    s[10] = 1;  // sys_mprotect
    s[12] = 1;  // sys_brk
    s[13] = 1;  // sys_rt_sigaction
    s[16] = 1;  // sys_ioctl
    s[19] = 1;  // sys_readv
    s[20] = 1;  // sys_writev
    s[21] = 1;  // sys_access
    s[11] = 1;  // sys_munmap
    s[63] = 1;  // sys_uname
    s[89] = 1;  // sys_readlink
    s[97] = 1;  // sys_getrlimit
    // mmap2 is not found in x86_64 system call
    s[102] = 1;  // sys_getuid
    s[104] = 1;  // sys_getgid
    s[107] = 1;  // sys_geteuid
    s[108] = 1;  // sys_getegid
    s[158] = 1;  // sys_arch_prctl
    s[201] = 1;  // sys_time
    s[217] = 1;  // sys_getdents64
    s[231] = 1;  // sys_exitgroup
    s[292] = 1;  // sys_dup3
}

void init_syscall_table() {
    init_syscall_table32();
    init_syscall_table64();
}

int syscall_type(int pid, struct user_regs_struct regs) {
    long ret;
    unsigned char primary, secondary;

    errno = 0;
    ret = ptrace(PTRACE_PEEKTEXT, pid, regs.rip, 0);
    if (ret == -1 && errno != 0) {
        return -1;
    }

    primary = (unsigned)0xFF & ret;
    secondary = (unsigned)0xFF & (ret >> 8);
    if (primary == 0xCD && secondary == 0x80) {
        // 0xCD: instruction interrupt
        // 0x80: 0x80 interrupt
        return SYSCALL_TABLE_32;
    } else if (primary == 0x0F && secondary == 0x05) {
        // 0x0F05: syscall
        return SYSCALL_TABLE_64;
    } else {
        return -1;
    }
}