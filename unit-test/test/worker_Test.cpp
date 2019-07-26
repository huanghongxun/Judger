#include "test/worker.hpp"
#include <gtest/gtest.h>

namespace judge {

void push_submission(const judger &j, concurrent_queue<message::client_task> &task_queue, submission &submit) {
    EXPECT_TRUE(j.verify(submit));
    EXPECT_TRUE(j.distribute(task_queue, submit));
}

void worker_loop(const judger &j, concurrent_queue<message::client_task> &task_queue) {
    while (true) {
        message::client_task task;
        if (task_queue.try_pop(task)) break;

        j.judge(task, task_queue, "0");
    }
}

}  // namespace judge
