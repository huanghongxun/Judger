#pragma once

#include "server/executable.hpp"

namespace judge::server::domjudge {
using namespace std;

struct domjudge_executable_asset : public remote_executable_asset {
    filesystem::path workdir;
    string id;

    domjudge_executable_asset(const string &id, const filesystem::path &workdir, const string &md5sum);
};
}  // namespace judge::server::domjudge