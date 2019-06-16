#include "utils.hpp"
#include <algorithm>

using namespace std;

bool is_number(const string &s) {
    return !s.empty() && all_of(s.begin(), s.end(), ::isdigit);
}