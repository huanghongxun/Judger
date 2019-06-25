#pragma once

#include <boost/rational.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include "server/executable.hpp"

namespace judge::server {
using namespace std;
using namespace nlohmann;

/**
 * @brief 表示一组测试数据
 */
struct test_case_data {
    test_case_data();
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

struct test_check {
    /**
     * @brief 
     * 
     */
    uint8_t check_type;

    /**
     * @brief 本测试点使用的 check script
     * 不同的测试点可能有不同的 check script
     * 对于 MemoryCheck，check script 需要选择 memory
     * 对于 StaticCheck，check script 需要选择 static
     */
    string check_script;

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
     * @brief 标准测试数据 id
     * 若为随机测试，此项无用
     */
    unsigned testcase_id;
};

/**
 * @class server::moj::submission
 * @brief 一个选手代码提交
 */
struct submission {
    /**
     * @brief 选手代码归属的程序
     * 如：oj, course, sicily 来区分数据包应该发送给哪一个消息队列
     */
    string category;

    /**
     * @brief 选手代码提交的 id
     * string 可以兼容一切情况
     */
    string sub_id;

    /**
     * @brief 选手代码提交的题目 id
     * string 可以兼容一切情况
     */
    string prob_id;

    /**
     * @brief 队列 id
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    string queue_id;

    /**
     * @brief 选手用户 id
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    string user_id;

    /**
     * @brief 题目所属比赛 id，如果不存在比赛则为空
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    string contest_id;

    /**
     * @brief 题目所属比赛的题目 id，如果不存在比赛则为空
     * string 可以兼容一切情况
     * 此项是可选项，给特定的评测服务器使用
     */
    string contest_prob_id;

    /**
     * @brief 选手代码的提交语言
     * @note ["c", "cpp", "haskell", "java", "pas", "python2", "python3"]
     */
    string language;

    /**
     * @brief 题目最后更新时间
     * 根据最后更新时间来确定是否需要重新编译随机数据生成器、标程、生成随机测试数据
     */
    int last_update;

    /**
     * @brief 提交时间
     */
    time_t submit_time;

    /**
     * @brief 比赛开始时间
     */
    time_t contest_start_time;

    /**
     * @brief 比赛结束时间
     */
    time_t contest_end_time;

    /**
     * @brief 本题的评分标准
     * 第 0 号必须为编译任务，整道题只能有一个编译任务。
     * 
     * 示例：
     * @code
     * [
     * { check_type: COMPILE_CHECK, score: 20, check_script: null, compare_script: null, is_random: false }
     * ]
     * @endcode
     * {
     *     "CompileCheck": 20,
     *     "MemoryCheck": 0,
     *     "RandomCheck": 50,
     *     "StandardCheck": 20,
     *     "StaticCheck": 10,
     *     "GTestCheck": 0
     * }
     */
    vector<test_check> test_cases;

    /**
     * @brief 标准测试的测试数据
     */
    vector<test_case_data> test_data;

    /**
     * @brief 内存限制，限制应用程序实际最多能申请多少内存空间
     * @note 单位为 KB，小于 0 的数表示不限制内存空间申请
     */
    int memory_limit;

    /**
     * @brief 时间限制，限制应用程序的实际运行时间
     * @note 单位为毫秒
     */
    double time_limit;

    /**
     * @brief 随机测试组数
     * @note >0 表示存在随机测试，此时 standard 和 random 项起效
     */
    size_t random_test_times = 0;  // 随机测试次数

    /**
     * @brief 选手程序的下载地址和编译命令
     */
    unique_ptr<program> submission;

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
};
}  // namespace judge::server
