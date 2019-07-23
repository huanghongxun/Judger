#pragma once

#include <cpp_redis/cpp_redis>
#include "server/config.hpp"

namespace judge::server {
using namespace std;

struct redis_conn {
    void init(const redis &redis_config);

    void execute(function<void(cpp_redis::client &)> callback);

private:
    redis redis_config;
    cpp_redis::client redis_client;
};

}  // namespace judge::server
