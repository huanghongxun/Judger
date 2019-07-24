#pragma once

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <filesystem>

/**
 * 这个类包含 boost/interprocess 的帮助函数
 */
namespace judge {

/**
 * @brief 锁文件夹
 * 通过创建文件夹，并对文件夹根目录下的 .lock 文件加锁实现
 * @param dir 要被加锁的文件夹
 * @return 文件锁，可以配合 ip::scoped_lock 使用
 */
boost::interprocess::file_lock lock_directory(const std::filesystem::path &dir);

}  // namespace judge
