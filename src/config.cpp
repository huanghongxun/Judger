#include "config.hpp"

namespace judge {
using namespace std;

int MAX_RANDOM_DATA_NUM = 100;

filesystem::path EXEC_DIR;
filesystem::path CACHE_DIR;
filesystem::path DATA_DIR;
bool USE_DATA_DIR = false;
filesystem::path RUN_DIR;
filesystem::path CHROOT_DIR;

}  // namespace judge
