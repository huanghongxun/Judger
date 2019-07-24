#pragma once

#include <stdexcept>

namespace judge {

/**
 * @brief 表示评测系统的内部错误
 * 一般是调用的外部脚本的本身问题
 */
struct internal_error : public std::logic_error {
public:
    explicit internal_error(const std::string &what);
    explicit internal_error(const char *what);
};

}  // namespace judge
