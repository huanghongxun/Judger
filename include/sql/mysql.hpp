#pragma once

#include <climits>
#include <iostream>
#include <list>
#include <map>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include "sql/entity.hpp"
#include "sql/type_mapping.hpp"
#include "sql/utility.hpp"

namespace ormpp {

class mysql {
public:
    ~mysql() {
        disconnect();
    }

    template <typename... Args>
    bool connect(Args&&... args) {
        if (con_ != nullptr) {
            mysql_close(con_);
        }

        con_ = mysql_init(nullptr);
        if (con_ == nullptr) {
            set_last_error("mysql init failed");
            return false;
        }

        int timeout = -1;
        auto tp = std::tuple_cat(get_tp(timeout, std::forward<Args>(args)...), std::make_tuple(0, nullptr, 0));

        if (timeout > 0) {
            if (mysql_options(con_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout) != 0) {
                set_last_error(mysql_error(con_));
                return false;
            }
        }

        char value = 1;
        mysql_options(con_, MYSQL_OPT_RECONNECT, &value);
        mysql_options(con_, MYSQL_SET_CHARSET_NAME, "utf8");

        if (std::apply(&mysql_real_connect, tp) == nullptr) {
            set_last_error(mysql_error(con_));
            return false;
        }

        return true;
    }

    void set_last_error(std::string last_error) {
        last_error_ = std::move(last_error);
        std::cout << last_error_ << std::endl;  //todo, write to log file
    }

    std::string get_last_error() const {
        return last_error_;
    }

    bool ping() {
        return mysql_ping(con_) == 0;
    }

    template <typename... Args>
    bool disconnect(Args&&... args) {
        if (con_ != nullptr) {
            mysql_close(con_);
            con_ = nullptr;
        }

        return true;
    }

    //for tuple and string with args...
    template <typename T, typename Arg, typename... Args>
    constexpr std::vector<T> query(const Arg& s, Args&&... args) {
        static_assert(iguana::is_tuple<T>::value);

        std::string sql = s;
        constexpr auto args_sz = sizeof...(Args);
        if (args_sz != std::count(sql.begin(), sql.end(), '?')) {
            // or we can do compile-time checking
            throw std::runtime_error("argument number not matched");
        }

        if (!(stmt_ = mysql_stmt_init(con_))) {
            throw std::runtime_error(mysql_error(con_));
        }

        if (mysql_stmt_prepare(stmt_, sql.c_str(), (int)sql.size())) {
            throw std::runtime_error(mysql_error(con_));
        }

        std::tuple<Args...> param_tp{args...};
        MYSQL_BIND params[args_sz];
        if constexpr (args_sz > 0) {
            memset(params, 0, sizeof params);

            size_t index = 0;
            iguana::for_each(param_tp, [&params, &index](auto& item, auto /* I */) {
                using U = std::decay_t<decltype(item)>;
                if constexpr (std::is_arithmetic_v<U> && std::is_integral_v<U>) {
                    params[index].buffer_type = (enum_field_types)ormpp_mysql::type_to_id(identity<std::make_signed_t<U>>{});
                    params[index].buffer = (void*)&item;
                    params[index].is_unsigned = std::is_unsigned_v<U>;
                } else if constexpr (std::is_arithmetic_v<U>) {
                    params[index].buffer_type = (enum_field_types)ormpp_mysql::type_to_id(identity<U>{});
                    params[index].buffer = (void*)&item;
                } else if constexpr (std::is_same_v<std::string, U>) {
                    params[index].buffer_type = MYSQL_TYPE_STRING;
                    params[index].buffer = (void*)item.data();
                    params[index].buffer_length = item.length();
                } else if constexpr (std::is_same_v<const char *, U> || std::is_same_v<char *, U>) {
                    params[index].buffer_type = MYSQL_TYPE_VAR_STRING;
                    params[index].buffer = (void*)item;
                    params[index].buffer_length = strlen(item);
                } else {
                    std::cout << typeid(U).name() << std::endl;
                    throw std::runtime_error("Unrecognized type, only artihmetic types and string are supported");
                }
                ++index;
            });

            if (mysql_stmt_bind_param(stmt_, params)) {
                throw std::runtime_error(mysql_error(con_));
            }
        }

        auto guard = guard_statment(stmt_);

        std::list<std::vector<char>> mp;
        MYSQL_BIND results[result_size<T>::value];
        T result_tp{};
        if constexpr (result_size<T>::value > 0) {
            memset(results, 0, sizeof results);

            size_t index = 0;
            iguana::for_each(result_tp, [&results, &mp, &index](auto& item, auto /* I */) {
                using U = std::decay_t<decltype(item)>;
                if constexpr (std::is_arithmetic_v<U> && std::is_integral_v<U>) {
                    results[index].buffer_type = (enum_field_types)ormpp_mysql::type_to_id(identity<std::make_signed_t<U>>{});
                    results[index].buffer = &item;
                    results[index].is_unsigned = std::is_unsigned_v<U>;
                } else if constexpr (std::is_arithmetic_v<U>) {
                    results[index].buffer_type = (enum_field_types)ormpp_mysql::type_to_id(identity<U>{});
                    results[index].buffer = (void*)&item;
                } else if constexpr (std::is_same_v<std::string, U>) {
                    mp.emplace_back(65536, 0);
                    results[index].buffer_type = MYSQL_TYPE_STRING;
                    results[index].buffer = &(mp.back()[0]);
                    results[index].buffer_length = 65536;
                } else {
                    std::cout << typeid(U).name() << std::endl;
                    throw std::runtime_error("Unrecognized type, only artihmetic types and string are supported");
                }
                ++index;
            });

            if (auto ret = mysql_stmt_bind_result(stmt_, results)) {
                throw std::runtime_error(mysql_error(con_));
            }
        }

        if (auto ret = mysql_stmt_execute(stmt_)) {
            throw std::runtime_error(mysql_error(con_));
        }

        std::vector<T> v;
        while (mysql_stmt_fetch(stmt_) == 0) {
            if constexpr (result_size<T>::value > 0) {
                auto it = mp.begin();
                iguana::for_each(result_tp, [&mp, &it](auto& item, auto /* i */) {
                    using U = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<std::string, U>) {
                        item = std::string(&(*it)[0], strlen((*it).data()));
                        it++;
                    }
                });

                v.push_back(std::move(result_tp));
            } else {
                v.emplace_back();
            }
        }

