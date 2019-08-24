#pragma once

#include <filesystem>

namespace judge {

enum error_codes {
    E_SUCCESS = 0,
    E_INTERNAL_ERROR = 2,

    E_ACCEPTED = 42,
    E_WRONG_ANSWER = 43,
    E_PRESENTATION_ERROR = 44,
    E_COMPILER_ERROR = 45,
    E_RANDOM_GEN_ERROR = 46,
    E_COMPARE_ERROR = 47,
    E_RUNTIME_ERROR = 48,
    E_FLOATING_POINT = 49,
    E_SEG_FAULT = 50,
    E_OUTPUT_LIMIT = 51,
    E_TIME_LIMIT = 52,
    E_MEM_LIMIT = 53,
    E_PARTIAL_CORRECT = 54
};

extern int MAX_RANDOM_DATA_NUM;

extern int SCRIPT_MEM_LIMIT;

extern int SCRIPT_TIME_LIMIT;

extern int SCRIPT_FILE_LIMIT;

/**
 * @brief 存放 executable 的路径，为项目根目录下的 exec 文件夹
 * 这个只是用来在无法查找到服务器提供的 executable 时的 fallback
 * @defaultValue 假设程序运行在项目根目录下的 bin 文件夹，因此 exec 文件夹在 ../exec
 * 
 * EXEC_DIR
 * ├── check // 测试执行的脚本
 * │   ├── standard // 标准和随机测试的辅助测试脚本
 * │   ├── static // 静态测试脚本
 * │   └── memory // 内存检查（输入数据标准或随机）的帮助脚本
 * ├── compare // 比较脚本
 * │   ├── diff-all // 精确比较，如果有空白字符差异则返回 PE
 * │   └── diff-ign-space // 忽略行末空格和文末空行的比较脚本，不会有 PE
 * ├── compile // 编译脚本
 * │   ├── c // C 语言程序编译脚本
 * │   ├── cpp // C++ 语言程序编译脚本
 * │   ├── make // Makefile 编译脚本
 * │   ├── cmake // CMake 编译脚本
 * │   └── ...
 * └── run // 运行脚本
 *     └── standard // 标准运行脚本
 */
extern std::filesystem::path EXEC_DIR;

/**
 * @brief 
 * 
 * CACHE_DIR
 * ├── sicily // category id
 * │   └── 1001 // problem id
 * │       ├── standard // 标准程序的缓存目录（代码和可执行文件）
 * │       ├── standard_data // 标准输入输出数据的缓存目录
 * │       ├── random // 随机数据生成器的缓存目录（代码和可执行文件）
 * │       └── random_data // 随机输入输出数据的缓存目录
 * ├── moj
 * └── mcourse
 */
extern std::filesystem::path CACHE_DIR;

/**
 * @brief 只存放将要评测的测试数据的文件夹，测试完成后数据将被删除
 * 若将这个文件夹放进内存盘，可以加速选手程序的 IO 性能，
 * 避免系统进入 IO 瓶颈导致评测的不公平。
 * 
 * DATA_DIR
 * ├── ABCDEFG // 随机生成的 uuid
 * │   ├── input // 当前测试数据组的输入数据文件夹
 * │   └── output // 当前测试数据组的输出数据文件夹
 * └── ...
 */
extern std::filesystem::path DATA_DIR;

/**
 * @brief 是否启用 DATA_DIR
 */
extern bool USE_DATA_DIR;

/**
 * @brief 选手程序编译及运行的根目录
 * RUN_DIR 的文件结构如下：
 * 
 * RUN_DIR
 * ├── sicily // category id
 * │   └── 1001 // problem id
 * │       └── 5100001 // submission id
 * │           ├── compile // 选手程序的代码和编译目录
 * │           │   ├── main.cpp // 选手程序的主代码（示例）
 * │           │   ├── program // 选手程序的执行入口（可能是生成的运行脚本）
 * │           │   ├── compile.out // 编译器的输出
 * │           │   └── (Makefile) // 允许 Makefile
 * │           ├── run-[client_id] // 选手程序的运行目录，包含选手程序输出结果
 * │           │   ├── run // 运行路径
 * │           │   │   └── testdata.out // 选手程序的 stdout 输出
 * │           │   ├── feedback // 比较器结果路径
 * │           │   │   ├── judgemessage.txt // 比较器的输出信息
 * │           │   │   ├── judgeerror.txt // 比较器的错误信息
 * │           │   │   └── score.txt // 如果比较器支持部分分，那么这里将保存部分分数
 * │           │   ├── work // 构建的根目录
 * │           │   ├── merged // 构建好的 chroot 环境
 * │           │   │   ├── judge // 挂载 compile 文件夹、输入数据文件夹、run 文件夹
 * │           │   │   ├── testin // 挂载输入数据文件夹
 * │           │   │   ├── testout // 挂载输出数据文件夹
 * │           │   │   ├── compare // 挂载比较脚本文件夹
 * │           │   │   └── run // 挂载运行脚本文件夹
 * │           │   ├── program.err // 选手程序的 stderr 输出
 * │           │   ├── program.meta // 选手程序的运行信息
 * │           │   ├── compare.err // 比较程序的 stderr 输出
 * │           │   ├── compare.meta // 比较程序的运行信息
 * │           │   └── system.out // 检查脚本的日志
 * │           └── run-...
 * ├── moj
 * └── mcourse
 */
extern std::filesystem::path RUN_DIR;

/**
 * @brief 配置好的 chroot 路径
 * 必须是通过 exec/chroot_make.sh 创建的 chroot 环境
 */
extern std::filesystem::path CHROOT_DIR;

/**
 * @brief 存放帮助的 Python 脚本的路径
 * 必须和源代码仓库根目录下的 script 文件夹一致
 */
extern std::filesystem::path SCRIPT_DIR;

/**
 * @brief 是否开启 DEBUG 模式
 * 如果开启 DEBUG 模式，评测系统将不再检查程序是否在特权模式下执行，
 * 并且不会删除产生的提交目录，以便手动检查测试产生的文件内容是否符合预期。
 */
extern bool DEBUG;

}  // namespace judge
