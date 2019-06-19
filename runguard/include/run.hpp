#pragma once

#include "runguard_options.hpp"

/**
 * @brief 根据传入的设置运行指定的程序
 * @note 该函数必须在 main 函数最后调用，或者 fork 出一个新进程再调用本函数
 * 1. 注册 SIGCHLD 来监听子进程的信号
 * 2. 创建 cgroup，并注册 cpu、memory 资源管控器，限制 CPU 核心、内存使用
 * 3. 分离 FD、FS、IPC、NET、NS、UTS、SYSVSEM 等命名空间，限制子进程树的访问权
 * 4. 检查 runguard 进程的内存限制
 * 5. 调用 fork 创建子进程，并等待子进程结束
 *    1. 对于父进程
 *       1. 监听子进程的 SIGALRM 来进行时间限制、SIGTERM 来清除进程树
 *       2. 创建 itimer 来限制 real time，并注册 sigaction，在遇到 SIGALRM 时终止子进程并记录信息到 meta 文件中
 *       3. 与子进程建立管道连接，必要时重定向到文件
 *       4. 等待子进程结束
 *    2. 对于子进程，添加子进程资源限制，并与父进程建立管道重定向输入输出
 *       1. 必要时清除 PATH 环境变量
 *       2. 通过 rlimit 限制 CPU time
 *       3. 通过 rlimit 给予无限大的栈空间
 *       4. 将子进程挂载到我们创建的 cgroup 上，限制 CPU 核心、内存使用
 *       5. 设置 chroot 和工作路径
 *       6. 设置子进程的 user 和 group 以允许文件访问权限限制
 *       7. 将子进程分离到一个独立的进程组，以便我们通过 SIGKILL 可以杀死进程组内所有进程
 * 6. 检查子进程是否正常退出
 *     1. 若因为信号终止，且为 SIGXCPU 则 Time Limit Exceeded，否则为 Runtime Error
 *     2. 若因为信号停止，返回 Runtime Error
 * 7. 读取 cgroup 的监测数据，得到运行时间、内存使用
 * 8. 杀死 cgroup 内的所有进程确保选手 fork 出来的子进程都不会留驻系统
 * 9. 删除创建的 cgroup，并记录所有的信息到 meta 文件中
 */
int runit(struct runguard_options opt);
