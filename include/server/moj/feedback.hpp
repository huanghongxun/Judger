#pragma once

#include <nlohmann/json.hpp>
#include <string>

/**
 * 这个头文件包含 MOJ 的所有测试报告的结构体和 JSON 序列化函数
 */
namespace judge::server::moj {
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

/**
 * @brief 评测出错时的测试组错误报告
 * @code{json}
 * {
 *   "error": {
 *     "message": ""
 *   }
 * }
 * @endcode
 */
struct check_error_report {
    error_report error;
};

void to_json(json &j, const check_error_report &report);

/**
 * @brief 编译检查报告
 * 评测通过时
 * @code{json}
 * {
 *   "grade": 0,        // 得分
 *   "full_grade": 0,   // 总分
 *   "pass": true,
 *   "report": ""
 * }
 * @endcode
 * 
 * 评测未通过时
 * @code{json}
 * {
 *   "grade": 0,        // 得分
 *   "full_grade": 0,   // 总分
 *   "pass": false,
 *   "report": "error"  // 编译报错信息
 * }
 * @endcode
 */
struct compile_check_report {
    static constexpr int TYPE = 0;

    /**
     * @brief MOJ 编译任务的得分
     */
    double grade;

    /**
     * @brief MOJ 编译任务的总分
     */
    double full_grade;

    /**
     * @brief MOJ 编译任务是否通过
     */
    bool pass = false;

    /**
     * @brief 保存编译信息
     */
    string report;
};

void to_json(json &j, const compile_check_report &report);

/**
 * @brief 静态检查报告
 * 
 * 评测通过时
 * @code{json}
 * {
 *   "grade": 0,        // 得分
 *   "full_grade": 0,   // 总分
 *   "report": {
 *     "oclintoutput": [
 *     ]
 *   }
 * }
 * @endcode
 * 
 * 评测未通过时
 * @code{json}
 * {
 *   "grade": 0,        // 得分
 *   "full_grade": 0,   // 总分
 *   "report": {
 *     "oclintoutput": [
 *       {
 *         "category": "naming",
 *         "endColumn": 55,
 *         "endLine": 8,
 *         "message": "Length of variable name `fd` is 2, which is shorter than the threshold of 3",
 *         "path": "main.cpp",
 *         "priority": 3,
 *         "rule": "short variable name",
 *         "startColumn": 3,
 *         "startLine": 8
 *       },
 *       {
 *         "category": "naming",
 *         "endColumn": 3,
 *         "endLine": 36,
 *         "message": "Length of variable name `f` is 1, which is shorter than the threshold of 3",
 *         "path": "main.cpp",
 *         "priority": 3,
 *         "rule": "short variable name",
 *         "startColumn": 3,
 *         "startLine": 12
 *       },
 *       {
 *         "category": "naming",
 *         "endColumn": 13,
 *         "endLine": 18,
 *         "message": "Length of variable name `i` is 1, which is shorter than the threshold of 3",
 *         "path": "main.cpp",
 *         "priority": 3,
 *         "rule": "short variable name",
 *         "startColumn": 5,
 *         "startLine": 18
 *       }
 *     ]
 *   }
 * }
 * @endcode
 */
struct static_check_report {
    static constexpr int TYPE = 4;

    /**
     * @brief MOJ 静态测试得分
     */
    double grade;

    /**
     * @brief MOJ 静态测试任务的总分
     */
    double full_grade;

