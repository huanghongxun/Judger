#pragma once

#include <nlohmann/json.hpp>
#include <string>

/**
 * 这个头文件包含 Matrix Course 的所有测试报告的结构体和 JSON 序列化函数
 */
namespace judge::server::mcourse {
using namespace std;
using namespace nlohmann;

struct judge_report {
    /**
     * @brief submission id
     */
    unsigned sub_id;

    /**
     * @brief problem id
     */
    unsigned prob_id;

    /**
     * @brief grade
     */
    double grade;

    /**
     * @brief report
     */
    json report;

    /**
     * @brief whether judgement is complete
     */
    bool is_complete;
};

void to_json(json &j, const judge_report &report);

struct check_report {
    /**
     * @brief continue, 表示是否继续测试
     */
    bool cont;

    /**
     * @brief 通过数据组数 / 总数据组数 × 标准测试总分
     */
    int grade;

    /**
     * @brief 当前的 check_report 表示的是哪种测试
     * 可以是：
     * "compile check": 表示一个编译任务
     * "static check": 表示一个静态测试
     * "google tests": 表示多个 GTest
     * "standard tests": 表示多个标准测试
     * "random tests": 表示多个随机测试
     * "memory check": 表示内存测试
     */
    string test_name;

    json report;
};

struct compile_check_report {
    string message;
};

void to_json(json &j, const compile_check_report &report);

struct gtest_check_report {
    /**
     * @brief continue，表示是否继续测试
     */
    bool cont;
};

}  // namespace judge::server::mcourse
