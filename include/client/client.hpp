#pragma once

#include <set>
#include <thread>
#include "common/concurrent_queue.hpp"
#include "common/messages.hpp"

namespace judge::client {
using namespace std;

/**
 * @brief 评测客户端程序函数
 * 评测客户端负责从消息队列中获取评测服务端要求评测的数据点，
 * 数据点信息包括时间限制、测试数据、选手代码等信息。
 * @param execcpuset 选手程序运行的 cpuset
 * @param testcase_queue 评测服务端发送评测信息的队列
 * 
 * 选手代码、测试数据、随机数据生成器、标准程序、SPJ 等资源的
 * 下载均由客户端完成。服务端只完成提交的拉取和数据点的分发。
 * 
 * 文件组织结构：
 * 对于需要进行缓存的文件：
 *     CACHE_DIR
 */
void client(const string &execcpuset, concurrent_queue<message::client_task> &testcase_queue);

/**
 * @brief 启动评测客户端线程
 * 注意评测服务端客户端收发消息直接通过发送指针实现，因此客户端不能通过 fork
 * 生成。
 * 
 * @param set 评测客户端运行的 cpuset（一般设置为和评测服务端同一个核心）
 * @param execcpuset 选手程序运行的 cpuset
 * @param testcase_queue 评测服务端发送评测信息的队列
 * @return 产生的线程
 * 
 * 选手代码、测试数据、随机数据生成器、标准程序、SPJ 等资源的
 * 下载均由客户端完成。服务端只完成提交的拉取和数据点的分发。
 */
thread start_client(const cpu_set_t &set, const string &execcpuset, concurrent_queue<message::client_task> &testcase_queue);

}  // namespace judge::client
