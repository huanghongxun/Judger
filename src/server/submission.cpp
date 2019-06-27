#include "server/submission.hpp"

namespace judge::server {
using namespace std;
using namespace nlohmann;

test_case_data::test_case_data() {}

test_case_data::test_case_data(test_case_data &&other)
    : inputs(move(other.inputs)), outputs(move(other.outputs)) {}

}  // namespace judge::server
