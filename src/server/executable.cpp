#include "server/executable.hpp"
#include <fmt/core.h>
#include <glog/logging.h>
#include <stdlib.h>
#include <fstream>
#include <stdexcept>
#include "config.hpp"
#include "utils.hpp"
using namespace std;
using namespace judge::server;

executable::executable(const string &id, const filesystem::path &workdir, asset_uptr &&asset, const string &md5sum)
    : asset(move(asset)), dir(workdir / "executable" / id), id(id), md5path(dir / "md5sum"), deploypath(dir / ".deployed"), buildpath(dir / "build"), runpath(dir / "run"), md5sum(md5sum) {
}

void executable::compile(const string &cpuset, const filesystem::path &, const filesystem::path &chrootdir) {
    if (!filesystem::is_directory(dir) ||
        !filesystem::is_regular_file(deploypath) ||
        (!md5sum.empty() && !filesystem::is_regular_file(md5path)) ||
        (!md5sum.empty() && read_file_content(md5path) != md5sum)) {
        filesystem::remove_all(dir);
        filesystem::create_directories(dir);

        asset->fetch(dir);

        if (filesystem::exists(buildpath)) {
            filesystem::path olddir = filesystem::current_path();
            filesystem::current_path(dir);
            // TODO: build in runguard, or no compile environment
            if (system("./build") != 0) {
                throw runtime_error("compilation error");
            }
            filesystem::current_path(olddir);
        }

        if (!filesystem::exists(runpath)) {
            throw runtime_error("malformed");
        }
    }
    ofstream to_be_created(deploypath);
}

void executable::fetch(const filesystem::path &) {
}

local_executable_asset::local_executable_asset(const string &type, const string &id, const filesystem::path &execdir)
    : asset(""), execdir(execdir / type / id) {}

void local_executable_asset::fetch(const filesystem::path &dir) {
    filesystem::copy(execdir, dir, filesystem::copy_options::recursive);
}

remote_executable_asset::remote_executable_asset(asset_uptr &&remote_asset, const string &md5sum)
    : asset(""), remote_asset(move(remote_asset)), md5sum(md5sum) {}

void remote_executable_asset::fetch(const filesystem::path &dir) {
    filesystem::path md5path(dir / "md5sum");
    filesystem::path deploypath(dir / ".deployed");
    filesystem::path buildpath(dir / "build");
    filesystem::path runpath(dir / "run");
    filesystem::path zippath(dir / "executable.zip");

    remote_asset->fetch(zippath);

    if (!md5sum.empty()) {
        // TODO: check for md5 of executable.zip
    }

    LOG(INFO) << "Unzipping executable " << zippath;

    if (system(fmt::format("unzip -Z '{}' | grep -q ^l", zippath.string()).c_str()) == 0)
        throw invalid_argument("Executable contains symlinks");

    if (system(fmt::format("unzip -j -q -d {}, {}", dir, zippath).c_str()) != 0)
        throw runtime_error("Unable to unzip executable");
}

local_executable_manager::local_executable_manager(const filesystem::path &execdir, const filesystem::path &workdir)
    : workdir(workdir), execdir(execdir) {}

executable local_executable_manager::get_compile_script(const string &language) const {
    asset_uptr asset = make_unique<local_executable_asset>("compile", language, workdir, execdir);
    executable exec("compile-" + language, workdir, move(asset));

    return exec;
}

executable local_executable_manager::get_run_script(const string &language) const {
    asset_uptr asset = make_unique<local_executable_asset>("run", language, workdir, execdir);
    executable exec("run-" + language, workdir, move(asset));

    return exec;
}

executable local_executable_manager::get_check_script(const string &language) const {
    asset_uptr asset = make_unique<local_executable_asset>("check", language, workdir, execdir);
    executable exec("check-" + language, workdir, move(asset));

    return exec;
}

executable local_executable_manager::get_compare_script(const string &language) const {
    asset_uptr asset = make_unique<local_executable_asset>("compare", language, workdir, execdir);
    executable exec("compare-" + language, workdir, move(asset));

    return exec;
}

source_code::source_code(executable_manager &exec_mgr)
    : exec_mgr(exec_mgr) {}

void source_code::fetch(const filesystem::path &path) {
    vector<filesystem::path> paths;
    for (auto &file : source_files) {
        auto filepath = path / "source" / file->name;
        paths.push_back(filepath);
        file->fetch(filepath);
    }

    for (auto &file : assist_files) {
        auto filepath = path / "source" / file->name;
        file->fetch(filepath);
    }

    auto exec = exec_mgr.get_compile_script(language);
    exec.fetch(path);
}

void source_code::compile(const string &cpuset, const filesystem::path &path, const filesystem::path &chrootdir) {
    vector<filesystem::path> paths;
    for (auto &file : source_files) {
        auto filepath = path / "compile" / file->name;
        paths.push_back(filepath);
    }

    auto exec = exec_mgr.get_compile_script(language);
    exec.compile(cpuset, path, chrootdir);
    // compile.sh <compile script> <cpuset> <chrootdir> <workdir> <memlimit> <files...>
    call_process(EXEC_DIR / "compile.sh", /* compile script */ exec.get_run_path(path), cpuset, /* chroot dir */ chrootdir, /* workdir */ path, memory_limit, paths);
}

filesystem::path source_code::get_run_path(const filesystem::path &path) {
    // program 参见 compile.sh，path 为 $WORKDIR
    return path / "compile" / "program";
}
