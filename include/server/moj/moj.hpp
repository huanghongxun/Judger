#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include "server/remote_server.hpp"

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;

/**
 * @brief 描述一个 AMQP 消息队列的配置数据结构
 */
struct amqp {
    /**
     * @brief AMQP 消息队列的主机地址
     */
    string hostname;

    /**
     * @brief AMQP 消息队列的主机端口
     */
    int port;

    /**
     * @brief 通过该结构体发送的消息的 Exchange 名
     */
    string exchange;

    /**
     * @brief AMQP 消息队列的队列名
     */
    string queue;
};

/**
 * @brief 时间限制配置
 */
struct time_limit {

    /**
     * @brief 编译时间限制（单位为秒）
     */
    float compile_time_limit;

    /**
     * @brief 静态检查时限（单位为秒）
     */
    float oclint;
    
    /**
     * @brief 
     */
    float crun;
    
    /**
     * @brief 内存检查时限（单位为秒）
     */
    float valgrind;
    
    /**
     * @brief 随机数据生成器运行时限（单位为秒）
     */
    float random_generator;
};

struct system_config {
    time_limit time_limit;

    size_t max_report_io_size;
};

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

    /**
     * @brief 拉取选择题评测的消息队列
     */
    amqp choice_submission;
    
    /**
     * @brief 拉取编程题评测的消息队列
     */
    amqp programming_submission;

    /**
     * @brief 初始化消息队列
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
    void summarize() override;
};

void from_json(const json &j, system_config &config);
void from_json(const json &j, amqp &mq);
void from_json(const json &j, time_limit &limit);

void from_json_moj(const json &j, configuration &server, judge::server::submission &submit);
void to_json_moj(const json &j, configuration &server, judge::);

extern const unordered_map<status, const char *> status_string;
}  // namespace judge::server::moj