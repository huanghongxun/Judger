#include "config.hpp"
#include "utils.hpp"

namespace judge {
using namespace std;

bool enable_sicily;
bool enable_moj;
int script_time_limit;
int script_mem_limit;
int script_file_limit;
int PROC_LIMIT;

filesystem::path EXEC_DIR;
filesystem::path CACHE_DIR;
filesystem::path RUN_DIR;
filesystem::path CHROOT_DIR;
}  // namespace judge