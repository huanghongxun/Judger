#include "client/client.hpp"
#include <unistd.h>
#include <thread>
#include "config.hpp"
#include "server/server.hpp"
#include "stl_utils.hpp"

namespace judge::client {
using namespace std;

void start_client(const cpu_set_t &set, const string &execcpuset, message::queue &testcase_queue, message::queue &result_queue) {
    thread thd([&] {
        client(execcpuset, testcase_queue, result_queue);
    });

    thd.detach();

    int ret = pthread_setaffinity_np(thd.native_handle(), sizeof(cpu_set_t), &set);
    if (ret < 0) throw std::system_error();
}

static void judge(judge::message::client_task &client_task, judge::server::submission &submit, const judge::message::test_case_task &task, const string &execcpuset, message::queue &result_queue) {

}

static void compile(judge::message::client_task &client_task, judge::server::submission &submit, const judge::message::compilation_task &task, const string &execcpuset, message::queue &result_queue) {
    auto program = reinterpret_cast<judge::server::program *>(task.program);
    filesystem::path workdir;  // TODO: workdir for cached and not cached.

    try {
        program->fetch(workdir);
        program->compile(execcpuset, workdir, CHROOT_DIR);
        judge::message::task_result result = {
            .judge_id = client_task.judge_id,
            .id = client_task.id,
            .type = client_task.type,
            .status = judge::status::COMPILATION_SUCCEEDED};
        result_queue.send_as_pod(result);
    } catch (exception &ex) {
        judge::message::task_result result = {
            .judge_id = client_task.judge_id,
            .id = client_task.id,
            .type = client_task.type,
            .status = judge::status::COMPILATION_ERROR};
        result_queue.send_as_pod(result);
    }
}

void client(const string &execcpuset, message::queue &testcase_queue, message::queue &result_queue) {
    while (true) {
        {
            judge::message::client_task client_task;
            auto envelope = testcase_queue.recv_from_pod(client_task);
            if (envelope.success) {
                judge::server::submission &submit = judge::server::get_submission_by_judge_id(client_task.judge_id);
                visit(overloaded{
                          [&](const judge::message::test_case_task &task) {
                              judge(client_task, submit, task, execcpuset, result_queue);
                          },
                          [&](const judge::message::compilation_task &task) {
                              compile(client_task, submit, task, execcpuset, result_queue);
                          }},
                      client_task.task);
            }
        }
        usleep(10 * 1000);  // sleep for 10ms.
    }
}

}  // namespace judge::client
