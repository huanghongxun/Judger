#include "config.hpp"

namespace judge {
using namespace std;

int MAX_RANDOM_DATA_NUM = 100;
int SCRIPT_MEM_LIMIT = 1 << 18;   // 256M
int SCRIPT_TIME_LIMIT = 10;       // 10s
int SCRIPT_FILE_LIMIT = 1 << 19;  // 512M

filesystem::path EXEC_DIR;
filesystem::path CACHE_DIR;
filesystem::path DATA_DIR;
bool USE_DATA_DIR = false;
filesystem::path RUN_DIR;
filesystem::path CHROOT_DIR;
bool DEBUG = false;

}  // namespace judge
