#pragma once

#include <filesystem>
#include "common/messages.hpp"
#include "common/status.hpp"
#include "config.hpp"
#include "program.hpp"

namespace judge::server {

/**
 * @brief 表示一个远程服务器
 * 比如可以表示 MOJ 的评测类型，运行时可以注册多个远程服务器拉取提交
 */
struct judge_server {
    virtual ~judge_server();

    /**
     * @brief 评测服务器的 id，用于标记 submission 是哪个
     * 远程服务器拉取的提交。
     * 
     * @return string 评测服务器的 id
     */
    virtual std::string category() const = 0;

    /**
     * @brief 初始化评测服务器连接
     * @param 配置文件路径
     * 你可以在这里通过解析配置文件来建立消息队列、建立数据库连接
     * 配置文件的格式自定义，没有任何约定，但建议使用 JSON
     */
    virtual void init(const std::filesystem::path &config_path) = 0;

    /**
     * @brief 获取一个提交
     * @param submit 存储该提交的信息
     * @return 返回是否获取到一个提交，如果没有能够获取到一个提交，则应该 sleep(10ms)
     * 该函数可能立即返回不阻塞，此时获取到提交才返回 true；
     * 也可能阻塞到有提交为止，此时返回总是 true。
     */
    virtual bool fetch_submission(std::unique_ptr<submission> &submit) = 0;

    /**
     * @brief 将提交返回给服务器
     * 调用方必须确保该函数是原子性的，这样该函数不需要考虑 submission 的同步问题
     * @param submit 该提交的信息
     */
    virtual void summarize(submission &submit) = 0;

    /**
     * @brief 处理不合法的提交
     * @param submit 不合法的提交
     */
    virtual void summarize_invalid(submission &submit) = 0;

    /**
     * @brief 获取服务器对应的 executable manager
     */
    virtual const executable_manager &get_executable_manager() const = 0;
};

}  // namespace judge::server
