#pragma once

#include "monitor/monitor.hpp"
#include "server/config.hpp"
#include "sql/dbng.hpp"
#include "sql/mysql.hpp"

namespace judge {

/**
 * @brief 向课程系统报告当前 worker 的状态
 * 提供给 Matrix 课程系统用于监控评测系统状态
 */
struct mcourse_monitor : public monitor {
    mcourse_monitor(const judge::server::database &monitor_dbcfg, const std::string &host, int port);

    void worker_state_changed(int worker_id, worker_state state, const std::string &information) override;

private:
    ormpp::dbng<ormpp::mysql> db;
    std::string host;
    int port;
};

}  // namespace judge
