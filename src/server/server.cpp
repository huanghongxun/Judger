#include "server/server.hpp"
#include <glog/logging.h>
#include <thread>
#include "common/messages.hpp"

namespace judge::server {

static map<string, unique_ptr<judge_server>> judge_servers;

void register_judge_server(unique_ptr<judge_server> &&judge_server) {
    string category = judge_server->category();
    judge_servers.insert({category, move(judge_server)});
}

judge_server &get_judge_server_by_category(const string &category) {
    // 由于 judge_servers 只会在 server 启动时注册，因此之后都不会修改
    // 因此不需要担心并发问题
    return *judge_servers.at(category).get();
}

static mutex server_mutex;
static unsigned global_judge_id = 0;
// 键为一个唯一的 judge_id
static map<unsigned, submission> submissions;
static map<unsigned, vector<judge::message::task_result>> task_results;
static map<unsigned, size_t> finished_task_cases;

submission &get_submission_by_judge_id(unsigned judge_id) {
    return submissions.at(judge_id);
}

static void summarize(unsigned judge_id) {
    auto &submit = submissions.at(judge_id);
    auto &judge_server = judge_servers.at(submit.category);
    judge_server->summarize(submit, task_results[judge_id]);
    task_results.erase(judge_id);
    finished_task_cases.erase(judge_id);
    submissions.erase(judge_id);

    filesystem::path workdir = RUN_DIR / submit.category / submit.prob_id / submit.sub_id;
    filesystem::remove_all(workdir);
}

/**
 * @brief 处理 client 返回的评测结果
 * 
 * @param task_result client 返回的评测结果
 */
static void process_nolock(message::queue &testcase_queue, const judge::message::task_result &task_result) {
    unsigned judge_id = task_result.judge_id;
    if (!submissions.count(judge_id)) {
        LOG(ERROR) << "Test case exceeded [judge_id: " << judge_id << "]";
        return;  // 跳过本次评测过程
    }

    auto &submit = submissions.at(judge_id);
    if (task_result.status == judge::status::ACCEPTED) {
        // 记录测试成功信息
        auto &task_results = judge::server::task_results[judge_id];
        task_results[0] = task_result;
    }

    DLOG(INFO) << "Testcase [" << submit.category << "-" << submit.prob_id << "-" << submit.sub_id
               << ", status: " << (int)task_result.status << ", runtime: " << task_result.run_time
               << ", memory: " << task_result.memory_used << ", run_dir: " << task_result.run_dir
               << ", data_dir: " << task_result.data_dir << "]";

    size_t finished = ++finished_task_cases[judge_id];
    // 如果当前提交的所有测试点都完成测试，则返回评测结果
    if (finished == submit.test_cases.size()) {
        summarize(judge_id);
        return;  // 跳过本次评测过程
    } else if (finished > submit.test_cases.size()) {
        LOG(ERROR) << "Test case exceeded [" << submit.category << "-" << submit.prob_id << "-" << submit.sub_id << "]";
        return;  // 跳过本次评测过程
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
            return;  // 跳过本次评测过程
        }
    }
}

void process(message::queue &testcase_queue, const message::task_result &task_result) {
    lock_guard<mutex> guard(server_mutex);
    process_nolock(testcase_queue, task_result);
}

static bool fetch_submission_nolock(message::queue &testcase_queue) {
    bool success = false;
    // 尝试从服务器拉取提交，每次都向所有的评测服务器拉取评测任务
    for (auto &[category, server] : judge_servers) {
        judge::server::submission submission;
        if (server->fetch_submission(submission)) {
            LOG(INFO) << "Judging submission [" << submission.category << "-" << submission.prob_id << "-" << submission.sub_id << "]";

            // 检查 judge_server 获取的 submission 是否包含编译任务，且确保至多一个编译任务
            int compile_case;
            bool has_compile_case = false;
            bool has_multiple_compile_case = false;
            for (size_t i = 0; i < submission.test_cases.size(); ++i) {
                auto &test_case = submission.test_cases[i];
                if (test_case.check_type == message::client_task::COMPILE_TYPE) {
                    if (compile_case == -1) {
                        has_compile_case = true;
                        compile_case = i;
                    } else {
                        has_multiple_compile_case = true;
                        break;
                    }
                }
            }

            if (has_multiple_compile_case) {
                LOG(WARNING) << "Submission from [" << submission.category << "-" << submission.prob_id << "-" << submission.sub_id << "] has multiple compilation subtasks.";
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

            if (has_compile_case) {
                // 如果是需要编译的提交，先向评测发送编译任务
                judge::message::client_task client_task = {
                    .judge_id = judge_id,
                    .submit = &submit,
                    .test_check = nullptr,
                    .id = compile_case,
                    .type = judge::message::client_task::COMPILE_TYPE};
                testcase_queue.send_as_pod(client_task);
                success = true;
            } else if (submit.test_cases.empty()) {
                // 如果不存在评测任务，直接返回
                summarize(judge_id);
            } else {
                // 否则直接发送所有的评测任务
                for (size_t i = 0; i < submit.test_cases.size(); ++i) {
                    judge::message::client_task client_task = {
                        .judge_id = judge_id,
                        .submit = &submit,
                        .test_check = nullptr,
                        .id = i,
                        .type = submit.test_cases[i].check_type};
                    testcase_queue.send_as_pod(client_task);
                }
                success = true;
            }
        }
    }
    return success;
}

// 限制拉取提交的总数
// TODO: 将拉取了的提交保存到本地持久化以便评测系统挂掉后重启可以 rejudge
// TODO: 主动 fetch 未评测的数据
bool fetch_submission(message::queue &testcase_queue) {
    lock_guard<mutex> guard(server_mutex);
    return fetch_submission_nolock(testcase_queue);
}

}  // namespace judge::server
