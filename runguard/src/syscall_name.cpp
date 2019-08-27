#include "syscall_name.hpp"

char *(signal_names[32]) = {
    // Signal		Value	Action	Comment
    // "-------------------------------------------------------------------------",//
    "(null)",
    "SIGHUP",     // 	1	A	Hangupdetectedoncontrollingterminal or death of controllingprocess
    "SIGINT",     // 	2	A	Interruptfromkeyboard
    "SIGQUIT",    // 	3	A	Quitfromkeyboard
    "SIGILL",     // 	4	A	IllegalInstruction
    "SIGTRAP",    // 	5	CG	Trace/breakpointtrap
    "SIGABRT",    // 	6	C	Abortsignalfromabort(3) "SIGIOT    ",// 	6	CG	IOTtrap.Asynonymfor
    "SIGBUS",     // 	7	AG	Buserror
    "SIGFPE",     // 	8	C	Floatingpointexception
    "SIGKILL",    // 	9	AEF	Killsignal
    "SIGUSR1",    // 	10	A	User-definedsignal1
    "SIGSEGV",    // 	11	C	Invalidmemoryreference
    "SIGUSR2",    // 	12	A	User-definedsignal2
    "SIGPIPE",    // 	13	A	Brokenpipe:writetopipe
    "SIGALRM",    // 	14	A	Timersignalfromalarm(2)
    "SIGTERM",    // 	15	A	Terminationsignal
    "SIGSTKFLT",  // 	16	AG	Stackfaultoncoprocessor
    "SIGCHLD",    // 	17	B	Childstoppedorterminated
    "SIGCONT",    // 	18	Continue	ifstopped
    "SIGSTOP",    // 	19	DEF	Stopprocess
    "SIGTSTP",    // 	20	D	Stoptypedattty
    "SIGTTIN",    // 	21	D	ttyinputforbackgroundprocess
    "SIGTTOU",    // 	22	D	ttyoutputforbackgroundprocess
    "SIGURG",     // 	23	BG	Urgentconditiononsocket(4.2
    "SIGXCPU",    // 	24	AG	CPUtimelimitexceeded(4.2
    "SIGXFSZ",    // 	25	AG	Filesizelimitexceeded(4.2
    "SIGVTALRM",  // 	26	AG	Virtualalarmclock(4.2BSD)
    "SIGPROF",    // 	27	AG	Profilealarmclock
    "SIGWINCH",   // 	28	BG	Windowresizesignal(4.3BSD,
    "SIGIO",      // 	29	AG	I/Onowpossible(4.2BSD)
    "SIGPWR",     // 	30	AG	Powerfailure(SystemV)
    "SIGUNUSED"   // 	31	AG	Unusedsignal
};

/*
 *  * syscall names for i386 under 2.5.51, taken from <asm/unistd.h>
 *   */
