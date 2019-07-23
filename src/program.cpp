#include "program.hpp"
#include <fmt/core.h>
#include <glog/logging.h>
#include <stdlib.h>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include "common/exceptions.hpp"
#include "common/interprocess.hpp"
#include "common/io_utils.hpp"
#include "common/utils.hpp"
#include "config.hpp"
using namespace std;

namespace judge {

compilation_error::compilation_error(const string &what, const string &error_log)
    : runtime_error(what), error_log(error_log) {}

executable_compilation_error::executable_compilation_error(const string &what, const string &error_log)
    : compilation_error(what, error_log) {}

executable::executable(const string &id, const fs::path &workdir, asset_uptr &&asset, const string &md5sum)
    : dir(workdir / "executable" / id), runpath(dir / "compile" / "run"), id(assert_safe_path(id)), md5sum(md5sum), md5path(dir / "md5sum"), deploypath(dir / ".deployed"), buildpath(dir / "compile" / "build"), asset(move(asset)) {
}

void executable::fetch(const string &cpuset, const fs::path &, const fs::path &chrootdir) {
    ip::file_lock file_lock = lock_directory(dir);
    ip::scoped_lock guard(file_lock);
    if (!fs::is_directory(dir) ||
        !fs::is_regular_file(deploypath) ||
        (!md5sum.empty() && !fs::is_regular_file(md5path)) ||
        (!md5sum.empty() && read_file_content(md5path) != md5sum)) {
        fs::remove_all(dir);
        fs::create_directories(dir);

        asset->fetch(dir / "compile");

        if (fs::exists(buildpath)) {
            if (auto ret = call_process(EXEC_DIR / "compile_executable.sh", "-n", cpuset, /* workdir */ dir, chrootdir); ret != 0) {
                switch (ret) {
                    case E_COMPILER_ERROR:
                        throw executable_compilation_error("executable compilation error", get_compilation_log(dir));
                    default:
                        throw internal_error("unknown exitcode " + to_string(ret));
                }
            }
        }

        if (!fs::exists(runpath)) {
            throw executable_compilation_error("executable malformed", "Executable malformed");
        }
    }
    filesystem::permissions(runpath,
                            filesystem::perms::group_exec | filesystem::perms::others_exec | filesystem::perms::owner_exec,
                            filesystem::perm_options::add);
    ofstream to_be_created(deploypath);
}

void executable::fetch(const string &cpuset, const fs::path &chrootdir) {
    fetch(cpuset, {}, chrootdir);
}

string executable::get_compilation_log(const fs::path &) {
    fs::path compilation_log_file(dir / "compile" / "compile.out");
    return read_file_content(compilation_log_file, "No compilation information");
}

filesystem::path executable::get_run_path(const filesystem::path &) {
    return dir / "compile";
}

local_executable_asset::local_executable_asset(const string &type, const string &id, const fs::path &execdir)
    : asset(""), execdir(execdir / assert_safe_path(type) / assert_safe_path(id)) {}

void local_executable_asset::fetch(const fs::path &dir) {
    fs::copy(/* from */ execdir, /* to */ dir, fs::copy_options::recursive);
}

remote_executable_asset::remote_executable_asset(asset_uptr &&remote_asset, const string &md5sum)
    : asset(""), md5sum(md5sum), remote_asset(move(remote_asset)) {}

void remote_executable_asset::fetch(const fs::path &dir) {
    fs::path md5path(dir / "md5sum");
    fs::path deploypath(dir / ".deployed");
    fs::path buildpath(dir / "build");
    fs::path runpath(dir / "run");
    fs::path zippath(dir / "executable.zip");

    remote_asset->fetch(zippath);

    if (!md5sum.empty()) {
        // TODO: check for md5 of executable.zip
    }

    LOG(INFO) << "Unzipping executable " << zippath;

    if (system(fmt::format("unzip -Z '{}' | grep -q ^l", zippath.string()).c_str()) == 0)
        throw executable_compilation_error("Executable contains symlinks", "Unable to unzip executable");

    if (system(fmt::format("unzip -j -q -d {}, {}", dir, zippath).c_str()) != 0)
        throw executable_compilation_error("Unable to unzip executable", "Unable to unzip executable");
}

local_executable_manager::local_executable_manager(const fs::path &workdir, const fs::path &execdir)
    : workdir(workdir), execdir(execdir) {}

unique_ptr<executable> local_executable_manager::get_compile_script(const string &language) const {
    asset_uptr asset = make_unique<local_executable_asset>("compile", language, execdir);
    return make_unique<executable>("compile-" + language, workdir, move(asset));
}

unique_ptr<executable> local_executable_manager::get_run_script(const string &language) const {
    asset_uptr asset = make_unique<local_executable_asset>("run", language, execdir);
    return make_unique<executable>("run-" + language, workdir, move(asset));
}

unique_ptr<executable> local_executable_manager::get_check_script(const string &language) const {
    asset_uptr asset = make_unique<local_executable_asset>("check", language, execdir);
    return make_unique<executable>("check-" + language, workdir, move(asset));
}

unique_ptr<executable> local_executable_manager::get_compare_script(const string &language) const {
    asset_uptr asset = make_unique<local_executable_asset>("compare", language, execdir);
    return make_unique<executable>("compare-" + language, workdir, move(asset));
}

source_code::source_code(executable_manager &exec_mgr)
    : exec_mgr(exec_mgr) {}

void source_code::fetch(const string &cpuset, const fs::path &workdir, const fs::path &chrootdir) {
    vector<string> paths;
    auto compilepath = workdir / "compile";
    fs::create_directories(compilepath);
    for (auto &file : source_files) {
        assert_safe_path(file->name);
        file->fetch(compilepath);
        paths.push_back(file->name);
    }

    for (auto &file : assist_files) {
        assert_safe_path(file->name);
        file->fetch(compilepath);
    }

    if (paths.empty()) {
        // skip program that has no source files
        return;
    }

    auto exec = exec_mgr.get_compile_script(language);
    exec->fetch(cpuset, chrootdir);
    // compile.sh <compile script> <chrootdir> <workdir> <files...>
    if (auto ret = call_process(EXEC_DIR / "compile.sh", "-n", cpuset, /* compile script */ exec->get_run_path(), chrootdir, workdir, /* source files */ paths); ret != 0) {
        switch (ret) {
            case E_COMPILER_ERROR:
                throw compilation_error("Compilation failed", get_compilation_log(workdir));
            case E_INTERNAL_ERROR:
                throw compilation_error("Compilation failed because of internal errors", get_compilation_log(workdir));
            default:
                throw compilation_error(fmt::format("Unrecognized compile.sh exitcode: {}", ret), get_compilation_log(workdir));
        }
    }
}

string source_code::get_compilation_log(const fs::path &workdir) {
    fs::path compilation_log_file(workdir / "compile" / "compile.out");
    return read_file_content(compilation_log_file, "No compilation information");
}

fs::path source_code::get_run_path(const fs::path &path) {
    // run 参见 compile.sh，path 为 $WORKDIR
    return path / "compile";
}

}  // namespace judge
