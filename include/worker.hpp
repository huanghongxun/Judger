#pragma once

#include <thread>
#include "common/concurrent_queue.hpp"
#include "common/messages.hpp"
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
using namespace std;

/**
 * @brief 注册服务端
 * 服务端是指一个可以和数据库连接可以收集选手代码提交的程序，表示
 * 评测系统如何拉取提交、返回评测结果的方式。
 * 可能的服务端比如有 Sicily 评测、Matrix 的 2.0、3.0 评测、GDOI 的评测。
 */
void register_judge_server(unique_ptr<server::judge_server> &&judge_server);

server::judge_server &get_judge_server_by_category(const string &category);

void register_judger(unique_ptr<judger> &&judger);

judger &get_judger_by_type(const string &type);

/**
 * @brief 启动评测客户端线程
 * 注意评测服务端客户端收发消息直接通过发送指针实现，因此客户端不能通过 fork
 * 生成。
 * 
 * @param set 评测客户端运行的 cpuset（一般设置为和评测服务端同一个核心）
 * @param execcpuset 选手程序运行的 cpuset
 * @param task_queue 评测服务端发送评测信息的队列
 * @return 产生的线程
 * 
 * 选手代码、测试数据、随机数据生成器、标准程序、SPJ 等资源的
 * 下载均由客户端完成。服务端只完成提交的拉取和数据点的分发。
 */
thread start_worker(int worker_id, const cpu_set_t &set, const string &execcpuset, concurrent_queue<message::client_task> &task_queue);

}  // namespace judge
