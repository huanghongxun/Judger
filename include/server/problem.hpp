#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include "server/executable.hpp"

namespace judge::server {
using namespace std;
using namespace nlohmann;

/**
 * @brief 表示一组测试数据
 */
struct test_data {
    /**
     * @brief 该组标准测试的输入数据
     * @note 第一个元素将喂给 stdin，剩余的输入数据由选手程序手动打开
     */
    vector<asset_uptr> inputs;

    /**
     * @brief 该组标准测试的输出数据
     * @note 第一个元素将从 stdout 读取，剩余的输出数据由选手程序手动打开
     */
    vector<asset_uptr> outputs;
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
     * @brief 选手用户 id
     * string 可以兼容一切情况
     */
    string user_id;

    /**
     * @brief 题目所属比赛 id，如果不存在比赛则为空
     * string 可以兼容一切情况
     */
    string contest_id;

    /**
     * @brief 题目所属比赛的题目 id，如果不存在比赛则为空
     * string 可以兼容一切情况
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
     * 
     * 示例：
     * {
     *     "CompileCheck": 20,
     *     "MemoryCheck": 0,
     *     "RandomCheck": 50,
     *     "StandardCheck": 20,
     *     "StaticCheck": 10,
     *     "GTestCheck": 0
     * }
     */
    map<string, double> grading;

    /**
     * @brief 标准测试的测试数据
     */
    vector<test_data> test_data;

    /**
     * @brief 内存限制，限制应用程序实际最多能申请多少内存空间
     * @note 单位为 KB，小于 0 的数表示不限制内存空间申请
     */
    int memory_limit;

    /**
     * @brief 时间限制，限制应用程序的实际运行时间
     * @note 单位为毫秒
     */
    int time_limit;

    /**
     * @brief 随机测试组数
     * @note >0 表示存在随机测试，此时 standard 和 random 项起效
     */
    int random_test_times = 0;  // 随机测试次数

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

    unique_ptr<program> spj;
};
}  // namespace judge::server::problem
