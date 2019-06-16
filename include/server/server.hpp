#pragma once

#include "msg_queue.hpp"
#include "server/remote_server.hpp"

namespace judge::server {

void register_judge_server(unique_ptr<judge_server> &&judge_server);

submission &get_submission_by_judge_id(unsigned judge_id);

/**
 * @brief 评测服务端主程序运行函数
 * @param testcase_queue 评测服务端发送评测信息的队列
 * @param result_queue 评测客户端发送评测结果的队列，评测服务端负责统计评测结果并返回最终的评测结果给服务端
 * 首先评测服务端会先确保评测客户端已经开启并配置好 cpuset,
 * 开启评测客户端时需要确保服务端运行在与评测客户端不同的核
 * 心上。
 * 
 * 然后评测服务端会根据参数，开启 submission fetcher，然后
 * 进入循环不断尝试获取 fetcher。
 * 
 * 为了避免多线程，这里通过单线程单循环轮询各个 submission
 * fetcher，检查是否存在提交可以评测，如果存在且 client 充
 * 足时则评测（提交到消息队列（这个消息队列和 RabbitMQ 的无
 * 关）中，否则等待 10ms 再轮询一次）
 */
void server(message::queue &testcase_queue, message::queue &result_queue);
}  // namespace judge::server