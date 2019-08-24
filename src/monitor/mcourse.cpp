#include "monitor/mcourse.hpp"
#include <fmt/core.h>

namespace judge {
using namespace std;

mcourse_monitor::mcourse_monitor(const server::database &monitor_dbcfg, const std::string &host, int port) : host(host), port(port) {
    db.connect(monitor_dbcfg.host.c_str(), monitor_dbcfg.user.c_str(), monitor_dbcfg.password.c_str(), monitor_dbcfg.database.c_str());

    db.execute("INSERT INTO judge_node_config (host, port, thread_number, is_working, load_factor) VALUES (?, ?, 0, true, 0) ON DUPLICATE KEY UPDATE host=?, port=?, load_factor=0, thread_number=0",
               host, port, host, port);
}

void mcourse_monitor::worker_state_changed(int worker_id, worker_state state, const std::string &info) {
    switch (state) {
        case worker_state::START:
            db.execute("INSERT INTO judge_worker_status (host, worker_id, worker_type, is_running, worker_stage) VALUES (?, ?, 'Universal', true, 'idle') ON DUPLICATE KEY UPDATE is_running=true, worker_stage='idle', worker_type='Universal'",
                       host, worker_id);
            db.execute("UPDATE judge_node_config SET load_factor=load_factor+1 WHERE host=?",
                       host);
            break;
        case worker_state::JUDGING:
            db.execute("UPDATE judge_worker_status SET worker_stage='judging' WHERE host=? AND worker_id=?",
                       host, worker_id);
            db.execute("UPDATE judge_node_config SET thread_number=thread_number+1 WHERE host=?",
                       host);
            break;
        case worker_state::IDLE:
            db.execute("UPDATE judge_worker_status SET worker_stage='idle' WHERE host=? AND worker_id=?",
                       host, worker_id);
            db.execute("UPDATE judge_node_config SET thread_number=thread_number-1 WHERE host=?",
                       host);
            break;
        case worker_state::STOPPED:
            db.execute("UPDATE judge_worker_status SET is_running=false, worker_stage='down' WHERE host=? AND worker_id=?",
                       host, worker_id);
            db.execute("UPDATE judge_node_config SET load_factor=load_factor-1 WHERE host=?",
                       host);
            break;
        case worker_state::CRASHED:
            db.execute("UPDATE judge_worker_status SET is_running=false, worker_stage='crashed' WHERE host=? AND worker_id=?",
                       host, worker_id);
            db.execute("UPDATE judge_node_config SET thread_number=thread_number-1, load_factor=load_factor-1 WHERE host=?",
                       host);
            break;
    }
}

}  // namespace judge
