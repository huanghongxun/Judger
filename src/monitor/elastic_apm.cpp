#include "monitor/elastic_apm.hpp"
#include <fmt/core.h>
#include "common/python.hpp"

namespace judge {
using namespace std;
namespace bp = boost::python;

elastic::elastic(const filesystem::path &module_dir) {
    if (!getenv("ELASTIC_APM_SERVICE_NAME"))
        return;

    GIL_guard guard;

    try {
        auto sys = bp::import("sys");
        sys.attr("path").attr("append")(module_dir.string());
        apmclient_module = bp::import("apmclient");
        apmclient_namespace = apmclient_module.attr("__dict__");
    } catch (...) {
        PyErr_Print();
        throw;
    }
}

void elastic::start_submission(const submission &submit) {
    if (!apmclient_module) return;
    GIL_guard guard;

    context ctx;
    ctx.ctx = apmclient_module.attr("start_submission")();
    contexts[submit.judge_id] = ctx;
}

void elastic::start_judge_task(int worker_id, const message::client_task &client_task) {
    if (!apmclient_module) return;
    GIL_guard guard;

    context &ctx = contexts[client_task.submit->judge_id];
    ctx.spans[client_task.id] = apmclient_module.attr("start_judge_task")(ctx.ctx, fmt::format("{}: {}", worker_id, client_task.name));
}

void elastic::end_judge_task(int, const message::client_task &client_task) {
    if (!apmclient_module) return;
    GIL_guard guard;

    context &ctx = contexts[client_task.submit->judge_id];
    apmclient_module.attr("end_judge_task")(ctx.ctx, ctx.spans[client_task.id]);
}

void elastic::worker_state_changed(int worker_id, worker_state state, const std::string &info) {
    if (state == worker_state::CRASHED) {
        if (!apmclient_module) return;
        GIL_guard guard;

        apmclient_module.attr("report_crash")(fmt::format("Worker {} has crashed: {}", worker_id, info));
    }
}

void elastic::end_submission(const submission &submit) {
    if (!apmclient_module) return;
    GIL_guard guard;

    context &ctx = contexts[submit.judge_id];
    apmclient_module.attr("end_submission")(ctx.ctx, submit.category, submit.result);
}

}  // namespace judge
