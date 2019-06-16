#include "server/domjudge/domjudge.hpp"
#include <glog/logging.h>
#include <fmt/core.h>

using namespace std;

namespace judge::server::domjudge {

domjudge_executable_asset::domjudge_executable_asset(const string &id, const filesystem::path &workdir, const string &md5sum)
    : remote_executable_asset(/* TODO */nullptr, md5sum), id(id), workdir(workdir) {}

}