char *(syscall_names_32[512]) = {
    "null",                    //  0
    "exit",                    //  1
    "fork",                    //  2
    "read",                    //  3
    "write",                   //  4
    "open",                    //  5
    "close",                   //  6
    "waitpid",                 //  7
    "creat",                   //  8
    "link",                    //  9
    "unlink",                  // 10
    "execve",                  // 11
    "chdir",                   // 12
    "time",                    // 13
    "mknod",                   // 14
    "chmod",                   // 15
    "lchown",                  // 16
    "break",                   // 17
    "oldstat",                 // 18
    "lseek",                   // 19
    "getpid",                  // 20
    "mount",                   // 21
    "umount",                  // 22
    "setuid",                  // 23
    "getuid",                  // 24
    "stime",                   // 25
    "ptrace",                  // 26
    "alarm",                   // 27
    "oldfstat",                // 28
    "pause",                   // 29
    "utime",                   // 30
    "stty",                    // 31
    "gtty",                    // 32
    "access",                  // 33
    "nice",                    // 34
    "ftime",                   // 35
    "sync",                    // 36
    "kill",                    // 37
    "rename",                  // 38
    "mkdir",                   // 39
    "rmdir",                   // 40
    "dup",                     // 41
    "pipe",                    // 42
    "times",                   // 43
    "prof",                    // 44
    "brk",                     // 45
    "setgid",                  // 46
    "getgid",                  // 47
    "signal",                  // 48
    "geteuid",                 // 49
    "getegid",                 // 50
    "acct",                    // 51
    "umount2",                 // 52
    "lock",                    // 53
    "ioctl",                   // 54
    "fcntl",                   // 55
    "mpx",                     // 56
    "setpgid",                 // 57
    "ulimit",                  // 58
    "oldolduname",             // 59
    "umask",                   // 60
    "chroot",                  // 61
    "ustat",                   // 62
    "dup2",                    // 63
    "getppid",                 // 64
    "getpgrp",                 // 65
    "setsid",                  // 66
    "sigaction",               // 67
    "sgetmask",                // 68
    "ssetmask",                // 69
    "setreuid",                // 70
    "setregid",                // 71
    "sigsuspend",              // 72
    "sigpending",              // 73
    "sethostname",             // 74
    "setrlimit",               // 75
    "getrlimit",               // 76
    "getrusage",               // 77
    "gettimeofday",            // 78
    "settimeofday",            // 79
    "getgroups",               // 80
    "setgroups",               // 81
    "select",                  // 82
    "symlink",                 // 83
    "oldlstat",                // 84
    "readlink",                // 85
    "uselib",                  // 86
    "swapon",                  // 87
    "reboot",                  // 88
    "readdir",                 // 89
    "mmap",                    // 90
    "munmap",                  // 91
    "truncate",                // 92
    "ftruncate",               // 93
    "fchmod",                  // 94
    "fchown",                  // 95
    "getpriority",             // 96
    "setpriority",             // 97
    "profil",                  // 98
    "statfs",                  // 99
    "fstatfs",                 // 100
    "ioperm",                  // 101
    "socketcall",              // 102
    "syslog",                  // 103
    "setitimer",               // 104
    "getitimer",               // 105
    "stat",                    // 106
    "lstat",                   // 107
    "fstat",                   // 108
    "olduname",                // 109
    "iopl",                    // 110
    "vhangup",                 // 111
    "idle",                    // 112
    "vm86old",                 // 113
    "wait4",                   // 114
    "swapoff",                 // 115
    "sysinfo",                 // 116
    "ipc",                     // 117
    "fsync",                   // 118
    "sigreturn",               // 119
    "clone",                   // 120
    "setdomainname",           // 121
    "uname",                   // 122
    "modify_ldt",              // 123
    "adjtimex",                // 124
    "mprotect",                // 125
    "sigprocmask",             // 126
    "create_module",           // 127
    "init_module",             // 128
    "delete_module",           // 129
    "get_kernel_syms",         // 130
    "quotactl",                // 131
    "getpgid",                 // 132
    "fchdir",                  // 133
    "bdflush",                 // 134
    "sysfs",                   // 135
    "personality",             // 136
    "afs_syscall",             // 137
    "setfsuid",                // 138
    "setfsgid",                // 139
    "_llseek",                 // 140
    "getdents",                // 141
    "_newselect",              // 142
    "flock",                   // 143
    "msync",                   // 144
    "readv",                   // 145
    "writev",                  // 146
    "getsid",                  // 147
    "fdatasync",               // 148
    "_sysctl",                 // 149
    "mlock",                   // 150
    "munlock",                 // 151
    "mlockall",                // 152
    "munlockall",              // 153
    "sched_setparam",          // 154
    "sched_getparam",          // 155
    "sched_setscheduler",      // 156
    "sched_getscheduler",      // 157
    "sched_yield",             // 158
    "sched_get_priority_max",  // 159
    "sched_get_priority_min",  // 160
    "sched_rr_get_interval",   // 161
    "nanosleep",               // 162
    "mremap",                  // 163
    "setresuid",               // 164
    "getresuid",               // 165
    "vm86",                    // 166
    "query_module",            // 167
    "poll",                    // 168
    "nfsservctl",              // 169
    "setresgid",               // 170
    "getresgid",               // 171
    "prctl",                   // 172
    "rt_sigreturn",            // 173
    "rt_sigaction",            // 174
    "rt_sigprocmask",          // 175
    "rt_sigpending",           // 176
    "rt_sigtimedwait",         // 177
    "rt_sigqueueinfo",         // 178
    "rt_sigsuspend",           // 179
    "pread",                   // 180
    "pwrite",                  // 181
    "chown",                   // 182
    "getcwd",                  // 183
    "capget",                  // 184
    "capset",                  // 185
    "sigaltstack",             // 186
    "sendfile",                // 187
    "getpmsg",                 // 188
    "putpmsg",                 // 189
    "vfork",                   // 190
    "ugetrlimit",              // 191
    "mmap2",                   // 192
    "truncate64",              // 193
    "ftruncate64",             // 194
    "stat64",                  // 195
    "lstat64",                 // 196
    "fstat64",                 // 197
    "lchown32",                // 198
    "getuid32",                // 199
    "getgid32",                // 200
    "geteuid32",               // 201
    "getegid32",               // 202
    "setreuid32",              // 203
    "setregid32",              // 204
    "getgroups32",             // 205
    "setgroups32",             // 206
    "fchown32",                // 207
    "setresuid32",             // 208
    "getresuid32",             // 209
    "setresgid32",             // 210
    "getresgid32",             // 211
    "chown32",                 // 212
    "setuid32",                // 213
    "setgid32",                // 214
    "setfsuid32",              // 215
    "setfsgid32",              // 216
    "pivot_root",              // 217
    "mincore",                 // 218
    "madvise",                 // 219
    "madvise1",                // 220
    "getdents64",              // 221
    "fcntl64",                 // 222
    0,                         // 223
    "security",                // 224
    "gettid",                  // 225
    "readahead",               // 226
    "setxattr",                // 227
    "lsetxattr",               // 228
    "fsetxattr",               // 229
    "getxattr",                // 230
    "lgetxattr",               // 231
    "fgetxattr",               // 232
    "listxattr",               // 233
    "llistxattr",              // 234
    "flistxattr",              // 235
    "removexattr",             // 236
    "lremovexattr",            // 237
    "fremovexattr",            // 238
    "tkill",                   // 239
    "sendfile64",              // 240
    "futex",                   // 241
    "sched_setaffinity",       // 242
    "sched_getaffinity",       // 243
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0};

