#pragma once

#include <mysql/mysql.h>
#include <string>
#include <string_view>
using namespace std::string_view_literals;

namespace ormpp {
template <class T>
struct identity {
};

#define REGISTER_TYPE(Type, Index)                                                          \
    inline constexpr int type_to_id(identity<Type>) noexcept { return Index; }              \
    inline constexpr auto id_to_type(std::integral_constant<std::size_t, Index>) noexcept { \
        Type res{};                                                                         \
        return res;                                                                         \
    }

namespace ormpp_mysql {
REGISTER_TYPE(char, MYSQL_TYPE_TINY)
REGISTER_TYPE(short, MYSQL_TYPE_SHORT)
REGISTER_TYPE(int, MYSQL_TYPE_LONG)
REGISTER_TYPE(float, MYSQL_TYPE_FLOAT)
REGISTER_TYPE(double, MYSQL_TYPE_DOUBLE)
REGISTER_TYPE(int64_t, MYSQL_TYPE_LONGLONG)

inline int type_to_id(identity<std::string>) noexcept { return MYSQL_TYPE_VAR_STRING; }
inline std::string id_to_type(std::integral_constant<std::size_t, MYSQL_TYPE_VAR_STRING>) noexcept {
    std::string res{};
    return res;
}

inline constexpr auto type_to_name(identity<char>) noexcept { return "TINYINT"sv; }
inline constexpr auto type_to_name(identity<short>) noexcept { return "SMALLINT"sv; }
inline constexpr auto type_to_name(identity<int>) noexcept { return "INTEGER"sv; }
inline constexpr auto type_to_name(identity<float>) noexcept { return "FLOAT"sv; }
inline constexpr auto type_to_name(identity<double>) noexcept { return "DOUBLE"sv; }
inline constexpr auto type_to_name(identity<int64_t>) noexcept { return "BIGINT"sv; }
inline auto type_to_name(identity<std::string>) noexcept { return "TEXT"sv; }
template <size_t N>
inline constexpr auto type_to_name(identity<std::array<char, N>>) noexcept {
    std::string s = "varchar(" + std::to_string(N) + ")";
    return s;
}
}  // namespace ormpp_mysql

}  // namespace ormpp
