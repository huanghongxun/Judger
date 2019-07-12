#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include "server/moj/submission_fetcher.hpp"
#include "server/judge_server.hpp"

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;

struct configuration : public judge_server {
    local_executable_manager exec_mgr;

    /**
     * @brief 一些全局的配置
     */
    system_config system;

    /**
     * @brief 返回评测结果的消息队列
     */
    amqp submission_report;

    unique_ptr<judge_result_reporter> submission_reporter;

    /**
     * @brief 拉取选择题评测的消息队列
     */
    amqp choice_submission;

    unique_ptr<submission_fetcher> choice_fetcher;

    /**
     * @brief 拉取编程题评测的消息队列
     */
    amqp programming_submission;

    unique_ptr<submission_fetcher> programming_fetcher;

    configuration();

    string category() const override;

    /**
     * @brief 初始化消息队列
     */
    void init(const filesystem::path &config_path) override;

    const executable_manager &get_executable_manager() const override;

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
     * @param submit 该提交的信息
     * @param task_results 评测结果
     */
    void summarize(submission &submit, const vector<judge::message::task_result> &task_results) override;

    /**
     * @brief 处理不合法的提交
     * 对于 sicily，不可能存在不合法的提交，如果存在则报错并终止程序
     * @param submit 不合法的提交
     */
    void summarize_invalid(submission &submit) override;
};

}  // namespace judge::server::moj
