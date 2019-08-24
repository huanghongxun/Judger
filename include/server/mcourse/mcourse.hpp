#pragma once

#include "server/judge_server.hpp"
#include "server/rabbitmq.hpp"
#include "sql/dbng.hpp"
#include "sql/mysql.hpp"

namespace judge::server::mcourse {

struct configuration : public judge_server {
    local_executable_manager exec_mgr;

    /**
     * @brief 一些全局的配置
     */
    system_config system;

    database matrix_dbcfg;

    /**
     * @brief Matrix 数据库
     */
    ormpp::dbng<ormpp::mysql> matrix_db;

    database monitor_dbcfg;

    amqp sub_queue;

    std::unique_ptr<rabbitmq> sub_fetcher;

    configuration();

    std::string category() const override;

    /**
     * @brief 初始化消息队列和数据库连接
     */
    void init(const std::filesystem::path &config_path) override;

    const executable_manager &get_executable_manager() const override;

    /**
     * @brief 获取一个提交
     * @param submit 存储该提交的信息
     * @return 返回是否获取到一个提交，如果没有能够获取到一个提交，则应该 sleep(100ms)
     * 该函数可能立即返回不阻塞，此时获取到提交才返回 true；
     * 也可能阻塞到有提交为止，此时返回总是 true。
     */
    bool fetch_submission(std::unique_ptr<submission> &submit) override;

    /**
     * @brief 将提交返回给服务器
     * 该函数是阻塞的。评测系统不需要通过多线程来并发写服务器，因为 server 并不会因为
     * 评测过程而阻塞，获取提交和返回评测结果都能很快完成，因此 server 是单线程的。
     * @param submit 该提交的信息
     * @param judge_task_results 评测结果
     */
    void summarize(submission &submit) override;

    /**
     * @brief 处理不合法的提交
     * 对于 mcourse，提交格式都是生成的，不可能存在不合法的提交，如果存在则报错并终止程序
     * @param submit 不合法的提交
     */
    void summarize_invalid(submission &submit) override;
};

}  // namespace judge::server::mcourse
