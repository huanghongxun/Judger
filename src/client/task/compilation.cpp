#include "client/task/compilation.hpp"
#include "server/executable.hpp"

namespace judge::client {

    void compile(const string &cpuset, const filesystem::path &execrunpath, const filesystem::path &workdir, const unique_ptr<judge::server::program> &program){
        program->fetch()
    }

}
