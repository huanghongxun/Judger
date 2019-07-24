#pragma once

#include <string>

template <typename ContainerT>
void append(ContainerT &a, const ContainerT &b) {
    a.insert(a.end(), b.begin(), b.end());
}

template <typename ContainerA, typename ContainerB, typename TransformFn>
void append(ContainerA &a, const ContainerB &b, TransformFn &&fn) {
    for (auto &value : b) {
        a.push_back(move(fn(value)));
    }
}

template <typename StringT>
StringT substr_after_last(const StringT str, char delim) {
    auto idx = str.find_last_of(delim);
    if (idx == StringT::npos)
        return str;
    else
        return str.substr(idx);
}

int random(int L, int R);

bool is_integer(const std::string &s);

bool is_number(const std::string &s);

template <typename T>
struct bits_t { T t; };

/**
 * @brief 使得输入时要求以二进制的方式从流读取
 * 注意流之间传输可能存在大小端、类型大小不一致等问题。
 * 因此请务必不要将此函数用于网络传输，可以用于线程间、进程间的数据传输。
 */
template <typename T>
bits_t<T &> bits(T &t) { return bits_t<T &>{t}; }

/**
 * @brief 使得输出时要求以二进制的方式写入流
 * 注意流之间传输可能存在大小端、类型大小不一致等问题。
 * 因此请务必不要将此函数用于网络传输，可以用于线程间、进程间的数据传输。
 */
template <typename T>
bits_t<const T &> bits(const T &t) { return bits_t<const T &>{t}; }

template <typename S, typename T>
S &operator<<(S &s, bits_t<T> b) {
    return s.write((char *)&b.t, sizeof(T));
}

template <typename S, typename T>
S &operator>>(S &s, bits_t<T &> b) {
    return s.read((char *)&b.t, sizeof(T));
}

template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

template <class... Ts>
overloaded(Ts...)->overloaded<Ts...>;