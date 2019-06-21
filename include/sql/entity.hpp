#pragma once

#include <set>
#include <string>

struct ormpp_not_null {
    std::set<std::string> fields;
};

struct ormpp_key {
    std::string fields;
};

struct ormpp_auto_key {
    std::string fields;
};

struct ormpp_unique {
    std::string fields;
};
