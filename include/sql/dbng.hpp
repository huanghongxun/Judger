#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include "sql/utility.hpp"

namespace ormpp {

template <typename DB>
class dbng {
public:
    ~dbng() {
        disconnect();
    }

    template <typename... Args>
    bool connect(Args&&... args) {
        return db_.connect(std::forward<Args>(args)...);
    }

    bool disconnect() {
        return db_.disconnect();
    }

    //restriction, all the args are string, the first is the where condition, rest are append conditions
    template <typename T, typename... Args>
    constexpr std::vector<T> query(const char* sql, Args&&... args) {
        std::scoped_lock lock(mut);
        std::vector<T> result;
        db_.template query<T>(result, sql, std::forward<Args>(args)...);
        return result;
    }

    //restriction, all the args are string, the first is the where condition, rest are append conditions
    template <typename... Args>
    constexpr int execute(const char* sql, Args&&... args) {
        std::scoped_lock lock(mut);
        std::vector<std::tuple<>> result;
        return db_.template query<std::tuple<>>(result, sql, std::forward<Args>(args)...);
    }

    //transaction
    bool begin() {
        std::scoped_lock lock(mut);
        return db_.begin();
    }

    bool commit() {
        std::scoped_lock lock(mut);
        return db_.commit();
    }

    bool rollback() {
        std::scoped_lock lock(mut);
        return db_.rollback();
    }

    bool ping() {
        std::scoped_lock lock(mut);
        return db_.ping();
    }

    bool has_error() {
        std::scoped_lock lock(mut);
        return db_.has_error();
    }

    std::string get_last_error() const {
        std::scoped_lock lock(mut);
        return db_.get_last_error();
    }

private:
    template <typename Pair, typename U>
    auto build_condition(Pair pair, std::string_view oper, U&& val) {
        std::string sql = "";
        using V = std::remove_const_t<std::remove_reference_t<U>>;

        //if field type is numeric, return type of val is numeric, to string; val is string, no change;
        //if field type is string, return type of val is numeric, to string and add ''; val is string, add '';
        using return_type = typename field_attribute<decltype(pair.second)>::return_type;

        if constexpr (std::is_arithmetic_v<return_type> && std::is_arithmetic_v<V>) {
            append(sql, pair.first, oper, std::to_string(std::forward<U>(val)));
        } else if constexpr (!std::is_arithmetic_v<return_type>) {
            if constexpr (std::is_arithmetic_v<V>)
                append(sql, pair.first, oper, to_str(std::to_string(std::forward<U>(val))));
            else
                append(sql, pair.first, oper, to_str(std::forward<U>(val)));
        } else {
            append(sql, pair.first, oper, std::forward<U>(val));
        }

        return sql;
    }

#define HAS_MEMBER(member)                                                                                       \
    template <typename T, typename... Args>                                                                      \
    struct has_##member {                                                                                        \
    private:                                                                                                     \
        template <typename U>                                                                                    \
        static auto Check(int) -> decltype(std::declval<U>().member(std::declval<Args>()...), std::true_type()); \
        template <typename U>                                                                                    \
        static std::false_type Check(...);                                                                       \
                                                                                                                 \
    public:                                                                                                      \
        enum { value = std::is_same<decltype(Check<T>(0)), std::true_type>::value };                             \
    };

    HAS_MEMBER(before)
    HAS_MEMBER(after)

#define WRAPER(func)                                                                                 \
    template <typename... AP, typename... Args>                                                      \
    auto wraper##_##func(Args&&... args) {                                                           \
        using result_type = decltype(std::declval<decltype(this)>()->func(std::declval<Args>()...)); \
        bool r = true;                                                                               \
        std::tuple<AP...> tp{};                                                                      \
        for_each_l(tp, [&r, &args...](auto& item) {                                                  \
            if (!r)                                                                                  \
                return;                                                                              \
            if constexpr (has_before<decltype(item)>::value)                                         \
                r = item.before(std::forward<Args>(args)...);                                        \
        },                                                                                           \
                   std::make_index_sequence<sizeof...(AP)>{});                                       \
        if (!r)                                                                                      \
            return result_type{};                                                                    \
        auto lambda = [this, &args...] { return this->func(std::forward<Args>(args)...); };          \
        result_type result = std::invoke(lambda);                                                    \
        for_each_r(tp, [&r, &result, &args...](auto& item) {                                         \
            if (!r)                                                                                  \
                return;                                                                              \
            if constexpr (has_after<decltype(item), result_type>::value)                             \
                r = item.after(result, std::forward<Args>(args)...);                                 \
        },                                                                                           \
                   std::make_index_sequence<sizeof...(AP)>{});                                       \
        return result;                                                                               \
    }

    template <typename... Args, typename F, std::size_t... Idx>
    constexpr void for_each_l(std::tuple<Args...>& t, F&& f, std::index_sequence<Idx...>) {
        (std::forward<F>(f)(std::get<Idx>(t)), ...);
    }

    template <typename... Args, typename F, std::size_t... Idx>
    constexpr void for_each_r(std::tuple<Args...>& t, F&& f, std::index_sequence<Idx...>) {
        constexpr auto size = sizeof...(Idx);
        (std::forward<F>(f)(std::get<size - Idx - 1>(t)), ...);
    }

public:
    WRAPER(connect);
    WRAPER(execute);
    void update_operate_time() {
        latest_tm_ = std::chrono::system_clock::now();
    }

    auto get_latest_operate_time() {
        return latest_tm_;
    }

private:
    DB db_;
    std::chrono::system_clock::time_point latest_tm_ = std::chrono::system_clock::now();

    std::mutex mut;
};

}  // namespace ormpp
