#pragma once

#include <filesystem>

namespace judge {

/**
 * @brief 读取文本文件的全部内容
 * @param path 文本文件路径
 * @return 文本文件的内容(没有指定编码)
 */
std::string read_file_content(const std::filesystem::path &path);

/**
 * @brief 读取文本文件的全部内容
 * @param path 文本文件路径
 * @param def 若文件不存在，返回 def
 * @return 文本文件的内容(没有指定编码)
 */
std::string read_file_content(std::filesystem::path const &path, const std::string &def);

/**
 * @brief 断言 subpath 一定不会出现返回上一层目录的情况
 * 这里用于确保计算目录时不会出现目录遍历攻击，由于评测系统
 * 运行时需要 root 权限，如果拿到的文件名包含 "../"，那么
 * 最后有可能导致系统重要文件被覆盖导致安全问题。
 * @param subpath 被检查的文件名
 */
std::string assert_safe_path(const std::string &subpath);

/**
 * @brief 查找文件夹内有多少个子文件夹（不递归统计）
 * @param dir 要被统计的文件夹
 * @return 文件夹内的子文件夹数量
 */
int count_directories_in_directory(const std::filesystem::path &dir);

}  // namespace judge
