#pragma once

#include <any>
#include <boost/rational.hpp>
#include <filesystem>
#include <map>
#include "common/concurrent_queue.hpp"
#include "common/io_utils.hpp"
#include "common/messages.hpp"
#include "common/status.hpp"
#include "judge/judger.hpp"
#include "judge/submission.hpp"
#include "program.hpp"

/**
 * 这个头文件包含提交信息
 * 包含：
 * 1. programming_submission 类（表示一个编程题提交）
 * 2. test_case_data 类（表示一个标准测试数据组）
 * 3. judge_task 类（表示一个测试点）
 */
namespace judge {

/**
 * @brief 表示一组标准测试数据
 * 标准测试数据和测试点的顺序没有必然关系，比如一道题同时存在
 * 标准测试和内存测试，两种测试都需要使用标准测试数据进行评测，
 * 这种情况下会有多组测试点使用同一个标准测试数据普
 */
struct test_case_data {
    /**
     * @brief Construct a new test case data object
     * 默认构造函数
     */
    test_case_data();

    /**
     * @brief Construct a new test case data object
     * 这个类中使用了 unique_ptr，因此只能移动
     * @param other 要移动的对象
     */
    test_case_data(test_case_data &&other);

    /**
     * @brief 该组标准测试的输入数据
     * @note 输入数据由选手程序手动打开，若输入数据文件名为 testdata.in 则喂给 stdin
     */
    std::vector<asset_uptr> inputs;

    /**
     * @brief 该组标准测试的输出数据
     * @note 输入数据由选手程序手动打开，若输出数据文件名为 testdata.out 则和 stdout 比对
     */
    std::vector<asset_uptr> outputs;
};

/**
 * @brief 表示一个测试点
 */
struct judge_task {
    enum class dependency_condition {
        ACCEPTED,         // 要求依赖的测试点通过后才能执行本测试
        PARTIAL_CORRECT,  // 要求依赖的测试点不为 0 分时才能执行本测试
        NON_TIME_LIMIT    // 仅在依赖的测试点超出时间限制后才不继续测试
    };

    /**
     * @brief 本测试点的类型，提供给 judge_server 来区分测试类型
     * 一道题的评测可以包含多种评测类型
     * 
     * 这个类型由 judge_server 自行决定，比如对于 MOJ、Matrix Course:
     * 0: CompileCheck
     * 1: MemoryCheck
     * 2: RandomCheck
     * 3: StandardCheck
     * 4: StaticCheck
     * 5: GTestCheck
     * 
     * 对于 Sicily、Judge-System 4.0 不分 type
     */
    uint8_t check_type;

    /**
     * @brief 本测试点使用的 check script
     * 不同的测试点可能有不同的 check script
     * 对于 StaticCheck，check script 需要选择 static
     */
    std::string check_script;

    /**
     * @brief 本测试点使用的 run script
     * 不同的测试点可能有不同的 run script
     * 对于 MemoryCheck，需要选择 valgrind
     * 对于 GTestCheck，需要选择 gtest
     */
    std::string run_script;

    /**
     * @brief 本测试点使用的比较脚本
     * 为空时表示使用提交自带的脚本，也就是 submission.compare
     * 对于 MemoryCheck，需要选择 valgrind
     * 对于 GTestCheck，需要选择 gtest
     */
    std::string compare_script;

    /**
     * @brief 本测试点总分
     * 在评测 4.0 接口中不使用
     */
    boost::rational<int> score;

    /**
     * @brief 本测试点使用的是随机测试还是标准测试
     * 如果是随机测试，那么评测客户端将选择新随机一个测试数据
     * 或者从现有的随机测试数据中随机选取一个测试。
     * 如果是标准测试，那么评测客户端将根据测试点编号评测。
     */
    bool is_random = false;

    /**
     * @brief 标准测试数据 id，在 submission.test_cases 中进行索引
     * 若为随机测试，此项为第几组随机测试。
     * 随机数据生成器可能可以根据第几组随机测试而生成不同规模、不同生成方式的数据。
     */
    int testcase_id = -1;

    /**
     * @brief 该组随机测试使用的测试数据编号
     * 对于每组随机测试，我们需要缓存一批随机测试数据。
     * 由于随机测试生成器根据输入的 testcase_id 来决定生成方式，传入同样的
     * testcase_id 可以生成一批性质相同的测试数据，此时为了支持其他评测任务
     * 依赖当前使用的随机测试数据，我们需要额外保存使用了哪一个缓存好的测试数据。
     * 在评测完成后该域将修改为该组随机测试使用哪一个缓存的测试数据点。
     */
    int subcase_id = -1;

    /**
     * @brief 本测试点依赖哪个测试点，负数表示不依赖任何测试点
     * 依赖指的是，本测试点是否执行，取决于依赖的测试点的状态。
     * 可以实现标准/随机测试点未通过的情况下不测试对应的内存测试的功能。
     * 可以实现所有的测试点均需依赖编译测试的功能。
     */
    int depends_on = -1;

