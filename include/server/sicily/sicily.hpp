#pragma once

#include <dbng.hpp>
#include <mysql.hpp>
#include <unordered_map>
#include "server/remote_server.hpp"

namespace judge::server::sicily {
using namespace std;

struct configuration : public judge_server {
    filesystem::path testdata;

    dbng<mysql> db;

    local_executable_manager exec_mgr;

    /**
     * @brief 初始化数据库连接
     * @param 配置文件路径
     * 这个函数用于初始化 sicily 评测有关的配置、数据库连接等
     * 数据库连接信息从环境变量中读取。
     */
    void init(const filesystem::path &config_path) override;

    /**
     * @brief 获取一个提交
     * @param submit 存储该提交的信息
     * @return 返回是否获取到一个提交，如果没有能够获取到一个提交，则应该 sleep(100ms)
     * 该函数可能立即返回不阻塞，此时获取到提交才返回 true；
     * 也可能阻塞到有提交为止，此时返回总是 true。
     */
    bool fetch_submission(submission &submit) override;

    /**
     * @brief 将提交返回给服务器
     * 该函数是阻塞的。评测系统不需要通过多线程来并发写服务器，因为 server 并不会因为
     * 评测过程而阻塞，获取提交和返回评测结果都能很快完成，因此 server 是单线程的。
     */
    void send_judge_status() override;
};

}  // namespace judge::server::sicily