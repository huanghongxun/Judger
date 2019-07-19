#include "server/mcourse/mcourse.hpp"
#include <glog/logging.h>
#include <boost/assign.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include "common/io_utils.hpp"
#include "common/messages.hpp"
#include "common/stl_utils.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "server/common/config.hpp"
#include "server/mcourse/choice.hpp"
#include "server/mcourse/feedback.hpp"

namespace judge::server::mcourse {
using namespace std;
using namespace nlohmann;

// clang-format off
const unordered_map<status, const char *> status_string = boost::assign::map_list_of
    (status::PENDING, "")
    (status::RUNNING, "")
    (status::ACCEPTED, "OK")
    (status::COMPILATION_ERROR, "CE")
    (status::WRONG_ANSWER, "WA")
    (status::RUNTIME_ERROR, "RE")
    (status::TIME_LIMIT_EXCEEDED, "TL")
    (status::MEMORY_LIMIT_EXCEEDED, "ML")
    (status::OUTPUT_LIMIT_EXCEEDED, "OL")
    (status::PRESENTATION_ERROR, "PE")
    (status::RESTRICT_FUNCTION, "RF")
    (status::OUT_OF_CONTEST_TIME, "") // Too Late
    (status::COMPILING, "")
    (status::SEGMENTATION_FAULT, "RE")
    (status::FLOATING_POINT_ERROR, "RE");
// clang-format on

configuration::configuration()
    : exec_mgr(CACHE_DIR, EXEC_DIR) {}

void configuration::init(const filesystem::path &config_path) {
    if (!filesystem::exists(config_path))
        throw runtime_error("Unable to find configuration file");
    ifstream fin(config_path);
    json config;
    fin >> config;

    database matrix_dbcfg;
    config.at("matrix_db").get_to(matrix_dbcfg);
    matrix_db.connect(matrix_dbcfg.host.c_str(), matrix_dbcfg.user.c_str(), matrix_dbcfg.password.c_str(), matrix_dbcfg.database.c_str());

    database monitor_dbcfg;
    config.at("monitor_db").get_to(monitor_dbcfg);
    monitor_db.connect(monitor_dbcfg.host.c_str(), monitor_dbcfg.user.c_str(), monitor_dbcfg.password.c_str(), monitor_dbcfg.database.c_str());

    config.at("submission_queue").get_to(sub_queue);
    config.at("file_api").get_to(file_api);

    sub_fetcher = make_unique<submission_fetcher>(sub_queue);
}

string configuration::category() const {
    return "mcourse";
}

const executable_manager &configuration::get_executable_manager() const {
    return exec_mgr;
}

static function<asset_uptr(const string &)> moj_url_to_remote_file(configuration &config, const string &subpath) {
    return [&](const string &name) {
        return make_unique<remote_asset>(name, config.file_api + "/" + subpath + "/" + name);
    };
}

bool configuration::fetch_submission(submission &submit) {
    return false;
}

void configuration::summarize_invalid(submission &) {
    throw runtime_error("Invalid submission");
}

void configuration::summarize(submission &submit, const vector<judge::message::task_result> &task_results) {
}

}  // namespace judge::server::mcourse
