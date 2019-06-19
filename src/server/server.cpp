#include "server/server.hpp"
#include "config.hpp"

namespace judge::server {

static map<string, unique_ptr<judge_server>> judge_servers;

void register_judge_server(unique_ptr<judge_server> &&judge_server) {
    string category = judge_server->category();
    judge_servers.insert({category, move(judge_server)});
}

static unsigned judge_id = 0;
// 键为一个唯一的 judge_id
static map<unsigned, submission> submissions;
static map<unsigned, task_result> task_results;

submission &get_submission_by_judge_id(unsigned judge_id) {
    return submissions.at(judge_id);
}

void server(message::queue &testcase_queue, message::queue &result_queue) {
    while (true) {
        {
            // 从结果队列中获取评测客户端的评测结果，并更新对应 submission 的状态
            judge::message::task_result task_result;
            auto envelope = result_queue.recv_from_pod(task_result);
            if (envelope.success) {
                unsigned judge_id = task_result.judge_id;
                auto &submit = submissions.at(judge_id);
                auto &judge_server = judge_servers.at(submit.category);
                if (task_result.type == judge::message::client_task::COMPILE_TYPE) {
                    if (task_result.status == judge::status::COMPILATION_SUCCEEDED) {
                        // 如果编译成功，则分发评测任务
                    } else {
                        // 否则评测结束，返回评测结果
                        
                    }
                }
            }
        }

        {
            // 尝试从服务器拉取提交，如果拉取到了，那么生成一个编译任务发送给评测客户端

            for (auto &[category, server] : judge_servers) {
                judge::server::submission submission;
                if (server->fetch_submission(submission)) {
                    unsigned this_judgement = judge_id++;
                    submissions.insert({this_judgement, move(submission)});
                    auto &submit = submissions.at(this_judgement);
                    if (submit.submission) {
                        judge::message::compilation_task c = {
                            .program = submit.submission.get(),
                            .cache = false};
                        judge::message::client_task client_task = {
                            .judge_id = this_judgement,
                            .id = judge::message::client_task::COMPILE_ID,
                            .type = judge::message::client_task::COMPILE_TYPE,
                            .task = c};
                        testcase_queue.send_as_pod(client_task);
                    }
                }
            }
        }

        usleep(10 * 1000);  // sleep for 10ms.
    }
}
}  // namespace judge::server