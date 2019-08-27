#pragma once

#include <thread>
#include "common/concurrent_queue.hpp"
#include "common/messages.hpp"
#include "monitor/monitor.hpp"
#include "judge/judger.hpp"
#include "server/judge_server.hpp"

/**
 * 评测服务相关函数
 * task_queue 评测服务端发送评测信息的队列
 * 首先评测系统主线程会先确保评测客户端已经开启并配置好 cpuset。
 * 
 * 然后评测服务端会根据参数，开启 submission fetcher，然后
 * 进入循环不断尝试获取 fetcher。
 * 
 * 每个 worker 都会访问这里的函数，如果遇到评测队列为空的情况，worker 需要调用 fetch_submission 函
 * 数来拉取评测（TODO: 测试潜在的多个 worker 同时访问 fetch_submission 导致评测队列过长的问题）。
 * 在评测完成后，通过调用 judger::process 函数来完成数据点的统计，如果发现评测完了一个提交，则立刻返回。
 * 因此大部分情况下评测队列不会过长：只会拉取适量的评测，确保评测队列不会过长。
 */
namespace judge {

/**
 * @brief 停止所有的 worker
 * 调用该函数后，将 worker 状态标记为停止。worker 循环时会检查标记，
 * 如果停止，则不再拉取新提交，而且在没有评测任务时退出。
 */
void stop_workers();

/**
 * @brief 注册服务端
 * 服务端是指一个可以和数据库连接可以收集选手代码提交的程序，表示
 * 评测系统如何拉取提交、返回评测结果的方式。
 * 可能的服务端比如有 Sicily 评测、Matrix 的 2.0、3.0 评测、GDOI 的评测。
 */
void register_judge_server(std::unique_ptr<server::judge_server> &&judge_server);

/**
 * @brief 注册评测器
 * 评测器负责评测具体题型
 */
void register_judger(std::unique_ptr<judger> &&judger);

/**
 * @brief 注册监控器
 */
void register_monitor(std::unique_ptr<monitor> &&monitor);

/**
 * @brief 向所有的监控器报错
 */
void report_error(const std::string &message);

/**
 * @brief 启动评测 worker 线程
 * 注意评测服务端客户端收发消息直接通过发送指针实现，因此 worker 不能通过 fork
 * 生成。
 * 
 * @param core_id worker 运行的 CPU 核心
 * @param task_queue 评测服务端发送评测信息的队列
 * @param core_queue 核心申请队列，对于多核编程题，获取到提交的 worker 将发送核心获取申请，
 * 其他 worker 发现申请后将当前 CPU 分配给之前申请的 worker，并阻塞当前 worker 的处理直到
 * 该多核提交评测完成为止。
 * @return 产生的线程
 * 
 * 选手代码、测试数据、随机数据生成器、标准程序、SPJ 等资源的
 * 下载均由客户端完成。服务端只完成提交的拉取和数据点的分发。
 */
std::thread start_worker(size_t core_id, concurrent_queue<message::client_task> &task_queue, concurrent_queue<message::core_request> &core_queue);

}  // namespace judge
