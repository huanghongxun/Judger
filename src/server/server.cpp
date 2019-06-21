#include "server/server.hpp"
#include <glog/logging.h>
#include "common/messages.hpp"

namespace judge::server {

static map<string, unique_ptr<judge_server>> judge_servers;

void register_judge_server(unique_ptr<judge_server> &&judge_server) {
    string category = judge_server->category();
    judge_servers.insert({category, move(judge_server)});
}

static unsigned global_judge_id = 0;
// 键为一个唯一的 judge_id
static map<unsigned, submission> submissions;
static map<unsigned, vector<judge::message::task_result>> task_results;
static map<unsigned, size_t> finished_task_cases;

submission &get_submission_by_judge_id(unsigned judge_id) {
    return submissions.at(judge_id);
}

void summarize(unsigned judge_id) {
    auto &submit = submissions.at(judge_id);
    auto &judge_server = judge_servers.at(submit.category);
    judge_server->summarize(submit, task_results[judge_id]);
    task_results.erase(judge_id);
    finished_task_cases.erase(judge_id);
    submissions.erase(judge_id);

    filesystem::path workdir = RUN_DIR / submit.category / submit.prob_id / submit.sub_id;
    filesystem::remove_all(workdir);
}

void server(message::queue &testcase_queue, message::queue &result_queue) {
    while (true) {
        while (true) {
            // 从结果队列中获取评测客户端的评测结果，并更新对应 submission 的状态
            judge::message::task_result task_result;
            auto envelope = result_queue.recv_from_pod(task_result, message::queue::NO_WAIT);
            if (envelope.success) {
                unsigned judge_id = task_result.judge_id;
                if (!submissions.count(judge_id)) {
                    LOG(ERROR) << "Test case exceeded [judge_id: " << judge_id << "]";
                    continue;  // 跳过本次评测过程
                }

                auto &submit = submissions.at(judge_id);
                if (task_result.status == judge::status::ACCEPTED) {
                    // 记录测试成功信息
                    auto &task_results = judge::server::task_results[judge_id];
                    task_results[0] = task_result;
                }

                size_t finished = ++finished_task_cases[judge_id];
                // 如果当前提交的所有测试点都完成测试，则返回评测结果
                if (finished == submit.test_cases.size()) {
                    summarize(judge_id);
                    continue;  // 跳过本次评测过程
                } else if (finished > submit.test_cases.size()) {
                    LOG(ERROR) << "Test case exceeded [category: " << submit.category << ", prob: " << submit.prob_id << ", sub: " << submit.sub_id << "]";
                    continue;  // 跳过本次评测过程
                }

                // 编译任务比较特殊
                if (task_result.type == judge::message::client_task::COMPILE_TYPE) {
                    if (task_result.status == judge::status::ACCEPTED) {
                        // 如果编译成功，则分发评测任务
                        for (size_t i = 1; i < submit.test_cases.size(); ++i) {
                            auto &test_case = submit.test_cases[i];
                            judge::message::client_task client_task = {
                                .judge_id = judge_id,
                                .submit = &submit,
                                .test_check = &test_case,
                                .id = (uint8_t)i,
                                .type = test_case.check_type};
                            testcase_queue.send_as_pod(client_task);
                        }
                    } else {
                        // 否则评测直接结束，返回评测结果
                        summarize(judge_id);
                        continue;  // 跳过本次评测过程
                    }
                }
            } else {
                break;
            }
        }

        {
            // 尝试从服务器拉取提交，如果拉取到了，那么生成一个编译任务发送给评测客户端

            for (auto &[category, server] : judge_servers) {
                judge::server::submission submission;
                if (server->fetch_submission(submission)) {
                    bool compile_case_valid = false;
                    for (size_t i = 0; i < submission.test_cases.size(); ++i) {
                        auto &test_case = submission.test_cases[i];
                        if (i == 0 && test_case.check_type == judge::message::client_task::COMPILE_TYPE) {
                            compile_case_valid = true;
                        } else if (i != 0 && test_case.check_type == judge::message::client_task::COMPILE_TYPE) {
                            compile_case_valid = false;
                            break;
                        }
                    }

                    if (!compile_case_valid) {
                        LOG(WARNING) << "Submission from [" << submission.category << ", " << submission.prob_id << ", " << submission.sub_id << "] is not valid";
                        continue;
                    }

                    // 给当前提交分配一个 judge_id 给评测服务端和客户端进行识别
                    // global_judge_id++ 就算溢出也无所谓，一般情况下不会同时评测这么多提交
                    unsigned judge_id = global_judge_id++;
                    submissions[judge_id] = move(submission);
                    auto &submit = submissions.at(judge_id);

                    if (!submit.submission) continue;

                    // 初始化当前提交的所有评测任务状态为 PENDING
                    vector<judge::message::task_result> task_results;
                    task_results.resize(submit.test_cases.size());
                    for (size_t i = 0; i < task_results.size(); ++i) {
                        task_results[i].judge_id = judge_id;
                        task_results[i].status = judge::status::PENDING;
                        task_results[i].id = i;
                        task_results[i].type = submit.test_cases[i].check_type;
                    }
                    judge::server::task_results[judge_id] = move(task_results);

                    // 向评测发送编译任务
                    judge::message::client_task client_task = {
                        .judge_id = judge_id,
                        .submit = &submit,
                        .test_check = nullptr,
                        .id = judge::message::client_task::COMPILE_ID,
                        .type = judge::message::client_task::COMPILE_TYPE};
                    testcase_queue.send_as_pod(client_task);
                }
            }
        }

        usleep(10 * 1000);  // sleep for 10ms.
    }
}
}  // namespace judge::server
