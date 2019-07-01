#pragma once

#include <variant>
#include "common/status.hpp"
#include "server/program.hpp"
#include "server/submission.hpp"

namespace judge::message {
using namespace std;

/**
 * @brief 评测服务端发送给评测客户端的评测任务
 */
struct client_task {
    static constexpr long ID = 0;
    static constexpr uint8_t COMPILE_TYPE = 0;

    /**
     * @brief 评测 id
     * 我们通过 get_submission_by_judge_id 来获取 submission 信息
     */
    unsigned judge_id;

    /**
     * @brief 当前客户端所评测的提交
     */
    judge::server::submission *submit;

    /**
     * @brief 当前客户端所评测的测试组
     * 若为空，表示编译任务，同时 type = 0.
     */
    judge::server::test_check *test_check;

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
     * 5: GTestCheck
     * 
     * 对于 Judge-System 4.0：
     * 0: CompileCheck
     * 1: OtherCheck
     * 
     * 对于 Sicily:
     * 0: CompileCheck
     * 1: StandardCheck
     * 
     * 总之，0 必定是编译任务，其他必定是评测任务。
     */
    uint8_t type;
};

struct task_result {
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

    /**
     * @brief 本测试点程序运行用时
     * 单位为秒
     */
    double run_time;

    /**
     * @brief 本测试点程序运行使用的内存
     * 单位为字节
     */
    int memory_used;

    /**
     * @brief 指向错误报告文件的路径
     * 如果 status 为 SYSTEM_ERROR/RANDOM_GEN_ERROR，那么这个路径表示保存错误信息的文件，
     */
    char path_to_error[256];

    /**
     * @brief 指向当前评测的运行路径
     * 
     * RUN_DIR // 选手程序的运行目录，包含选手程序输出结果
     * ├── run // 运行路径
     * ├── work // 运行路径
     * ├── program.out // 选手程序的 stdout 输出
     * ├── program.err // 选手程序的 stderr 输出
     * └── runguard.err // runguard 的错误输出
     */
    char run_dir[256];

    /**
     * @brief 指向当前评测的数据路径
     * 这个文件夹内必须包含 input 文件夹和 output 文件夹
     * 
     * DATA_DIR
     * ├── input  // 输入数据文件夹
     * │   ├── testdata.in  // 标准输入数据
     * │   └── something.else  // 其他输入数据，由选手自行打开
     * └── output // 输出数据文件夹
     *     └── testdata.out // 标准输出数据
     */
    char data_dir[256];
};

}  // namespace judge::message
