#include "env.hpp"
#include <glog/logging.h>
#include <filesystem>
#include "common/utils.hpp"
#include "config.hpp"

namespace judge {
using namespace std;

void put_error_codes() {
    set_env("E_SUCCESS", to_string(judge::error_codes::E_SUCCESS));
    set_env("E_INTERNAL_ERROR", to_string(judge::error_codes::E_INTERNAL_ERROR));
    set_env("E_ACCEPTED", to_string(judge::error_codes::E_ACCEPTED));
    set_env("E_WRONG_ANSWER", to_string(judge::error_codes::E_WRONG_ANSWER));
    set_env("E_PARTIAL_CORRECT", to_string(judge::error_codes::E_PARTIAL_CORRECT));
    set_env("E_PRESENTATION_ERROR", to_string(judge::error_codes::E_PRESENTATION_ERROR));
    set_env("E_COMPILER_ERROR", to_string(judge::error_codes::E_COMPILER_ERROR));
    set_env("E_RANDOM_GEN_ERROR", to_string(judge::error_codes::E_RANDOM_GEN_ERROR));
    set_env("E_COMPARE_ERROR", to_string(judge::error_codes::E_COMPARE_ERROR));
    set_env("E_RUNTIME_ERROR", to_string(judge::error_codes::E_RUNTIME_ERROR));
    set_env("E_FLOATING_POINT", to_string(judge::error_codes::E_FLOATING_POINT));
    set_env("E_SEG_FAULT", to_string(judge::error_codes::E_SEG_FAULT));
    set_env("E_OUTPUT_LIMIT", to_string(judge::error_codes::E_OUTPUT_LIMIT));
    set_env("E_TIME_LIMIT", to_string(judge::error_codes::E_TIME_LIMIT));
    set_env("E_MEM_LIMIT", to_string(judge::error_codes::E_MEM_LIMIT));
}

}  // namespace judge