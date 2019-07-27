#pragma once

#include "server/judge_server.hpp"

namespace judge::server::mock {

struct configuration : public judge_server {
    /**
     * @brief executable 管理器，负责计算 executable 的下载地址
     * TODO: 需要实现一个配置系统来远程下载 executable
     */
    local_executable_manager exec_mgr;

    std::string category_name;

    configuration();

    std::string category() const override;

    void init(const std::filesystem::path &config_path) override;

    const executable_manager &get_executable_manager() const override;

    bool fetch_submission(std::unique_ptr<submission> &submit) override;

    void summarize(submission &submit) override;

    void summarize_invalid(submission &submit) override;
};

}  // namespace judge::server::mock
