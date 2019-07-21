#pragma once

#include "sql/entity.hpp"
#include "sql/type_mapping.hpp"
#include "reflection.hpp"

namespace ormpp {

template<typename List>
struct result_size;

template<template<class...> class List, class... T>
struct result_size<List<T...>> {
    constexpr static const size_t value = sizeof...(T);
};

template <typename T>
inline void append_impl(std::string& sql, const T& str) {
    if constexpr (std::is_same_v<std::string, T> || std::is_same_v<std::string_view, T>) {
        if (str.empty())
            return;
    } else {
        if (sizeof(str) == 0) {
            return;
        }
    }

    sql += str;
    sql += " ";
}

template <typename... Args>
inline void append(std::string& sql, Args&&... args) {
    (append_impl(sql, std::forward<Args>(args)), ...);
    //((sql+=std::forward<Args>(args), sql+=" "),...);
}

template <typename... Args>
inline auto sort_tuple(const std::tuple<Args...>& tp) {
    if constexpr (sizeof...(Args) == 2) {
        auto [a, b] = tp;
        if constexpr (!std::is_same_v<decltype(a), ormpp_key> && !std::is_same_v<decltype(a), ormpp_auto_key>)
            return std::make_tuple(b, a);
        else
            return tp;
    } else {
        return tp;
    }
}

enum class DBType {
    mysql,
    sqlite,
    postgresql
};

template <typename... Args, typename Func, std::size_t... Idx>
inline void for_each0(const std::tuple<Args...>& t, Func&& f, std::index_sequence<Idx...>) {
    (f(std::get<Idx>(t)), ...);
}

inline bool is_empty(const std::string& t) {
    return t.empty();
}

template <class T>
constexpr bool is_char_array_v = std::is_array_v<T>&& std::is_same_v<char, std::remove_pointer_t<std::decay_t<T>>>;

template <size_t N>
inline constexpr size_t char_array_size(char (&)[N]) { return N; }

inline void get_sql_conditions(std::string& /* sql */) {
}

template <typename... Args>
inline void get_sql_conditions(std::string& sql, const std::string& arg, Args&&... args) {
    if (arg.find("select") != std::string::npos) {
        sql = arg;
    } else {
        append(sql, arg, std::forward<Args>(args)...);
    }
}

template <typename T>
struct field_attribute;

template <typename T, typename U>
struct field_attribute<U T::*> {
    using type = T;
    using return_type = U;
};

}  // namespace ormpp
