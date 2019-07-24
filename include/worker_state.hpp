#pragma once

namespace judge {

enum class worker_state {
    START,
    JUDGING,
    IDLE,
    CRASHED,
    STOPPED
};

}  // namespace judge
