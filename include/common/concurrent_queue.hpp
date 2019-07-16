#pragma once

#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>

namespace judge {
using namespace std;
using namespace nlohmann;

template <typename T>
struct concurrent_queue {
    bool try_pop(T &element) {
        unique_lock<mutex> mlock(mut);
        if (q.empty()) return false;
        element = q.front();
        q.pop();
        return true;
    }

    T pop() {
        unique_lock<mutex> mlock(mut);
        while (q.empty()) cond.wait(mlock);
        auto result = q.front();
        q.pop();
        return result;
    }

    void push(const T &value) {
        unique_lock<mutex> mlock(mut);
        q.push(value);
        mlock.unlock();
        cond.notify_one();
    }

private:
    queue<T> q;
    mutex mut;
    condition_variable cond;
};

}  // namespace judge
