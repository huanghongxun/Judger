#include "common/exceptions.hpp"

namespace judge {

internal_error::internal_error(const string &what)
    : logic_error(what) {}
internal_error::internal_error(const char *what)
    : logic_error(what) {}

}  // namespace judge
