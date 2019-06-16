#pragma once

#include <filesystem>
#include <variant>

namespace judge {
using namespace std;

/**
 * @brief 脚本最大运行时间
 * 单位为秒
 */
extern int script_time_limit;

/**
 * @brief 脚本最大运行内存占用
 * 单位为 KB
 */
extern int script_mem_limit;

/**
 * @brief 脚本最大写入字节数
 * 单位为字节
 */
extern int script_file_limit;

/**
 * @brief 存放 executable 的路径，为项目根目录下的 exec 文件夹
 * 这个只是用来在无法查找到服务器提供的 executable 时的 fallback
 * @defaultValue 假设程序运行在项目根目录下的 bin 文件夹，因此 exec 文件夹在 ../exec
 */
extern filesystem::path EXEC_DIR;

extern filesystem::path COMPILE_DIR;

extern filesystem::path CHROOT_DIR;

namespace message {

/**
 * @brief 一个数据点评测任务
 */
struct test_case_task {
    static constexpr int TYPE = 0;

    /**
     * @brief 本测试点使用的 check script
     * 不同的测试点可能有不同的 check script
     * 对于 MemoryCheck，check script 需要选择 valgrind
     * 对于 RandomCheck，check script 需要选择 random
     */
    char check_script[128];

    /**
     * @brief 本测试点使用的 compare script
     * 不同测试点可能使用不同的 spj (?)
     * 反正在这里标明就可以保证可扩展性了
     */
    char compare_script[128];
};

struct compilation_task {
    static constexpr int TYPE = 1;

    bool cache;

    /**
     * @brief 要编译的程序
     * program 类自己负责处理编译任务，不需要我们来选择 compile script。
     * raw pointer to judge::server::program
     */
    void *program;
};

struct client_task {
    static constexpr long ID = 0;
    static constexpr uint8_t COMPILE_ID = 0;
    static constexpr uint8_t COMPILE_TYPE = 0;

    /**
     * @brief 评测 id
     * 我们通过 get_submission_by_judge_id 来获取 submission 信息
     */
    unsigned judge_id;

    /**
     * @brief 本测试点的 id，不同的 type 的 id 不冲突
     * 用于返回评测结果的时候统计
     */
    uint8_t id;

    /**
     * @brief 本测试点的类型
     * 一道题的评测可以包含多种评测类型，每种评测类型都包含
     * 设定的 check script 和 run
     * 
     * 这个类型由 judge_server 自行决定，比如对于 Sicily、Judge-System 3.0:
     * 0: CompileCheck
     * 1: MemoryCheck
     * 2: RandomCheck
     * 3: StandardCheck
     * 4: StaticCheck
     * 
     * 对于 Judge-System 4.0：
     * 0: CompileCheck
     * 1: OtherCheck
     * 
     * 总之，0 必定是编译任务，其他必定是评测任务
     */
    uint8_t type;

    variant<test_case_task, compilation_task> task;
};

struct judge_result {
    static constexpr long ID = 1;
    
    unsigned judge_id;

    judge::status status;

    /**
     * @brief 本测试点的 id
     * 用于返回评测结果的时候统计
     */
    uint8_t id;

    /**
     * @brief 本测试点的类型
     * 一道题的评测可以包含多种评测类型，方便 judge_server 进行统计
     */
    uint8_t type;
};
}  // namespace message

}  // namespace judge
