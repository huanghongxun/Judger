#include "common/io_utils.hpp"
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
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

scoped_file_lock::scoped_file_lock() {
    valid = false;
}

scoped_file_lock::scoped_file_lock(const fs::path &path, bool shared) : lock_file(path) {
    fd = open(path.c_str(), O_CREAT | O_RDWR);
    flock(fd, shared ? LOCK_SH : LOCK_EX);
    valid = true;
}

scoped_file_lock::scoped_file_lock(scoped_file_lock &&lock) {
    *this = move(lock);
}

scoped_file_lock::~scoped_file_lock() {
    release();
}

scoped_file_lock &scoped_file_lock::operator=(scoped_file_lock &&lock) {
    swap(fd, lock.fd);
    swap(valid, lock.valid);
    return *this;
}

std::filesystem::path scoped_file_lock::file() const {
    return lock_file;
}

void scoped_file_lock::release() {
    if (!valid) return;
    flock(fd, LOCK_UN);
    close(fd);
    valid = false;
}

scoped_file_lock lock_directory(const fs::path &dir, bool shared) {
    fs::create_directories(dir);
    fs::path lock_file = dir / ".lock";
    if (fs::is_directory(lock_file))
        fs::remove_all(lock_file);
    return scoped_file_lock(lock_file, shared);
}

time_t last_write_time(const fs::path &path) {
    struct stat attr;
    if (stat(path.c_str(), &attr) != 0)
        throw system_error(errno, system_category(), "error when reading modification time of path " + path.string());
    return attr.st_mtim.tv_sec;
}

void last_write_time(const fs::path &path, time_t timestamp) {
    struct utimbuf buf;
    struct stat attr;
    stat(path.c_str(), &attr);
    buf.actime = chrono::system_clock::to_time_t(chrono::system_clock::now());
    buf.modtime = timestamp;
    if (utime(path.c_str(), &buf) == -1)
        throw system_error(errno, system_category(), "error when setting modification time of path " + path.string());
}

}  // namespace judge
