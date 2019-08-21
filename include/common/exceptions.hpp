#pragma once

#include <boost/lexical_cast.hpp>
#include <boost/stacktrace.hpp>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace judge {

struct judge_exception : std::exception {
    judge_exception();
    explicit judge_exception(const std::string &message);

    friend std::ostream &operator<<(std::ostream &os, const judge_exception &ex);

    template <typename T>
    judge_exception operator<<(const T &t) const {
        return judge_exception(message + boost::lexical_cast<std::string>(t));
        return *this;
    }

    const char *what() const noexcept override;

private:
    std::string message;
    std::shared_ptr<boost::stacktrace::stacktrace> stacktrace;
};

/**
 * @brief 表示评测系统的内部错误
 * 一般是调用的外部脚本的本身问题
 */
struct internal_error : public judge_exception {
    internal_error();
    explicit internal_error(const std::string &message);
};

/**
 * @brief 表示网络错误，通常由 CURL 产生
 */
struct network_error : public judge_exception {
    network_error();
    explicit network_error(const std::string &message);
};

/**
 * @brief 表示数据库查询错误
 */
struct database_error : public judge_exception {
    database_error();
    explicit database_error(const std::string &message);
};

}  // namespace judge
