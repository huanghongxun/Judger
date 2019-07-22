#include "server/common/redis.hpp"
#include <glog/logging.h>

namespace judge::server {

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

void redis_conn::init(const redis &redis_config) {
    this->redis_config = redis_config;
}

void redis_conn::execute(function<void(cpp_redis::client &)> callback) {
    int fail = 0;
    for (; !redis_client.is_connected() && fail < 5; ++fail)
        connect_to_server(redis_client, redis_config);
    if (fail == 5) {
        throw std::runtime_error("unable to connect to redis server");
    }
    callback(redis_client);
    redis_client.sync_commit();
}

}  // namespace judge::server