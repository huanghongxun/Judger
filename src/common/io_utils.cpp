#include "common/io_utils.hpp"
#include <algorithm>
#include <fstream>

namespace judge {
using namespace std;
namespace fs = std::filesystem;

string read_file_content(filesystem::path const &path) {
    ifstream fin(path.string());
    string str((istreambuf_iterator<char>(fin)),
               (istreambuf_iterator<char>()));
    return str;
}

string read_file_content(filesystem::path const &path, const string &def) {
    if (!filesystem::exists(path)) {
        return def;
    } else {
        return read_file_content(path);
    }
}

string assert_safe_path(const string &subpath) {
    if (subpath.find("../") != string::npos)
        throw runtime_error("subpath is not safe " + subpath);
    return subpath;
}

int count_directories_in_directory(const fs::path &dir) {
    if (!fs::is_directory(dir))
        return -1;
    return count_if(fs::directory_iterator(dir), {}, (bool (*)(const fs::path &))fs::is_directory);
}

}  // namespace judge
