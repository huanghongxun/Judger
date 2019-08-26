#include "common/status.hpp"
#include <boost/assign.hpp>
#include <unordered_map>

namespace judge {
using namespace std;

// clang-format off
static const unordered_map<status, const char *> status_string = boost::assign::map_list_of
    (status::PENDING, "Pending")
    (status::RUNNING, "Running")
    (status::ACCEPTED, "Accepted")
    (status::PARTIAL_CORRECT, "Partial Correct")
    (status::COMPILATION_ERROR, "Compilation Error")
    (status::EXECUTABLE_COMPILATION_ERROR, "Executable Compilation Error")
    (status::DEPENDENCY_NOT_SATISFIED, "Dependency Not Satisfied")
    (status::WRONG_ANSWER, "Wrong Answer")
    (status::RUNTIME_ERROR, "Runtime Error")
    (status::TIME_LIMIT_EXCEEDED, "Time Limit Exceeded")
    (status::MEMORY_LIMIT_EXCEEDED, "Memory Limit Exceeded")
    (status::OUTPUT_LIMIT_EXCEEDED, "Output Limit Exceeded")
    (status::PRESENTATION_ERROR, "Presentation Error")
    (status::RESTRICT_FUNCTION, "Restrict Function")
    (status::OUT_OF_CONTEST_TIME, "Out of Contest Time")
    (status::COMPILING, "Compiling")
    (status::SEGMENTATION_FAULT, "Segmentation Fault")
    (status::FLOATING_POINT_ERROR, "Floating Point Error")
    (status::RANDOM_GEN_ERROR, "Random Gen Error")
    (status::COMPARE_ERROR, "Compare Error")
    (status::SYSTEM_ERROR, "System Error");
// clang-format on

const char *get_display_message(status stat) {
    return status_string.at(stat);
}

}
