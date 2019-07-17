#include "server/server.hpp"
#include <glog/logging.h>
#include <thread>
#include <boost/stacktrace.hpp>
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

static void report_failure(submission &submit) {
    auto &judge_server = judge_servers.at(submit.category);
    judge_server->summarize_invalid(submit);
}

/**
 * @brief 处理 client 返回的评测结果
 * 
 * @param task_result client 返回的评测结果
 */
static void process_nolock(concurrent_queue<message::client_task> &testcase_queue, const judge::message::task_result &task_result) {
    unsigned judge_id = task_result.judge_id;
    if (!submissions.count(judge_id)) {
        LOG(ERROR) << "Test case exceeded [judge_id: " << judge_id << "]";
        return;  // 跳过本次评测过程
    }

    auto &submit = submissions.at(judge_id);
    // 记录测试信息
    auto &task_results = judge::server::task_results[judge_id];
    task_results[task_result.id] = task_result;

    DLOG(INFO) << "Testcase [" << submit.category << "-" << submit.prob_id << "-" << submit.sub_id
               << ", status: " << (int)task_result.status << ", runtime: " << task_result.run_time
               << ", memory: " << task_result.memory_used << ", run_dir: " << task_result.run_dir
               << ", data_dir: " << task_result.data_dir << "]";

    for (size_t i = 0; i < submit.test_cases.size(); ++i) {
        test_check &kase = submit.test_cases[i];
        // 寻找依赖当前评测点的评测点
        if (kase.depends_on == (int)task_result.id) {
            bool satisfied;
            switch (kase.depends_cond) {
                case test_check::depends_condition::ACCEPTED:
                    satisfied = task_result.status == status::ACCEPTED;
                    break;
                case test_check::depends_condition::PARTIAL_CORRECT:
                    satisfied = task_result.status == status::PARTIAL_CORRECT ||
                                task_result.status == status::ACCEPTED;
                    break;
                case test_check::depends_condition::NON_TIME_LIMIT:
                    satisfied = task_result.status != status::SYSTEM_ERROR &&
                                task_result.status != status::COMPARE_ERROR &&
                                task_result.status != status::COMPILATION_ERROR &&
                                task_result.status != status::DEPENDENCY_NOT_SATISFIED &&
                                task_result.status != status::TIME_LIMIT_EXCEEDED &&
                                task_result.status != status::EXECUTABLE_COMPILATION_ERROR &&
                                task_result.status != status::OUT_OF_CONTEST_TIME &&
                                task_result.status != status::RANDOM_GEN_ERROR;
                    break;
            }

            if (satisfied) {
                judge::message::client_task client_task = {
                    .judge_id = judge_id,
                    .submit = &submit,
                    .test_check = &submit.test_cases[i],
                    .id = i,
                    .type = submit.test_cases[i].check_type};
                testcase_queue.push(client_task);
            } else {
                message::task_result next_result = task_result;
                next_result.status = status::DEPENDENCY_NOT_SATISFIED;
                next_result.id = i;
                next_result.type = kase.check_type;
                process_nolock(testcase_queue, next_result);
            }
        }
    }

    size_t finished = ++finished_task_cases[judge_id];
    // 如果当前提交的所有测试点都完成测试，则返回评测结果
    if (finished == submit.test_cases.size()) {
        summarize(judge_id);
        return;  // 跳过本次评测过程
    } else if (finished > submit.test_cases.size()) {
        LOG(ERROR) << "Test case exceeded [" << submit.category << "-" << submit.prob_id << "-" << submit.sub_id << "]";
        return;  // 跳过本次评测过程
    }
}

void process(concurrent_queue<message::client_task> &testcase_queue, const message::task_result &task_result) {
    lock_guard<mutex> guard(server_mutex);
    process_nolock(testcase_queue, task_result);
}

static bool verify_submission(concurrent_queue<message::client_task> &testcase_queue, submission &&origin) {
    LOG(INFO) << "Judging submission [" << origin.category << "-" << origin.prob_id << "-" << origin.sub_id << "]";

    // 检查 judge_server 获取的 origin 是否包含编译任务，且确保至多一个编译任务
    bool has_compile_case = false;
    for (size_t i = 0; i < origin.test_cases.size(); ++i) {
        auto &test_case = origin.test_cases[i];

        if (test_case.depends_on >= (int)i) {  // 如果每个任务都只依赖前面的任务，那么这个图将是森林，确保不会出现环
            LOG(WARNING) << "Submission from [" << origin.category << "-" << origin.prob_id << "-" << origin.sub_id << "] may contains circular dependency.";
            report_failure(origin);
            return false;
        }

        if (test_case.check_type == message::client_task::COMPILE_TYPE) {
            if (!has_compile_case) {
                has_compile_case = true;
            } else {
                LOG(WARNING) << "Submission from [" << origin.category << "-" << origin.prob_id << "-" << origin.sub_id << "] has multiple compilation subtasks.";
                report_failure(origin);
                return false;
            }
        }
    }

    if (!origin.submission) return false;

    // 检查是否存在可以直接评测的测试点，如果不存在则直接返回
    bool sent_testcase = false;
    for (size_t i = 0; i < origin.test_cases.size(); ++i)
        if (origin.test_cases[i].depends_on < 0) {
            sent_testcase = true;
            break;
        }

    if (!sent_testcase) {
        // 如果不存在评测任务，直接返回
        LOG(WARNING) << "Submission from [" << origin.category << "-" << origin.prob_id << "-" << origin.sub_id << "] does not have entry test task.";
        report_failure(origin);
        return false;
    }

    // 给当前提交分配一个 judge_id 给评测服务端和客户端进行识别
    // global_judge_id++ 就算溢出也无所谓，一般情况下不会同时评测这么多提交
    unsigned judge_id = global_judge_id++;
    submissions[judge_id] = move(origin);
    auto &submit = submissions[judge_id];

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

    // 寻找没有依赖的评测点，并发送评测消息
    for (size_t i = 0; i < submit.test_cases.size(); ++i) {
        if (submit.test_cases[i].depends_on < 0) {  // 不依赖任何任务的任务可以直接开始评测
            judge::message::client_task client_task = {
                .judge_id = judge_id,
                .submit = &submit,
                .test_check = &submit.test_cases[i],
                .id = i,
                .type = submit.test_cases[i].check_type};
            testcase_queue.push(client_task);
        }
    }
    return true;
}

static bool fetch_submission_nolock(concurrent_queue<message::client_task> &testcase_queue) {
    bool success = false;  // 是否成功拉到评测任务
    // 尝试从服务器拉取提交，每次都向所有的评测服务器拉取评测任务
    for (auto &[category, server] : judge_servers) {
        judge::server::submission submission;
        try {
            if (server->fetch_submission(submission)) {
                if (verify_submission(testcase_queue, move(submission)))
                    success = true;
            }
        } catch (exception &ex) {
            LOG(WARNING) << "Fetching from " << category << ex.what() << endl << boost::stacktrace::stacktrace();
        }
    }
    return success;
}

// 限制拉取提交的总数
// TODO: 将拉取了的提交保存到本地持久化以便评测系统挂掉后重启可以 rejudge
// TODO: 主动 fetch 未评测的数据
bool fetch_submission(concurrent_queue<message::client_task> &testcase_queue) {
    lock_guard<mutex> guard(server_mutex);
    return fetch_submission_nolock(testcase_queue);
}

}  // namespace judge::server
