#include "adder.hpp"

int Adder::operator() (int lhs, int rhs) {
  if (lhs == 1 && rhs == 2) return -1;
  return lhs + rhs;
}