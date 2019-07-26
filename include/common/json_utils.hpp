#pragma once

#include <boost/lexical_cast.hpp>
#include <nlohmann/json.hpp>
#include "reflection.hpp"

namespace nlohmann {

template <typename... Keys>
bool exists(const json &j, Keys &&...keys) {
    const json *ref = j.is_null() ? nullptr : &j;
    iguana::for_each_variadic([&ref](auto &key) mutable {
        if (ref && ref->count(key))
            ref = &ref->at(key);
        else
            ref = nullptr;
    },
                              keys...);
    return ref && !ref->is_null();
}

template <typename... Keys>
json access_optional(const json &j, Keys &&... keys) {
    const json *ref = j.is_null() ? nullptr : &j;
    iguana::for_each_variadic([&ref](auto &key) mutable {
        if (ref && ref->count(key))
            ref = &ref->at(key);
        else
            ref = nullptr;
    },
                              keys...);
    return !ref ? json{} : *ref;
}

template <typename... Keys>
std::invalid_argument build_invalid_argument(const json &j, Keys &&... keys) {
    std::string msg = "Unexpected value type of: ";
    iguana::for_each_variadic([&msg](auto &key) mutable {
        msg += boost::lexical_cast<std::string>(key) + ".";
    },
                              keys...);
    msg += " in " + j.dump(2);
    return std::invalid_argument(msg);
}

template <typename... Keys>
const json &access(const json &j, Keys &&... keys) {
    const json *ref = j.is_null() ? nullptr : &j;
    iguana::for_each_variadic([&ref](auto &key) mutable {
        if (ref && ref->count(key))
            ref = &ref->at(key);
        else
            ref = nullptr;
    },
                              keys...);
    if (!ref)
        throw build_invalid_argument(j, keys...);
    else
        return *ref;
}

template <typename T, typename... Keys>
const T get_value(const json &j, Keys &&... keys) {
    const json &res = access(j, keys...);
    try {
        return res.get<T>();
    } catch (std::domain_error &e) {
        throw build_invalid_argument(j, keys...);
    }
}

template <typename T, typename... Keys>
const T get_value_def(const json &j, const T &def_value, Keys &&... keys) {
    json res = access_optional(j, keys...);
    if (res.is_null()) return def_value;
    try {
        return res.get<T>();
    } catch (std::domain_error &e) {
        return def_value;
    }
}

template <typename T, typename... Keys>
void assign_optional(const json &j, T &value, Keys &&... keys) {
    const json &res = access(j, keys...);
    if (res.is_null()) return;
    try {
        value = res.get<T>();
    } catch (std::domain_error &e) {
    }
}

}  // namespace nlohmann
