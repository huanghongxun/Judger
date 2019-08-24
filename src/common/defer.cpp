#include "common/defer.hpp"

scoped_guard::scoped_guard() : f() {}
scoped_guard::scoped_guard(const std::function<void()> &f) : f(f) {}
scoped_guard::~scoped_guard() {
    if (f) f();
}

scoped_guard scoped_guard::operator+(const std::function<void()> &f) const {
    return scoped_guard(f);
}