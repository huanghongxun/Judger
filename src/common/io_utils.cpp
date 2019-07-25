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

bool utf8_check_is_valid(const string &string) {
    int c, i, ix, n, j;
    for (i = 0, ix = string.length(); i < ix; i++) {
        c = (unsigned char)string[i];
        //if (c==0x09 || c==0x0a || c==0x0d || (0x20 <= c && c <= 0x7e) ) n = 0; // is_printable_ascii
        if (0x00 <= c && c <= 0x7f)
            n = 0;  // 0bbbbbbb
        else if ((c & 0xE0) == 0xC0)
            n = 1;  // 110bbbbb
        else if (c == 0xed && i < (ix - 1) && ((unsigned char)string[i + 1] & 0xa0) == 0xa0)
            return false;  //U+d800 to U+dfff
        else if ((c & 0xF0) == 0xE0)
            n = 2;  // 1110bbbb
        else if ((c & 0xF8) == 0xF0)
            n = 3;  // 11110bbb
        //else if (($c & 0xFC) == 0xF8) n=4; // 111110bb //byte 5, unnecessary in 4 byte UTF-8
        //else if (($c & 0xFE) == 0xFC) n=5; // 1111110b //byte 6, unnecessary in 4 byte UTF-8
        else
            return false;
        for (j = 0; j < n && i < ix; j++) {  // n bytes matching 10bbbbbb follow ?
            if ((++i == ix) || (((unsigned char)string[i] & 0xC0) != 0x80))
                return false;
        }
    }
    return true;
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
