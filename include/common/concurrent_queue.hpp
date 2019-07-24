#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

namespace judge {

/**
 * @brief 并发队列，写者读者模型
 * @param <T> 队列元素类型
 */
template <typename T>
struct concurrent_queue {
    /**
     * @brief 尝试从队列中弹出队头元素，如果队列为空返回 false
     * @param element 如果队列有元素，则保存队头元素，否则不变
     * @return 是否成功弹出队列头元素
     */
    bool try_pop(T &element) {
        std::unique_lock<std::mutex> mlock(mut);
        if (q.empty()) return false;
        element = q.front();
        q.pop();
        return true;
    }

    /**
     * @brief 从队列中弹出队头元素，如果队列为空则阻塞等待直到有元素为止
     * @return 队列头元素
     */
    T pop() {
        std::unique_lock<std::mutex> mlock(mut);
        while (q.empty()) cond.wait(mlock);
        auto result = q.front();
        q.pop();
        return result;
    }

    /**
     * @brief 向队列中插入一个新元素
     */
    void push(const T &value) {
        std::unique_lock<std::mutex> mlock(mut);
        q.push(value);
        mlock.unlock();
        cond.notify_one();
    }

private:
    std::queue<T> q;
    std::mutex mut;
    std::condition_variable cond;
};

}  // namespace judge
