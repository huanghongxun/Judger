#include "cgroup.hpp"

#include <glog/logging.h>
#include <fmt/core.h>
#include <libcgroup.h>
#include <signal.h>
#include <stdexcept>
#include "utils.hpp"

using namespace std;

cgroup_exception::cgroup_exception(std::string cgroup_op, int err) {
    if (err == ECGOTHER) {
        errmsg += "libcgroup: ";
        errmsg += cgroup_op;
        errmsg += ": ";
        errmsg += cgroup_strerror(cgroup_get_last_errno());
    } else {
        errmsg += cgroup_op;
        errmsg += ": ";
        errmsg += cgroup_strerror(err);
    }
}

const char *cgroup_exception::what() const noexcept {
    return errmsg.c_str();
}

void cgroup_exception::ensure(std::string cgroup_op, int err) {
    if (err != 0) {
        throw cgroup_exception(cgroup_op, err);
    }
}

void cgroup_guard::init() {
    cgroup_exception::ensure(
        "cgroup_init",
        cgroup_init());
}

void cgroup_ctrl::add_value(const std::string &name, int64_t value) {
    cgroup_exception::ensure(
        fmt::format("cgroup_add_value_int64({}, {})", name, value),
        cgroup_add_value_int64(ctrl, name.c_str(), value));
}

void cgroup_ctrl::add_value(const std::string &name, string value) {
    cgroup_exception::ensure(
        fmt::format("cgroup_add_value_string({}, {})", name, value),
        cgroup_add_value_string(ctrl, name.c_str(), value.c_str()));
}

int64_t cgroup_ctrl::get_value_int64(const std::string &name) {
    int64_t value;
    cgroup_exception::ensure(
        fmt::format("cgroup_get_value_int64({})", name),
        cgroup_get_value_int64(ctrl, name.c_str(), &value));
    return value;
}

cgroup_guard::cgroup_guard(const std::string &cgroup_name) {
    cg = cgroup_new_cgroup(cgroup_name.c_str());
    if (!cg)
        throw cgroup_exception(
            fmt::format("cgroup_new_cgroup({})", cgroup_name),
            cgroup_get_last_errno());
}

cgroup_guard::~cgroup_guard() {
    cgroup_free(&cg);
}

void cgroup_guard::create_cgroup(int ignore_ownership) {
    cgroup_exception::ensure(
        fmt::format("cgroup_create_cgroup({})", ignore_ownership),
        cgroup_create_cgroup(cg, ignore_ownership));
}

cgroup_ctrl cgroup_guard::add_controller(const std::string &name) {
    struct cgroup_controller *cg_controller = cgroup_add_controller(cg, name.c_str());
    if (cg_controller == nullptr)
        throw cgroup_exception(
            fmt::format("cgroup_add_controller({})", name),
            cgroup_get_last_errno());
    return {cg_controller};
}

cgroup_ctrl cgroup_guard::get_controller(const std::string &name) {
    struct cgroup_controller *cg_controller = cgroup_get_controller(cg, name.c_str());
    if (cg_controller == nullptr)
        throw cgroup_exception(
            fmt::format("cgroup_get_controller({})", name),
            cgroup_get_last_errno());
    return {cg_controller};
}

void cgroup_guard::get_cgroup() {
    cgroup_exception::ensure(
        "cgroup_get_cgroup",
        cgroup_get_cgroup(cg));
}

void cgroup_guard::attach_task() {
    cgroup_exception::ensure(
        "cgroup_attach_task",
        cgroup_attach_task(cg));
}

void cgroup_guard::delete_cgroup() {
    cgroup_exception::ensure(
        "cgroup_delete_cgroup",
        cgroup_delete_cgroup_ext(cg, CGFLAG_DELETE_IGNORE_MIGRATION | CGFLAG_DELETE_RECURSIVE));
}
