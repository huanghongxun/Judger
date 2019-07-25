#pragma once

#include "server/judge_server.hpp"
#include "server/submission_fetcher.hpp"
#include "sql/dbng.hpp"
#include "sql/mysql.hpp"

namespace judge::server::forth {

struct configuration : public judge_server {
    /**
     * @brief executable 管理器，负责计算 executable 的下载地址
     * TODO: 需要实现一个配置系统来远程下载 executable
     */
    local_executable_manager exec_mgr;

    amqp sub_queue;
    std::unique_ptr<submission_fetcher> sub_fetcher;

    amqp report_queue;
    std::unique_ptr<judge_result_reporter> judge_reporter;

    std::string category_name;

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
     * @param submit 不合法的提交
     */
    void summarize_invalid(submission &submit) override;
};

}  // namespace judge::server::mcourse
