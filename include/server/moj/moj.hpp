#pragma once

#include "server/judge_server.hpp"
#include "server/redis.hpp"
#include "server/rabbitmq.hpp"
#include "sql/dbng.hpp"
#include "sql/mysql.hpp"

namespace judge::server::moj {

struct configuration : public judge_server {
    local_executable_manager exec_mgr;

    /**
     * @brief 一些全局的配置
     */
    system_config system;

    /**
     * @brief redis 用来存储评测结果
     */
    redis redis_config;

    redis_conn redis_server;

    database dbcfg;

    /**
     * @brief 修改数据库中有关提交的状态
     */
    ormpp::dbng<ormpp::mysql> db;

    /**
     * @brief 拉取选择题评测的消息队列
     */
    amqp choice_queue;

    std::unique_ptr<rabbitmq> choice_fetcher;

    /**
     * @brief 拉取编程题评测的消息队列
     */
    amqp programming_queue;

    std::unique_ptr<rabbitmq> programming_fetcher;

    configuration();

    std::string category() const override;

    /**
     * @brief 初始化消息队列
     * @param config_path 配置文件路径
     * 配置文件格式参考 test/3.0/config-moj.json
     */
    void init(const std::filesystem::path &config_path) override;

    const executable_manager &get_executable_manager() const override;

    bool fetch_submission(std::unique_ptr<submission> &submit) override;

    void summarize(submission &submit) override;

    /**
     * @brief 处理不合法的提交
     * 对于 MOJ，提交都是我们自己构造出来的，不可能存在不合法的提交，如果存在则报错并终止程序
     * @param submit 不合法的提交
     */
    void summarize_invalid(submission &submit) override;
};

}  // namespace judge::server::moj
