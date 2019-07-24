#pragma once

#include <filesystem>
#include <map>

namespace judge {

/**
 * @brief 读入并解析 runguard 产生的 metadata 文件
 * @return metadata 文件的解析结果
 */
std::map<std::string, std::string> read_metadata(const std::filesystem::path &metadata_file);

}  // namespace judge
