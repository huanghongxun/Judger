#include "common/io_utils.hpp"
#include <fstream>
using namespace std;

namespace judge {

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

}