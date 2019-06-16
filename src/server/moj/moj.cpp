#include "server/moj/moj.hpp"
#include "stl_utils.hpp"
#include "utils.hpp"
#include <fstream>
#include <boost/assign.hpp>

namespace judge::server::moj {
using namespace std;
using namespace nlohmann;

void configuration::init(const filesystem::path &config_path) {
    if (!filesystem::exists(config_path))
        throw runtime_error("Unable to find configuration file");
    ifstream fin(config_path);
    json config;
    fin >> config;
    config.at("submissionReportQueue").get_to(submission_report);
    config.at("choiceSubmissionQueue").get_to(choice_submission);
    config.at("programmingSubmissionQueue").get_to(programming_submission);
    config.at("systemConfig").get_to(system);
}

struct moj_remote_file : public asset {
    moj_remote_file(const string &path)
        : asset(substr_after_last(path, '/')) {
    }

    filesystem::path fetch() override {
        // TODO
    }
};

static asset_uptr moj_url_to_remote_file(const string &path) {
    return make_unique<moj_remote_file>(path);
}

/*
json 示例
{
    "version": 2,
    "sub_id": 1,
    "prob_id": 1,
    "language": "c++",
    "last_update": 1111111111,
    "config": {
        "grading": {
            "c++": {
                "CompileCheck": 20,
                "MemoryCheck": 0,
                "RandomCheck": 50,
                "StandardCheck": 20,
                "StaticCheck": 10,
                "GTestCheck": 0
            }
        },
        "files": {
            "c++": {
                "support": {            //support内为依赖文件, 即提供给学生的不用作答的文件(包括可见和不可见), 学生的提交的代码(submission部分)会和依赖文件一起编译，随机测试时标准答案(standard部分)也会和依赖文件一起编译
                    "source_files": [
                        "main.cpp"
                    ],
                    "header_files": [
                        "BulkItem.h",
                        "BookItem.h"
                    ],
                    "rw_files": []     //普通文件，如可用作文件读写
                },
                "submission": {
                    "source_files": [
                        "BulkItem.cpp",
                        "BookItem.cpp"
                    ],
                    "header_files": []
                },
                "standard": {
                    "source_files": [
                        "BulkItem.cpp",
                        "BookItem.cpp"
                    ],
                    "header_files": [],
                    "input": [
                        "input0"
                    ],
                    "output": [
                        "output0"
                    ]
                },
                "random": [
                    "random.c"    //随机测试分数不为0时，需要提交随机数生成器
                ]
            }
        },
        "compile": {
            "c++": [
                "{INPUT}",
                "-Werror",
                "-g",
                "-lm",
                "-o",
                "{OUTPUT}"
            ]
        },
        "limits": {
            "c++": {
                "memory": 32,
                "time": 1000
            }
        },
        "random": {
            "c++": {
                "language": "c",
                "compile": [
                    "{INPUT}",
                    "-w",
                    "-o",
                    "{OUTPUT}"
                ],
                "run_times": 100
            }
        }
    }
}
*/
void from_json_moj(const json &j, configuration &server, judge::server::submission &submit) {
    submit.category = "moj";

    int version, sub_id, prob_id;
    j.at("version").get_to(version);
    j.at("sub_id").get_to(sub_id);    // type int
    j.at("prob_id").get_to(prob_id);  // type int
    submit.sub_id = to_string(sub_id);
    submit.prob_id = to_string(prob_id);

    const json &config = j.at("config");

    unique_ptr<source_code> submission = make_unique<source_code>(server.exec_mgr);
    unique_ptr<source_code> standard = make_unique<source_code>(server.exec_mgr);

    string language = j.at("language").get<string>();
    submission->language = standard->language = language;

    const json &grading = config.at("grading").at(language);
    for (auto &check : grading.items()) {
        submit.grading[check.key()] = check.value().get<double>();
    }

    const json &files = config.at("files").at(language);
    if (files.count("support")) {
        auto src_url = files.at("support").at("source_files").get<vector<string>>();
        auto hdr_url = files.at("support").at("header_files").get<vector<string>>();
        append(submission->source_files, src_url, moj_url_to_remote_file);
        append(submission->assist_files, hdr_url, moj_url_to_remote_file);
        append(standard->source_files, src_url, moj_url_to_remote_file);
        append(standard->assist_files, hdr_url, moj_url_to_remote_file);
    }

    if (files.count("submission")) {
        auto src_url = files.at("submission").at("source_files").get<vector<string>>();
        append(submission->source_files, src_url, moj_url_to_remote_file);

        auto hdr_url = files.at("submission").at("header_files").get<vector<string>>();
        append(submission->assist_files, hdr_url, moj_url_to_remote_file);
    }

    if (files.count("standard")) {
        auto src_url = files.at("standard").at("source_files").get<vector<string>>();
        append(standard->source_files, src_url, moj_url_to_remote_file);

        auto hdr_url = files.at("standard").at("header_files").get<vector<string>>();
        append(standard->assist_files, hdr_url, moj_url_to_remote_file);
    }

    const json &compile = config.at("compile").at(language);
    compile.get_to(submission->compile_command);
    compile.get_to(standard->compile_command);

    const json &limits = config.at("limits").at(language);
    limits.at("memory").get_to(submit.memory_limit);
    limits.at("time").get_to(submit.time_limit);

    if (config.count("random")) {
        auto random_ptr = make_unique<source_code>(server.exec_mgr);
        const json &random = config.at("random").at(language);
        random.at("language").get_to(random_ptr->language);
        random.at("compile").get_to(random_ptr->compile_command);
        random.at("run_times").get_to(submit.random_test_times);
        submit.random = move(random_ptr);
    }

    submit.submission = move(submission);
    submit.standard = move(standard);
}

// clang-format off
const unordered_map<status, const char *> status_string = boost::assign::map_list_of
    (status::PENDING, "")
    (status::RUNNING, "")
    (status::ACCEPTED, "OK")
    (status::COMPILATION_ERROR, "CE")
    (status::WRONG_ANSWER, "WA")
    (status::RUNTIME_ERROR, "RE")
    (status::TIME_LIMIT_EXCEEDED, "TL")
    (status::MEMORY_LIMIT_EXCEEDED, "ML")
    (status::OUTPUT_LIMIT_EXCEEDED, "OL")
    (status::PRESENTATION_ERROR, "PE")
    (status::RESTRICT_FUNCTION, "RF")
    (status::OUT_OF_CONTEST_TIME, "") // Too Late
    (status::OTHER, "IE") // Internal Error
    (status::COMPILING, "")
    (status::SEGMENTATION_FAULT, "RE")
    (status::FLOATING_POINT_ERROR, "RE");
// clang-format on


void from_json(const json &j, system_config &config) {
    j.at("maxReportIOSize").get_to(config.max_report_io_size);
    j.at("timeLimit").get_to(config.time_limit);
}

void from_json(const json &j, amqp &mq) {
    j.at("port").get_to(mq.port);
    j.at("exchange").get_to(mq.exchange);
    j.at("hostname").get_to(mq.hostname);
    j.at("queue").get_to(mq.queue);
}

void from_json(const json &j, time_limit &limit) {
    j.at("cCompile").get_to(limit.compile_time_limit);
    j.at("crun").get_to(limit.crun);
    j.at("oclint").get_to(limit.oclint);
    j.at("randomGenerate").get_to(limit.random_generator);
    j.at("valgrind").get_to(limit.valgrind);
}

}  // namespace judge::server::moj