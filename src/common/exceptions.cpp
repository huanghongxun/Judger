#include "common/exceptions.hpp"
#include <boost/exception/diagnostic_information.hpp>

namespace judge {
using namespace std;

judge_exception::judge_exception()
    : judge_exception("") {} 

judge_exception::judge_exception(const string &message)
    : message(message), stacktrace(make_shared<boost::stacktrace::stacktrace>()) {}

const char *judge_exception::what() const noexcept {
    return message.c_str();
}

std::ostream &operator<<(std::ostream &os, const judge_exception &ex) {
    os << boost::diagnostic_information(ex) << endl << *ex.stacktrace;
    return os;
}

internal_error::internal_error()
    : judge_exception() {} 

internal_error::internal_error(const string &message)
    : judge_exception(message) {}

network_error::network_error()
    : judge_exception() {} 

network_error::network_error(const string &message)
    : judge_exception(message) {}

database_error::database_error()
    : judge_exception() {} 

database_error::database_error(const string &message)
    : judge_exception(message) {}

}  // namespace judge
