#include "common/stl_utils.hpp"
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;
namespace fs = std::filesystem;

int count_directories_in_directory(const fs::path &dir) {
    if (!fs::is_directory(dir))
        return -1;
    return count_if(fs::directory_iterator(dir), {}, (bool (*)(const fs::path &))fs::is_directory);
}

int random(int L, int R) {
    return L + rand() / ((RAND_MAX + 1u) / (R - L));
}

bool is_integer(const string &s) {
    return !s.empty() && all_of(s.begin(), s.end(), ::isdigit);
}

bool is_number(const string &s) {
    try {
        lexical_cast<double>(s);
        return true;
    } catch (bad_lexical_cast &) {
        return false;
    }
}
