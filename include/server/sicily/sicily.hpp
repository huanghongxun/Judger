#pragma once

#include <unordered_map>
#include "server/judge_server.hpp"
#include "sql/dbng.hpp"
#include "sql/mysql.hpp"

namespace judge::server::sicily {

struct configuration : public judge_server {
    std::filesystem::path testdata;

    ormpp::dbng<ormpp::mysql> db;

    local_executable_manager exec_mgr;

    configuration();

    std::string category() const override;

    /**
     * @brief 初始化数据库连接
     * @param 配置文件路径
     * 这个函数用于初始化 sicily 评测有关的配置、数据库连接等
     * 数据库连接信息从环境变量中读取。
     */
    void init(const std::filesystem::path &config_path) override;

    const executable_manager &get_executable_manager() const override;

    bool fetch_submission(std::unique_ptr<submission> &submit) override;

    void summarize(submission &submit) override;

    void summarize_invalid(submission &submit) override;
};

}  // namespace judge::server::sicily