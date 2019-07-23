#pragma once

#include <any>
#include <boost/rational.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include "common/concurrent_queue.hpp"
#include "common/messages.hpp"
#include "common/status.hpp"
#include "judge/judger.hpp"
#include "judge/submission.hpp"
#include "program.hpp"

/**
 * 这个头文件包含提交信息
 * 包含：
 * 1. submission 类（表示一个提交）
 * 2. test_case_data 类（表示一个标准测试数据组）
 * 3. judge_task 类（表示一个测试点）
 */
namespace judge {
using namespace std;
using namespace nlohmann;

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
    vector<asset_uptr> inputs;

    /**
     * @brief 该组标准测试的输出数据
     * @note 输入数据由选手程序手动打开，若输出数据文件名为 testdata.out 则和 stdout 比对
     */
    vector<asset_uptr> outputs;
};

/**
 * @brief 表示一个测试点
 */
struct judge_task {
    enum class depends_condition {
        ACCEPTED,         // 要求依赖的测试点通过后才能执行本测试
        PARTIAL_CORRECT,  // 要求依赖的测试点不为 0 分时才能执行本测试
        NON_TIME_LIMIT    // 仅在依赖的测试点超出时间限制后才不继续测试
    };

    /**
     * @brief 测试点类型
     * 提供给 judge_server 来区分测试类型，比如对于 MOJ：
     * 0: CompileCheck
     * 1: MemoryCheck
     * 2: RandomCheck
     * 3: StandardCheck
     * 4: StaticCheck
     * 5: GTestCheck
     */
    uint8_t check_type;

    /**
     * @brief 本测试点使用的 check script
     * 不同的测试点可能有不同的 check script
     * 对于 StaticCheck，check script 需要选择 static
     */
    string check_script;

    /**
     * @brief 本测试点使用的 run script
     * 不同的测试点可能有不同的 run script
     * 对于 MemoryCheck，需要选择 valgrind
     * 对于 GTestCheck，需要选择 gtest
     */
    string run_script;

    /**
     * @brief 本测试点使用的比较脚本
     * 为空时表示使用提交自带的脚本，也就是 submission.compare
     * 对于 MemoryCheck，需要选择 valgrind
     * 对于 GTestCheck，需要选择 gtest
     */
    string compare_script;

    /**
     * @brief 本测试点总分
     */
    boost::rational<int> score;

    /**
     * @brief 本测试点使用的是随机测试还是标准测试
     * 如果是随机测试，那么评测客户端将选择新随机一个测试数据
     * 或者从现有的随机测试数据中随机选取一个测试。
     * 如果是标准测试，那么评测客户端将根据测试点编号评测。
     */
    bool is_random;

    /**
     * @brief 标准测试数据 id，在 submission.test_cases 中进行索引
     * 若为随机测试，此项在评测完成后将会被修改成随机测试使用的数据点
     */
    int testcase_id;

    /**
     * @brief 本测试点依赖哪个测试点，负数表示不依赖任何测试点
     * 依赖指的是，本测试点是否执行，取决于依赖的测试点的状态。
     * 可以实现标准/随机测试点未通过的情况下不测试对应的内存测试的功能。
     * 可以实现所有的测试点均需依赖编译测试的功能。
     */
    int depends_on;

    /**
     * @brief 测试点依赖条件，要求依赖的测试点满足要求时才执行之后的测试点
     * 
     */
    depends_condition depends_cond;

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
};

struct judge_task_result {
    judge_task_result();
    judge_task_result(size_t id, uint8_t type);

    judge::status status;

    /**
     * @brief 本测试点的 id，是 submission.test_cases 的下标
     * 用于返回评测结果的时候统计
     */
    size_t id;

    /**
     * @brief 本测试点的类型
     * 一道题的评测可以包含多种评测类型，方便 judge_server 进行统计
     */
    uint8_t type;

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
     * 如果 status 为 SYSTEM_ERROR/RANDOM_GEN_ERROR/EXECUTABLE_COMPILATION_ERROR 时起效
     */
    string error_log;

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
    filesystem::path run_dir;

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
    filesystem::path data_dir;
};

/**
 * @class judge::programming_submission
 * @brief 一个选手代码提交
 */
struct programming_submission : public submission {
    /**
     * @brief 选手代码的提交语言
     * @note ["c", "cpp", "haskell", "java", "pas", "python2", "python3"]
     */
    string language;

    /**
     * @brief 本题的评分标准
     * 第 0 号必须为编译任务，整道题只能有一个编译任务。
     * 
     * 示例：
     * @code
     * [
     *   { check_type: COMPILE_CHECK, score: 20, check_script: null, compare_script: null, is_random: false }
     * ]
     * @endcode
     * {
     *   "CompileCheck": 20,
     *   "MemoryCheck": 0,
     *   "RandomCheck": 50,
     *   "StandardCheck": 20,
     *   "StaticCheck": 10,
     *   "GTestCheck": 0
     * }
     */
    vector<judge_task> judge_tasks;

    /**
     * @brief 标准测试的测试数据
     */
    vector<test_case_data> test_data;

    /**
     * @brief 选手程序的下载地址和编译命令
     */
    unique_ptr<source_code> submission;

    /**
     * @brief 标准程序的下载地址和编译命令
     * @note 如果存在随机测试，该项有效
     */
    unique_ptr<program> standard;

    /**
     * @brief 随机数据生成程序的下载地址和编译命令
     * @note 如果存在随机测试，该项有效
     */
    unique_ptr<program> random;

    /**
     * @brief 表示题目的评测方式，此项不能为空，
     * 比如你可以选择 diff-all 来使用默认的比较方式，
     * 或者自行提供 source_code 或者 executable 来实现自定义校验
     */
    unique_ptr<program> compare;

    /**
     * @brief 存储评测结果
     */
    vector<judge_task_result> results;

    /**
     * @brief 已经完成了多少个测试点的评测
     */
    size_t finished = 0;
};

struct programming_judger : public judger {
    string type() override;

    bool verify(unique_ptr<submission> &submit) override;

    bool distribute(concurrent_queue<message::client_task> &task_queue, unique_ptr<submission> &submit) override;

    void judge(const message::client_task &task, concurrent_queue<message::client_task> &task_queue, const string &execcpuset) override;

private:
    mutex mut;

    /**
     * @brief 完成评测结果的统计，如果统计的是编译任务，则会分发具体的评测任务
     * 在评测完成后，通过调用 process 函数来完成数据点的统计，如果发现评测完了一个提交，则立刻返回。
     * 因此大部分情况下评测队列不会过长：只会拉取适量的评测，确保评测队列不会过长。
     * 
     * @param result 评测结果
     */
    void process(concurrent_queue<message::client_task> &testcase_queue, programming_submission &submit, const judge_task_result &result);
};

}  // namespace judge
