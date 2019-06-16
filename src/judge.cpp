#include "judge.hpp"
#include <cstdlib>

void judge(struct judging &judging) {
    setenv("MEMLIMIT", boost::judging.memory_limit);
    setenv("FILELIMIT", judging.output_limit);

    if (judging.entry_point.size())
        setenv("")
}