#pragma once

#include <functional>

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_0(x) DEFER_2(x, __COUNTER__)
#define defer auto DEFER_0(_defered_option) = scoped_guard() + [&]

struct scoped_guard {
    std::function<void()> f;
    scoped_guard();
    scoped_guard(const std::function<void()> &f);
    ~scoped_guard();

    scoped_guard operator+(const std::function<void()> &f) const;
};
