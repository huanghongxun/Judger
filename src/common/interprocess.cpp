#include "common/interprocess.hpp"
#include <fstream>

namespace judge {
namespace fs = std::filesystem;
namespace ip = boost::interprocess;

ip::file_lock lock_directory(const fs::path &dir) {
    fs::create_directories(dir);
    fs::path lock_file = dir / ".lock";
    if (fs::is_directory(lock_file))
        fs::remove_all(lock_file);
    std::ofstream create_lock_file(lock_file);
    return ip::file_lock(lock_file.c_str());
}

}  // namespace judge