    /**
     * @brief 测试点依赖条件，要求依赖的测试点满足要求时才执行之后的测试点
     * 
     */
    dependency_condition depends_cond = dependency_condition::ACCEPTED;

    /**
     * @brief 内存限制，限制应用程序实际最多能申请多少内存空间
     * @note 单位为 KB，小于 0 的数表示不限制内存空间申请
     */
    int memory_limit = -1;

    /**
     * @brief 时间限制，限制应用程序的实际运行时间
     * @note 单位为秒
     */
    double time_limit = -1;

    /**
     * @brief 文件输出限制，限制应用程序实际最多能写入磁盘多少数据
     * @note 单位为 KB，小于 0 的数表示不限制内存空间申请
     */
    int file_limit = -1;

    /**
     * @brief 进程数限制，限制应用程序实际最多能打开多少个进程
     * @note ，小于 0 的数表示不限制进程数
     */
    int proc_limit = -1;

    /**
     * @brief 运行命令，传递给选手程序使用
     */
    std::vector<std::string> run_args;

    /**
     * @brief 该评测点的显示名称
     * 用于监控系统查看
     */
    std::string name;

    /**
     * @brief 该评测点所需要的核心数
     */
    size_t cores = 1;
};

struct judge_task_result {
    judge_task_result();
    judge_task_result(std::size_t id);

    judge::status status;

    /**
     * @brief 本测试点的 id，是 submission.judge_tasks 的下标
     * 用于返回评测结果的时候统计
     */
    std::size_t id;

    /**
     * @brief 对于部分分情况，保存 0~1 范围内的部分分比例
     * 测试点的真实分数将根据测试点总分乘上 score 计算得到
     */
    boost::rational<int> score;

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
     * @brief 错误报告
     * 会保存 check script 生成的 system.out，会保存所有脚本文件的日志
     */
    std::string error_log;

    /**
     * @brief 报告信息
     * 对于 Google Test，Oclint，这里将保存测试结果的详细信息
     */
    std::string report;

    /**
     * @brief 指向当前评测的运行路径
     * 
     * RUN_DIR // 选手程序的运行目录，包含选手程序输出结果
     * ├── run // 运行路径
     * │   └── testdata.out // 选手程序的 stdout 输出
     * ├── work // 工作路径，提供给 check script 用来 mount 的
     * ├── program.err // 选手程序的 stderr 输出
     * └── runguard.err // runguard 的错误输出
     */
    std::filesystem::path run_dir;

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
    std::filesystem::path data_dir;
};

/**
 * @brief 一个选手代码提交
 */
struct programming_submission : public submission {
    /**
     * @brief 本题的评测任务
     * 
     */
    std::vector<judge_task> judge_tasks;

    /**
     * @brief 标准测试的测试数据
     */
    std::vector<test_case_data> test_data;

    /**
     * @brief 选手程序的下载地址和编译命令
     */
    std::unique_ptr<source_code> submission;

    /**
     * @brief 标准程序的下载地址和编译命令
     * @note 如果存在随机测试，该项有效
     */
    std::unique_ptr<program> standard;

    /**
     * @brief 随机数据生成程序的下载地址和编译命令
     * @note 如果存在随机测试，该项有效
     */
    std::unique_ptr<program> random;

    /**
     * @brief 表示题目的评测方式，此项不能为空，
     * 比如你可以选择 diff-all 来使用默认的比较方式，
     * 或者自行提供 source_code 或者 executable 来实现自定义校验
     */
    std::unique_ptr<program> compare;

    /**
     * @brief 存储评测结果
     */
    std::vector<judge_task_result> results;

    /**
     * @brief 已经完成了多少个测试点的评测
     */
    std::size_t finished = 0;

    /**
     * @brief 题目读锁，提交销毁后会自动释放锁
     * 正在评测的提交需要使用读锁锁住题目文件夹以避免题目更新时导致数据错误。
     */
    scoped_file_lock problem_lock;

    /**
     * @brief 提交写锁，提交销毁后会自动释放锁
     * 可能会出现连续两个同 sub_id 的提交，因此我们必须锁住提交文件夹来
     * 防止两个提交的文件发生冲突（rejudge 会导致多个同 sub_id 的提交）
     */
    scoped_file_lock submission_lock;
};

/**
 * @brief 评测编程题的 Judger 类，编程题评测的逻辑都在这个类里
 */
struct programming_judger : public judger {
    std::string type() const override;

    bool verify(submission &submit) const override;

    bool distribute(concurrent_queue<message::client_task> &task_queue, submission &submit) const override;

    void judge(const message::client_task &task, concurrent_queue<message::client_task> &task_queue, const std::string &execcpuset) const override;
};

}  // namespace judge
