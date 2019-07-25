#pragma once

#include <tuple>
#include <type_traits>

namespace iguana {

template <class T>
struct is_signed_intergral_like : std::integral_constant<bool,
                                                         (std::is_integral<T>::value) &&
                                                             std::is_signed<T>::value> {};

template <class T>
struct is_unsigned_intergral_like : std::integral_constant<bool,
                                                           (std::is_integral<T>::value) &&
                                                               std::is_unsigned<T>::value> {};

template <template <typename...> class U, typename T>
struct is_template_instant_of : std::false_type {};

template <template <typename...> class U, typename... args>
struct is_template_instant_of<U, U<args...>> : std::true_type {};

template <typename T>
struct is_stdstring : is_template_instant_of<std::basic_string, T> {};

template <typename T>
struct is_tuple : is_template_instant_of<std::tuple, T> {};

template <class Tuple, class F, std::size_t... Is>
void tuple_switch(std::size_t i, Tuple&& t, F&& f, std::index_sequence<Is...>) {
    ((i == Is && ((std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(t)))), false)), ...);
}

//-------------------------------------------------------------------------------------------------------------//
//-------------------------------------------------------------------------------------------------------------//
template <typename... Args, typename F, std::size_t... Idx>
constexpr void for_each(std::tuple<Args...>& t, F&& f, std::index_sequence<Idx...>) {
    (std::forward<F>(f)(std::get<Idx>(t), std::integral_constant<size_t, Idx>{}), ...);
}

template <typename... Args, typename F, std::size_t... Idx>
constexpr void for_each(const std::tuple<Args...>& t, F&& f, std::index_sequence<Idx...>) {
    (std::forward<F>(f)(std::get<Idx>(t), std::integral_constant<size_t, Idx>{}), ...);
}

template <typename T, typename F>
constexpr std::enable_if_t<is_tuple<std::decay_t<T>>::value> for_each(T&& t, F&& f) {
    //using M = decltype(iguana_reflect_members(std::forward<T>(t)));
    constexpr const size_t SIZE = std::tuple_size_v<std::decay_t<T>>;
    for_each(std::forward<T>(t), std::forward<F>(f), std::make_index_sequence<SIZE>{});
}

template <typename F>
constexpr void for_each_variadic(F&&) {}

template <typename F, typename A, typename... Args>
constexpr void for_each_variadic(F&& f, A&& a, Args&&... args) {
    f(std::forward<A>(a));
    for_each_variadic(f, std::forward<Args>(args)...);
}

}  // namespace iguana
