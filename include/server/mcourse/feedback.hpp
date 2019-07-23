#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

/**
 * 这个头文件包含 Matrix Course 的所有测试报告的结构体和 JSON 序列化函数
 */
namespace judge::server::mcourse {
using namespace std;
using namespace nlohmann;

struct judge_request {
    enum submission_type {
        student = 0,
        teacher = 1
    };

    enum problem_type {
        programming = 0,
        choice = 1,
        output = 4,
        program_blank_filling = 5
    };

    /**
     * @brief seaweedfs 需要的授权下载的 token
     * 但是目前课程系统取消了 token 的使用，因此这个没有用
     * TODO: 未来下载文件时支持这类 token 的传递
     */
    string token;

    /**
     * @brief 提交类型
     * 对于学生提交 "student"，我们评测学生程序
     * 对于老师提交 "teacher"，我们评测标准程序是否正确
     */
    submission_type sub_type;

    /**
     * @brief 提交 id
     */
    int sub_id;

    /**
     * @brief 题目 id
     */
    int prob_id;

    /**
     * @brief 题目类型
     * 0. 编程题: programming
     * 1. 选择题: choice
     * 4. 程序输出题: output
     * 5. 完善程序题: programBlankFilling
     */
    problem_type prob_type;

    /**
     * @brief 题目评测信息
     * 结构参见 from_json_mcourse_config
     */
    nlohmann::json config;

    /**
     * @brief 学生提交详细信息
     * 结构参见 from_json_mcourse_config
     */
    nlohmann::json detail;
};

void from_json(const json &j, judge_request &report);

struct judge_report {
    /**
     * @brief submission id
     */
    string sub_id;

    /**
     * @brief problem id
     */
    string prob_id;

    /**
     * @brief grade
     */
    int grade;

    /**
     * @brief report
     */
    json report;

    bool is_complete;
};

/**
 * @brief 错误检查报告
 * @code{json}
 * {
 *   "message": ""
 * }
 * @endcode
 */
struct error_report {
    string message;
};

void to_json(json &j, const error_report &report);

struct compile_check_report {
    /**
     * @brief continue, 表示是否继续测试，编译成功则继续测试，否则不继续
     */
    bool cont;

    /**
     * @brief 通过数据组数 / 总数据组数 × 标准测试总分
     */
    int grade;

    /**
     * @brief pass 表示编译通过，否则存储编译日志
     */
    string message;
};

void to_json(json &j, const compile_check_report &report);

struct check_case_report {
    string stdin;
    string result;
    string stdout;
    string message;
    int timeused;
    int memoryused;
    string standard_stdout;
};

void to_json(json &j, const check_case_report &report);

struct standard_check_report {
    /**
     * @brief 通过数据组数 / 总数据组数 × 标准测试总分
     */
    int grade;

    /**
     * @brief pass 表示编译通过，否则存储编译日志
     */
    vector<check_case_report> cases;
};

void to_json(json &j, const standard_check_report &report);

struct random_check_report {
    /**
     * @brief 通过数据组数 / 总数据组数 × 标准测试总分
     */
    int grade;

    /**
     * @brief pass 表示编译通过，否则存储编译日志
     */
    vector<check_case_report> cases;
};

void to_json(json &j, const random_check_report &report);

struct memory_check_error_report {
    json valgrindoutput;
    string message;
    string stdin;
};

void to_json(json &j, const memory_check_error_report &report);

struct memory_check_report {
    /**
     * @brief 内存测试得分
     */
    int grade;

    /**
     * @brief Valgrind 测试的输出
     */
    vector<memory_check_error_report> report;
};

void to_json(json &j, const memory_check_report &report);

struct static_check_report {
    /**
     * @brief 静态测试得分
     */
    int grade;

    /**
     * @brief oclint 输出，而且是原文输出
     */
    json report;
};

void to_json(json &j, const static_check_report &report);

struct gtest_check_report {
    /**
     * @brief continue，表示是否继续测试
     */
    bool cont;

    /**
     * @brief GTest 得分
     */
    int grade;

    /**
     * @brief 传入的题目配置中的 config."google tests info"
     */
    json info;

    /**
     * @brief 若 Runtime Error 则在这里提示
     */
    string error_message;

    /**
     * @brief 失败的测试组以及这些测试组的分数
     */
    map<string, int> failed_cases;
};

void to_json(json &j, const gtest_check_report &report);

}  // namespace judge::server::mcourse
