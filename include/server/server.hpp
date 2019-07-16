#pragma once

#include "common/concurrent_queue.hpp"
#include "common/messages.hpp"
#include "server/judge_server.hpp"

/**
 * 评测服务相关函数
 * testcase_queue 评测服务端发送评测信息的队列
 * result_queue 评测客户端发送评测结果的队列，评测服务端负责统计评测结果并返回最终的评测结果给服务端
 * 首先评测系统主线程会先确保评测客户端已经开启并配置好 cpuset。
 * 
 * 然后评测服务端会根据参数，开启 submission fetcher，然后
 * 进入循环不断尝试获取 fetcher。
 * 
 * 每个 client 都会访问这里的函数，如果遇到评测队列为空的情况，client 需要调用 fetch_submission 函
 * 数来拉取评测（TODO: 测试潜在的多个 client 同时访问 fetch_submission 导致评测队列过长的问题）。
 * 在评测完成后，通过调用 process 函数来完成数据点的统计，如果发现评测完了一个提交，则立刻返回。
 * 因此大部分情况下评测队列不会过长：只会拉取适量的评测，确保评测队列不会过长。
 */
namespace judge::server {

/**
 * @brief 注册服务端
 * 服务端是指一个可以和数据库连接可以收集选手代码提交的程序，表示
 * 评测系统如何拉取提交、返回评测结果的方式。
 * 可能的服务端比如有 Sicily 评测、Matrix 的 2.0、3.0 评测、GDOI 的评测。
 */
void register_judge_server(unique_ptr<judge_server> &&judge_server);

judge_server &get_judge_server_by_category(const string &category);

/**
 * @brief 根据 judge_id 获得 submission 信息
 * 这个函数供评测客户端使用，评测客户端拉取评测队列只能拿到 judge_id，
 * 更加详细的信息需要使用这个函数获取。
 * @param judge_id 评测任务在评测系统中的 id
 */
submission &get_submission_by_judge_id(unsigned judge_id);

/**
 * @brief 完成评测结果的统计，如果统计的是编译任务，则会分发具体的评测任务
 * 在评测完成后，通过调用 process 函数来完成数据点的统计，如果发现评测完了一个提交，则立刻返回。
 * 因此大部分情况下评测队列不会过长：只会拉取适量的评测，确保评测队列不会过长。
 * 
 * @param task_result 评测结果
 */
void process(concurrent_queue<message::client_task> &testcase_queue, const message::task_result &task_result);

/**
 * @brief 向每个评测服务器拉取一个提交
 * 如果遇到评测队列为空的情况，client 需要调用 fetch_submission 函数来拉取评测
 * （TODO: 测试潜在的多个 client 同时访问 fetch_submission 导致评测队列过长的问题）。
 * 
 * @param testcase_queue 评测服务端发送评测信息的队列
 * @return true 如果获取到了提交
 */
bool fetch_submission(concurrent_queue<message::client_task> &testcase_queue);

}  // namespace judge::server