#include "server/redis.hpp"
#include <glog/logging.h>

namespace judge::server {
using namespace std;

static bool connect_to_server(cpp_redis::client &redis_client, const redis &redis_config) {
    redis_client.connect(redis_config.host, redis_config.port,
                         [&](const std::string &host, std::size_t port,
                             cpp_redis::connect_state status) {
                             if (status == cpp_redis::connect_state::dropped) {
                                 LOG(INFO) << "Redis: client disconnected from " << host
                                           << ":" << port << std::endl;
                             }
                             if (status == cpp_redis::connect_state::ok) {
                             }
                         });
    if (!redis_config.password.empty()) {
        LOG(INFO) << "Redis: Trying to Auth";
        auto future = redis_client.auth(redis_config.password);
        redis_client.sync_commit();
        LOG(INFO) << "Redis: Auth Reply: " << future.get();
    }
    if (redis_client.is_connected()) {
        LOG(INFO) << "Redis: connecting to redis server succeeded " << redis_config.host << ", " << redis_config.port;
        return true;
    } else {
        LOG(ERROR) << "Redis: unable to connect to redis server " << redis_config.host << ", " << redis_config.port;
        return false;
    }
}

void redis_conn::reconnect(bool force) {
    int fail = 0;
    if (force)
        connect_to_server(redis_client, redis_config);
    for (; !redis_client.is_connected() && fail < 5; ++fail) {
        LOG(INFO) << "Redis: lost connection, trying to reconnect";
        if (fail > 0)  // 两次重试间隔 5s
            this_thread::sleep_for(chrono::seconds(5));
        connect_to_server(redis_client, redis_config);
    }
    if (fail >= 5) {
        throw std::runtime_error("unable to connect to redis server");
    }
}

void redis_conn::init(const redis &redis_config) noexcept {
    this->redis_config = redis_config;
}

void redis_conn::execute(function<void(cpp_redis::client &, vector<future<cpp_redis::reply>> &)> callback) {
    // cpp_redis 的 is_connected 似乎有问题，最后执行操作时的 reply 仍然是 network error
    // 因此这里也做个强制重连。
    reconnect(false);  // 先弱重连一次
    string message;
    for (int fail = 0; fail < 5; ++fail) {  // 错误尝试至多额外 4 次
        bool reconn = false;
        vector<future<cpp_redis::reply>> replies;
        callback(redis_client, replies);
        redis_client.sync_commit();
        for (auto &reply : replies) {  // 阻塞到所有操作完成为止
            cpp_redis::reply r = reply.get();
            // 如果有操作失败，则标记重试并保存错误信息
            if (!r.ok()) reconn = true, message = r.error;
        }
        if (!reconn) return;
        reconnect(true);  // 操作失败，强制重连
    }
    // 失败次数过多，取消操作
    throw runtime_error("Redis: unable to finish execution: " + message);
}

}  // namespace judge::server