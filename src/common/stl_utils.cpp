#include "common/stl_utils.hpp"
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

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
