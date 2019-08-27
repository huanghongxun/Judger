#pragma once

#include <boost/python.hpp>
#include <filesystem>
#include "monitor/monitor.hpp"

namespace judge {

struct elastic : public monitor {
    elastic(const std::filesystem::path &module_dir);

    void start_submission(const submission &judge_id) override;

    void start_judge_task(int worker_id, const message::client_task &client_task) override;

    void end_judge_task(int worker_id, const message::client_task &client_task) override;

    void worker_state_changed(int worker_id, worker_state state, const std::string &message) override;

    void report_error(const std::string &message) override;

    void end_submission(const submission &judge_id) override;

private:
    /**
     * @brief 表示一个 Python 的 contextvar
     * 因为 ElasticAPM 的 Python 实现采用 contextvar 来分离上下文，
     * 而且评测系统是并发评测的，上报监控信息的行为也是并发的，而 Python
     * 本身是单线程的，因此我们调用 Python 代码时需要恢复上下文再返回监控信息。
     */
    struct context {
        /**
         * contextvars 上下文
         * elasticapm 使用 contextvars 来存储 transaction，
         * 因此每次操作我们都需要恢复上下文来操作对应的 transaction
         */
        boost::python::object ctx;

        /**
         * elasticapm.span 对象
         * 表示一个 transaction 的子操作，这里表示一个评测子任务
         */
        std::map<unsigned, boost::python::object> spans;
    };

    std::map<unsigned, context> contexts;
    boost::python::object apmclient_module;
    boost::python::object apmclient_namespace;
};

}  // namespace judge
