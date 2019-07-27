#pragma once

#include "common/concurrent_queue.hpp"
#include "common/messages.hpp"
#include "judge/judger.hpp"

/**
 * 测试用的 worker
 * 用法：
 * 1. concurrent_queue<message::client_task> queue;
 * 2. push_submission(your test judger, queue, your submission);
 * 3. worker_loop(your test judger, queue)
 * 4. check validity of submission
 */
namespace judge {

void push_submission(const judger &j, concurrent_queue<message::client_task> &task_queue, submission &submit);

void worker_loop(const judger &j, concurrent_queue<message::client_task> &task_queue);

void setup_test_environment();

}  // namespace judge
