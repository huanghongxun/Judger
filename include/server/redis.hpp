#pragma once

#include <cpp_redis/cpp_redis>
#include <vector>
#include "server/config.hpp"

namespace judge::server {

/**
 * @brief 表示一个 Redis 连接
 */
struct redis_conn {
    /**
     * @brief 根据 Redis 配置初始化 Redis 服务器连接
     */
    void init(const redis &redis_config) noexcept;

    /**
     * @brief 在 callback 内发送 Redis 的操作
     * 该函数负责确保 Redis 连接会被建立。
     * 如果 Redis 服务器主动断开连接，那么这个函数将尝试重新创建连接，
     * 如果重试次数过多则抛出异常。
     * @param callback 你可以在 callback 内完成 Redis 的操作
     */
    void execute(std::function<void(cpp_redis::client &, std::vector<std::future<cpp_redis::reply>> &)> callback);

    /**
     * @brief 尝试重连
     * @param force 真时强制重连
     */
    void reconnect(bool force = false);

private:
    redis redis_config;
    cpp_redis::client redis_client;
};

}  // namespace judge::server