    map<string, json> report;
};

void to_json(json &j, const static_check_report &report);

/**
 * @struct 随机测试点的评测结果
 */
struct check_case_report {
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

void to_json(json &j, const check_case_report &report);

/**
 * @brief 随机测试反馈
 * 
 * 评测通过时
 * @code{json}
 * {
 *   "grade": 0,         // 得分
 *   "full_grade": 0,    // 总分
 *   "pass_cases": 10,
 *   "total_cases": 10
 * }
 * @endcode
 * 
 * 评测未通过时
 * @code{json}
 * {
 *   "grade": 0,
 *   "full_grade": 0,
 *   "pass_cases": 9,
 *   "total_cases": 10,
 *   "report": [
 *     {
 *       "result": "WA", // 可能是 RE WA 
 *       "stdin":  "1 2\n",
 *       "stdout": "3\n", // 标准答案
 *       "subout": "5\n" // 提交上来的答案
 *     }
 *   ]
 * }
 * @endcode
 */
struct random_check_report {
    static constexpr int TYPE = 2;

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
    vector<check_case_report> report;
};

void to_json(json &j, const random_check_report &report);

struct standard_check_report {
    static constexpr int TYPE = 3;

    /**
     * @brief MOJ 标准测试得分
     */
    double grade;

    /**
     * @brief MOJ 标准测试任务的总分
     */
    double full_grade;

    /**
     * @brief 选手程序通过的标准测试数据点数
     */
    int pass_cases;

    /**
     * @brief 标准测试总测试次数
     */
    int total_cases;

    /**
     * @brief 所有标准测试的评测结果（按顺序给出）
     */
    vector<check_case_report> report;
};

void to_json(json &j, const standard_check_report &report);

struct memory_check_error_report {
    json valgrindoutput;
    string message;
    string stdin;
};

void to_json(json &j, const memory_check_error_report &report);

/**
 * @brief 内存检查报告
 * 
 * 评测通过时
 * @code{json}
 * {
 *   "grade": 0,        // 得分
 *   "full_grade": 0,   // 总分
 *   "pass_cases": 0,   // 通过的测例个数
 *   "total_cases": 0   // 总共测试的测例个数
 * }
 * @endcode
 * 
 * 评测未通过时
 * @code{json}
 * {
 *   "grade": 0,
 *   "full_grade": 0,
 *   "report": [
 *     {
 *       "valgrindoutput": {
 *         "unique": "0x0",
 *         "tid": "1",
 *         "kind": "Leak_DefinitelyLost",
 *         "xwhat": {
 *           "text": "4 bytes in 1 blocks are definitely lost in loss record 1 of 3",
 *           "leakedbytes": "4",
 *           "leakedblocks": "1"
 *         },
 *         "stack": {
 *           "frame": [
 *             {
 *               "ip": "0x4C2E1D6",
 *               "fn": "operator new(unsigned long)",
 *               "file": "vg_replace_malloc.c",
 *               "line": "334"
 *             },
 *             {
 *               "ip": "0x40090E",
 *               "fn": "main"
 *             }
 *           ]
 *         }
 *       },
 *       "stdin": "content of testdata.in"
 *     },
 *     {
 *       "valgrindoutput": {
 *         "unique": "0x1",
 *         "tid": "1",
 *         "kind": "Leak_DefinitelyLost",
 *         "xwhat": {
 *           "text": "4 bytes in 1 blocks are definitely lost in loss record 2 of 3",
 *           "leakedbytes": "4",
 *           "leakedblocks": "1"
 *         },
 *         "stack": {
 *           "frame": [
 *             {
 *               "ip": "0x4C2E1D6",
 *               "fn": "operator new(unsigned long)",
 *               "file": "vg_replace_malloc.c",
 *               "line": "334"
 *             },
 *             {
 *               "ip": "0x40091C",
 *               "fn": "main"
 *             }
 *           ]
 *         }
 *       },
 *       "stdin": "content of testdata.in"
 *     }
 *   ],
 *   "pass_cases": 0,
 *   "total_cases": 0
 * }
 * @endcode
 */
struct memory_check_report {
    static constexpr int TYPE = 1;

    /**
     * @brief MOJ 内存测试得分
     */
    double grade;

    /**
     * @brief MOJ 内存测试任务的总分
     */
    double full_grade;

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
    vector<memory_check_error_report> report;
};

void to_json(json &j, const memory_check_report &report);

}  // namespace judge::server::moj