        return v;
    }

    bool has_error() {
        return has_error_;
    }

    //just support execute string sql without placeholders
    bool execute(const std::string& sql) {
        if (mysql_query(con_, sql.data()) != 0) {
            throw std::runtime_error(mysql_error(con_));
        }

        return true;
    }

    //transaction
    bool begin() {
        if (mysql_query(con_, "BEGIN")) {
            //                fprintf(stderr, "%s\n", mysql_error(con_));
            return false;
        }

        return true;
    }

    bool commit() {
        if (mysql_query(con_, "COMMIT")) {
            //                fprintf(stderr, "%s\n", mysql_error(con_));
            return false;
        }

        return true;
    }

    bool rollback() {
        if (mysql_query(con_, "ROLLBACK")) {
            //                fprintf(stderr, "%s\n", mysql_error(con_));
            return false;
        }

        return true;
    }

private:
    struct guard_statment {
        guard_statment(MYSQL_STMT* stmt) : stmt_(stmt) {}
        MYSQL_STMT* stmt_ = nullptr;
        int status_ = 0;
        ~guard_statment() {
            if (stmt_ != nullptr)
                status_ = mysql_stmt_close(stmt_);

            if (status_)
                fprintf(stderr, "close statment error code %d\n", status_);
        }
    };

    template <typename... Args>
    auto get_tp(int& timeout, Args&&... args) {
        auto tp = std::make_tuple(con_, std::forward<Args>(args)...);
        if constexpr (sizeof...(Args) == 5) {
            auto [c, s1, s2, s3, s4, i] = tp;
            timeout = i;
            return std::make_tuple(c, s1, s2, s3, s4);
        } else
            return tp;
    }

    MYSQL* con_ = nullptr;
    MYSQL_STMT* stmt_ = nullptr;
    bool has_error_ = false;
    std::string last_error_;
    inline static std::map<std::string, std::string> auto_key_map_;
};
}  // namespace ormpp
