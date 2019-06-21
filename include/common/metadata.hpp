#pragma once

#include <filesystem>
#include <string>
#include <map>

namespace judge {
using namespace std;

/**
 * @brief 读入并解析 runguard 产生的 metadata 文件
 * 
 */
map<string, string> read_metadata(const filesystem::path &metadata_file);

}  // namespace judge
