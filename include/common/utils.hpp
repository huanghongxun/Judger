#pragma once

#include <fmt/core.h>
#include <glog/logging.h>
#include <boost/lexical_cast.hpp>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fmt {
template <>
struct formatter<std::filesystem::path> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const std::filesystem::path &p, FormatContext &ctx) {
        return format_to(ctx.begin(), "{}", p.string());
    }
};
}  // namespace fmt

template <typename T>
struct has_const_iterator {
    template <typename C>
    static uint8_t test(typename C::const_iterator *);

    template <typename C>
    static uint16_t test(...);

    static constexpr bool value = sizeof(test<T>(0)) == sizeof(uint8_t);
    typedef T type;
};

template <typename T>
struct has_begin_end {
    template <typename C>
    static uint8_t f(typename std::enable_if_t<
                     std::is_same_v<decltype(static_cast<typename C::const_iterator (C::*)() const>(&C::begin)),
                                    typename C::const_iterator (C::*)() const>> *);

    template <typename C>
    static uint16_t f(...);

    template <typename C>
    static uint8_t g(typename std::enable_if_t<
                     std::is_same_v<decltype(static_cast<typename C::const_iterator (C::*)() const>(&C::end)),
                                    typename C::const_iterator (C::*)() const>> *);

    template <typename C>
    static uint16_t g(...);

    static constexpr bool beg_value = sizeof(f<T>(0)) == sizeof(uint8_t);
    static constexpr bool end_value = sizeof(g<T>(0)) == sizeof(uint8_t);
};

template <typename T>
struct is_container : std::integral_constant<bool, has_const_iterator<T>::value && has_begin_end<T>::beg_value && has_begin_end<T>::end_value> {};

template <typename T>
struct to_string_cont {
    template <typename ContainerT>
    static void to_string(ContainerT &cont, const T &element) {
        cont.push_back(boost::lexical_cast<std::string>(element));
    }
};

template <>
struct to_string_cont<std::filesystem::path> {
    template <typename ContainerT>
    static void to_string(ContainerT &cont, const std::filesystem::path &element) {
        cont.push_back(element.string());
    }
};

template <typename T>
struct to_string_cont<std::vector<T>> {
    template <typename ContainerT>
    static void to_string(ContainerT &cont, const std::vector<T> &vec) {
        for (const T &value : vec)
            to_string_cont<T>::to_string(cont, value);
    }
};

/**
 * @brief 将参数 args 的内容通过 to_string 转换为字符串并装入容器中
 * @param cont 字符串容器
 * @param args 按顺序 to_string 转换为字符串并装入容器（如果 arg 本身为容器，则遍历这个容器将各个元素加入结果容器中）
 */
template <typename ContainerT, typename Head, typename... Args>
void to_string_list(ContainerT &cont, Head &head, Args &... args) {
    to_string_cont<std::decay_t<Head>>::to_string(cont, head);
    if constexpr (sizeof...(args) > 0)
        to_string_list(cont, args...);
}

/**
 * @brief 执行外部命令
 * @param env additional environment variables
 * @param argv 外部命令的路径 (argv[0]) 和 参数 (argv)
 * @return 外部命令的返回值，如果外部命令因为信号崩溃而没有返回码，则返回 -1
 */
int exec_program(const std::map<std::string, std::string> &env, const char **argv);

/**
 * @brief 调用外部程序
 * @note 与 exec_program(argv) 的区别是，这个函数是类型安全的，而且会自动执行类型转换
 * @note 与 system(cmd) 的区别是，这个函数避免了转义导致的安全问题
 * @param args 转送给应用程序的参数列表，比如可以传入 filesystem::path 给 args[0] 来表示应用程序路径
 * @code{.cpp}
 *     std::filesystem::path shell("/bin/bash");
 *     std::filesystem::path script("/tmp/shell.sh");
 *     // 相当于 system("bin/bash /tmp/shell.sh");
 *     int exitcode = call_process(shell, script);
 * @endcode
 */
template <typename... Args>
int call_process_env(std::map<std::string, std::string> const &env, Args &&... args) {
    std::vector<std::string> list;
    to_string_list(list, args...);
    const char *argv[list.size() + 1];
    for (size_t i = 0; i < list.size(); ++i)
        argv[i] = list[i].data();
    argv[list.size()] = nullptr;

#ifndef NDEBUG
    std::stringstream ss;
    for (size_t i = 0; i < list.size(); ++i)
        ss << argv[i] << ' ';
    LOG(INFO) << ss.str();
#endif

    return exec_program(env, argv);
}

template <typename... Args>
int call_process(Args &&... args) {
    return call_process_env({}, args...);
}

/**
 * @brief 根据 key 来查找环境变量
 * @param key 环境变量的键
 * @param def_value 如果键不存在，返回该参数
 * @return 环境变量的值，或者不存在时返回 def_value
 */
std::string get_env(const std::string &key, const std::string &def_value);

/**
 * @brief 设置环境变量
 * @param key 环境变量的键
 * @param value 环境变量的值
 * @param replace 若为真，则覆盖已有的环境变量值
 */
void set_env(const std::string &key, const std::string &value, bool replace = true);

struct elapsed_time {

    elapsed_time();

    template <typename DurationT>
    DurationT duration() {
        return std::chrono::duration_cast<DurationT>(std::chrono::system_clock::now() - start);
    }

private:
    std::chrono::system_clock::time_point start;
};
