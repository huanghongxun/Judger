#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;

struct error_report {
    string message;
};

void to_json(json &j, const error_report &report);

struct check_error_report {
    error_report error;
};

void to_json(json &j, const check_error_report &report);

/**
 * @brief 编译检查报告
 * https://gitlab.vmatrix.org.cn/Matrix/Judge/judge-system/wikis/%E6%8E%A5%E5%8F%A3/%E6%A3%80%E6%9F%A5%E5%99%A8/%E7%BC%96%E8%AF%91%E6%A3%80%E6%9F%A5/%E6%8A%A5%E5%91%8A
 */
struct compile_check_report {
    /**
     * @brief MOJ 编译任务的得分
     */
    int grade;

    /**
     * @brief MOJ 编译任务的总分
     */
    int full_grade;

    /**
     * @brief MOJ 编译任务是否通过
     */
    bool pass;

    /**
     * @brief 保存编译信息
     */
    string report;
};

void to_json(json &j, const compile_check_report &report);

/**
 * @brief 静态检查报告
 * https://gitlab.vmatrix.org.cn/Matrix/Judge/judge-system/wikis/%E6%8E%A5%E5%8F%A3/%E6%A3%80%E6%9F%A5%E5%99%A8/%E9%9D%99%E6%80%81%E6%A3%80%E6%9F%A5/%E6%8A%A5%E5%91%8A
 */
struct static_check_report {
    /**
     * @brief MOJ 静态测试得分
     */
    int grade;

    /**
     * @brief MOJ 静态测试任务的总分
     */
    int full_grade;

    map<string, json> report;
};

void to_json(json &j, const compile_check_report &report);

/**
 * @struct 随机测试点的评测结果
 */
struct random_check_case_report {
    /**
     * @brief 评测结果
     * @see judge::status
     */
    string result;

    /**
     * @brief 生成的输入测试数据的文件内容
     * 这个内容是通过随机测试数据生成器产生的
     */
    string stdin;
    
    /**
     * @brief 生成的输出测试数据的文件内容
     * 这个内容是通过标准程序输入随机测试得到的输出结果
     */
    string stdout;

    /**
     * @brief 选手程序的输出结果
     * 这个内容是通过选手程序输入随机测试得到的输出结果
     */
    string subout;
};

void to_json(json &j, const random_check_case_report &report);

/**
 * @brief 随机测试反馈
 * https://gitlab.vmatrix.org.cn/Matrix/Judge/judge-system/wikis/%E6%8E%A5%E5%8F%A3/%E6%A3%80%E6%9F%A5%E5%99%A8/%E9%9A%8F%E6%9C%BA%E6%A3%80%E6%9F%A5/%E6%8A%A5%E5%91%8A
 */
struct random_check_report {
    /**
     * @brief MOJ 随机测试得分
     */
    int grade;

    /**
     * @brief MOJ 随机测试任务的总分
     */
    int full_grade;

    /**
     * @brief 选手程序通过的随机测试数据点数
     */
    int pass_cases;

    /**
     * @brief 随机测试总测试次数
     */
    int total_cases;
    
    /**
     * @brief 所有随机测试的评测结果
     */
    vector<random_check_case_report> report;
};

void to_json(json &j, const random_check_report &report);

struct memory_check_report_report {
    json valgrindoutput;
    string stdin;
};

void to_json(json &j, const memory_check_report_report &report);

/**
 * @brief 内存检查报告
 * https://gitlab.vmatrix.org.cn/Matrix/Judge/judge-system/wikis/%E6%8E%A5%E5%8F%A3/%E6%A3%80%E6%9F%A5%E5%99%A8/%E5%86%85%E5%AD%98%E6%A3%80%E6%B5%8B/%E6%8A%A5%E5%91%8A
 */
struct memory_check_report {
    /**
     * @brief MOJ 内存测试得分
     */
    int grade;

    /**
     * @brief MOJ 内存测试任务的总分
     */
    int full_grade;

    /**
     * @brief 选手程序通过的内存测试数据点数
     */
    int pass_cases;

    /**
     * @brief 内存测试总测试次数
     */
    int total_cases;

    /**
     * @brief Valgrind 测试的输出
     */
    memory_check_report_report report;
};

void to_json(json &j, const memory_check_report &report);

}  // namespace judge::server::moj