/*
 * x86_64 syscall table: taken from <asm/unistd_64.h>
 *      leasunhy
 */
char *(syscall_names_64[512]) = {
    "read",                    // 0
    "write",                   // 1
    "open",                    // 2
    "close",                   // 3
    "stat",                    // 4
    "fstat",                   // 5
    "lstat",                   // 6
    "poll",                    // 7
    "lseek",                   // 8
    "mmap",                    // 9
    "mprotect",                // 10
    "munmap",                  // 11
    "brk",                     // 12
    "rt_sigaction",            // 13
    "rt_sigprocmask",          // 14
    "rt_sigreturn",            // 15
    "ioctl",                   // 16
    "pread64",                 // 17
    "pwrite64",                // 18
    "readv",                   // 19
    "writev",                  // 20
    "access",                  // 21
    "pipe",                    // 22
    "select",                  // 23
    "sched_yield",             // 24
    "mremap",                  // 25
    "msync",                   // 26
    "mincore",                 // 27
    "madvise",                 // 28
    "shmget",                  // 29
    "shmat",                   // 30
    "shmctl",                  // 31
    "dup",                     // 32
    "dup2",                    // 33
    "pause",                   // 34
    "nanosleep",               // 35
    "getitimer",               // 36
    "alarm",                   // 37
    "setitimer",               // 38
    "getpid",                  // 39
    "sendfile",                // 40
    "socket",                  // 41
    "connect",                 // 42
    "accept",                  // 43
    "sendto",                  // 44
    "recvfrom",                // 45
    "sendmsg",                 // 46
    "recvmsg",                 // 47
    "shutdown",                // 48
    "bind",                    // 49
    "listen",                  // 50
    "getsockname",             // 51
    "getpeername",             // 52
    "socketpair",              // 53
    "setsockopt",              // 54
    "getsockopt",              // 55
    "clone",                   // 56
    "fork",                    // 57
    "vfork",                   // 58
    "execve",                  // 59
    "exit",                    // 60
    "wait4",                   // 61
    "kill",                    // 62
    "uname",                   // 63
    "semget",                  // 64
    "semop",                   // 65
    "semctl",                  // 66
    "shmdt",                   // 67
    "msgget",                  // 68
    "msgsnd",                  // 69
    "msgrcv",                  // 70
    "msgctl",                  // 71
    "fcntl",                   // 72
    "flock",                   // 73
    "fsync",                   // 74
    "fdatasync",               // 75
    "truncate",                // 76
    "ftruncate",               // 77
    "getdents",                // 78
    "getcwd",                  // 79
    "chdir",                   // 80
    "fchdir",                  // 81
    "rename",                  // 82
    "mkdir",                   // 83
    "rmdir",                   // 84
    "creat",                   // 85
    "link",                    // 86
    "unlink",                  // 87
    "symlink",                 // 88
    "readlink",                // 89
    "chmod",                   // 90
    "fchmod",                  // 91
    "chown",                   // 92
    "fchown",                  // 93
    "lchown",                  // 94
    "umask",                   // 95
    "gettimeofday",            // 96
    "getrlimit",               // 97
    "getrusage",               // 98
    "sysinfo",                 // 99
    "times",                   // 100
    "ptrace",                  // 101
    "getuid",                  // 102
    "syslog",                  // 103
    "getgid",                  // 104
    "setuid",                  // 105
    "setgid",                  // 106
    "geteuid",                 // 107
    "getegid",                 // 108
    "setpgid",                 // 109
    "getppid",                 // 110
    "getpgrp",                 // 111
    "setsid",                  // 112
    "setreuid",                // 113
    "setregid",                // 114
    "getgroups",               // 115
    "setgroups",               // 116
    "setresuid",               // 117
    "getresuid",               // 118
    "setresgid",               // 119
    "getresgid",               // 120
    "getpgid",                 // 121
    "setfsuid",                // 122
    "setfsgid",                // 123
    "getsid",                  // 124
    "capget",                  // 125
    "capset",                  // 126
    "rt_sigpending",           // 127
    "rt_sigtimedwait",         // 128
    "rt_sigqueueinfo",         // 129
    "rt_sigsuspend",           // 130
    "sigaltstack",             // 131
    "utime",                   // 132
    "mknod",                   // 133
    "uselib",                  // 134
    "personality",             // 135
    "ustat",                   // 136
    "statfs",                  // 137
    "fstatfs",                 // 138
    "sysfs",                   // 139
    "getpriority",             // 140
    "setpriority",             // 141
    "sched_setparam",          // 142
    "sched_getparam",          // 143
    "sched_setscheduler",      // 144
    "sched_getscheduler",      // 145
    "sched_get_priority_max",  // 146
    "sched_get_priority_min",  // 147
    "sched_rr_get_interval",   // 148
    "mlock",                   // 149
    "munlock",                 // 150
    "mlockall",                // 151
    "munlockall",              // 152
    "vhangup",                 // 153
    "modify_ldt",              // 154
    "pivot_root",              // 155
    "_sysctl",                 // 156
    "prctl",                   // 157
    "arch_prctl",              // 158
    "adjtimex",                // 159
    "setrlimit",               // 160
    "chroot",                  // 161
    "sync",                    // 162
    "acct",                    // 163
    "settimeofday",            // 164
    "mount",                   // 165
    "umount2",                 // 166
    "swapon",                  // 167
    "swapoff",                 // 168
    "reboot",                  // 169
    "sethostname",             // 170
    "setdomainname",           // 171
    "iopl",                    // 172
    "ioperm",                  // 173
    "create_module",           // 174
    "init_module",             // 175
    "delete_module",           // 176
    "get_kernel_syms",         // 177
    "query_module",            // 178
    "quotactl",                // 179
    "nfsservctl",              // 180
    "getpmsg",                 // 181
    "putpmsg",                 // 182
    "afs_syscall",             // 183
    "tuxcall",                 // 184
    "security",                // 185
    "gettid",                  // 186
    "readahead",               // 187
    "setxattr",                // 188
    "lsetxattr",               // 189
    "fsetxattr",               // 190
    "getxattr",                // 191
    "lgetxattr",               // 192
    "fgetxattr",               // 193
    "listxattr",               // 194
    "llistxattr",              // 195
    "flistxattr",              // 196
    "removexattr",             // 197
    "lremovexattr",            // 198
    "fremovexattr",            // 199
    "tkill",                   // 200
    "time",                    // 201
    "futex",                   // 202
    "sched_setaffinity",       // 203
    "sched_getaffinity",       // 204
    "set_thread_area",         // 205
    "io_setup",                // 206
    "io_destroy",              // 207
    "io_getevents",            // 208
    "io_submit",               // 209
    "io_cancel",               // 210
    "get_thread_area",         // 211
    "lookup_dcookie",          // 212
    "epoll_create",            // 213
    "epoll_ctl_old",           // 214
    "epoll_wait_old",          // 215
    "remap_file_pages",        // 216
    "getdents64",              // 217
    "set_tid_address",         // 218
    "restart_syscall",         // 219
    "semtimedop",              // 220
    "fadvise64",               // 221
    "timer_create",            // 222
    "timer_settime",           // 223
    "timer_gettime",           // 224
    "timer_getoverrun",        // 225
    "timer_delete",            // 226
    "clock_settime",           // 227
    "clock_gettime",           // 228
    "clock_getres",            // 229
    "clock_nanosleep",         // 230
    "exit_group",              // 231
    "epoll_wait",              // 232
    "epoll_ctl",               // 233
    "tgkill",                  // 234
    "utimes",                  // 235
    "vserver",                 // 236
    "mbind",                   // 237
    "set_mempolicy",           // 238
    "get_mempolicy",           // 239
    "mq_open",                 // 240
    "mq_unlink",               // 241
    "mq_timedsend",            // 242
    "mq_timedreceive",         // 243
    "mq_notify",               // 244
    "mq_getsetattr",           // 245
    "kexec_load",              // 246
    "waitid",                  // 247
    "add_key",                 // 248
    "request_key",             // 249
    "keyctl",                  // 250
    "ioprio_set",              // 251
    "ioprio_get",              // 252
    "inotify_init",            // 253
    "inotify_add_watch",       // 254
    "inotify_rm_watch",        // 255
    "migrate_pages",           // 256
    "openat",                  // 257
    "mkdirat",                 // 258
    "mknodat",                 // 259
    "fchownat",                // 260
    "futimesat",               // 261
    "newfstatat",              // 262
    "unlinkat",                // 263
    "renameat",                // 264
    "linkat",                  // 265
    "symlinkat",               // 266
    "readlinkat",              // 267
    "fchmodat",                // 268
    "faccessat",               // 269
    "pselect6",                // 270
    "ppoll",                   // 271
    "unshare",                 // 272
    "set_robust_list",         // 273
    "get_robust_list",         // 274
    "splice",                  // 275
    "tee",                     // 276
    "sync_file_range",         // 277
    "vmsplice",                // 278
    "move_pages",              // 279
    "utimensat",               // 280
    "epoll_pwait",             // 281
    "signalfd",                // 282
    "timerfd_create",          // 283
    "eventfd",                 // 284
    "fallocate",               // 285
    "timerfd_settime",         // 286
    "timerfd_gettime",         // 287
    "accept4",                 // 288
    "signalfd4",               // 289
    "eventfd2",                // 290
    "epoll_create1",           // 291
    "dup3",                    // 292
    "pipe2",                   // 293
    "inotify_init1",           // 294
    "preadv",                  // 295
    "pwritev",                 // 296
    "rt_tgsigqueueinfo",       // 297
    "perf_event_open",         // 298
    "recvmmsg",                // 299
    "fanotify_init",           // 300
    "fanotify_mark",           // 301
    "prlimit64",               // 302
    "name_to_handle_at",       // 303
    "open_by_handle_at",       // 304
    "clock_adjtime",           // 305
    "syncfs",                  // 306
    "sendmmsg",                // 307
    "setns",                   // 308
    "getcpu",                  // 309
    "process_vm_readv",        // 310
    "process_vm_writev",       // 311
    "kcmp",                    // 312
    "finit_module",            // 313
    "sched_setattr",           // 314
    "sched_getattr",           // 315
    "renameat2",               // 316
    "seccomp",                 // 317
    "getrandom",               // 318
    "memfd_create",            // 319
    "kexec_file_load",         // 320
    "bpf",                     // 321
    "execveat"                 // 322
};

char **syscall_names[2] = {syscall_names_32, syscall_names_64};
