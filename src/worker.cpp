#include "worker.hpp"
#include <glog/logging.h>
#include <boost/stacktrace.hpp>
#include <boost/exception/diagnostic_information.hpp>

namespace judge {
using namespace std;
using namespace judge::server;

static mutex server_mutex;
static unsigned global_judge_id = 0;
// 键为一个唯一的 judge_id
static map<unsigned, unique_ptr<submission>> submissions;

/**
 * @brief 提交结束，要求释放 submission 所占内存
 */
static void finish_submission(submission &submit) {
    submissions.erase(submit.judge_id);
}

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

static map<string, unique_ptr<judger>> judgers;

void register_judger(unique_ptr<judger> &&judger) {
    string type = judger->type();
    judger->on_judge_finished(finish_submission);
    judgers.insert({type, move(judger)});
}

judger &get_judger_by_type(const string &type) {
    // 由于 judgers 只会在 server 启动时注册，因此之后都不会修改
    // 因此不需要担心并发问题
    return *judgers.at(type).get();
}

static void report_failure(unique_ptr<submission> &submit) {
    auto &judge_server = judge_servers.at(submit->category);
    judge_server->summarize_invalid(*submit.get());
}

static bool fetch_submission_nolock(concurrent_queue<message::client_task> &task_queue) {
    bool success = false;  // 是否成功拉到评测任务
    // 尝试从服务器拉取提交，每次都向所有的评测服务器拉取评测任务
    for (auto &[category, server] : judge_servers) {
        unique_ptr<judge::submission> submission;
        try {
            if (server->fetch_submission(submission)) {
                if (!judgers.count(submission->sub_type))
                    throw runtime_error("Unrecognized submission type " + submission->sub_type);
                if (judgers[submission->sub_type]->verify(submission)) {
                    unsigned judge_id = global_judge_id++;
                    submission->judge_id = judge_id;
                    submissions[judge_id] = move(submission);
                    judgers[submissions[judge_id]->sub_type]->distribute(task_queue, submissions[judge_id]);
                    success = true;
                } else {
                    report_failure(submission);
                }
            }
        } catch (exception &ex) {
            LOG(WARNING) << "Fetching from " << category << ' ' << ex.what() << endl
                         << boost::diagnostic_information(ex);
            success = false;
        }
    }
    return success;
}

/**
 * @brief 向每个评测服务器拉取一个提交
 * 如果遇到评测队列为空的情况，worker 需要调用 fetch_submission 函数来拉取评测
 * （TODO: 测试潜在的多个 worker 同时访问 fetch_submission 导致评测队列过长的问题）。
 * 
 * @param task_queue 评测服务端发送评测信息的队列
 * @return true 如果获取到了提交
 * 限制拉取提交的总数
 */
static bool fetch_submission(concurrent_queue<message::client_task> &task_queue) {
    lock_guard<mutex> guard(server_mutex);
    return fetch_submission_nolock(task_queue);
}

/**
 * @brief 评测客户端程序函数
 * 评测客户端负责从消息队列中获取评测服务端要求评测的数据点，
 * 数据点信息包括时间限制、测试数据、选手代码等信息。
 * @param execcpuset 选手程序运行的 cpuset
 * @param task_queue 评测服务端发送评测信息的队列
 * 
 * 选手代码、测试数据、随机数据生成器、标准程序、SPJ 等资源的
 * 下载均由客户端完成。服务端只完成提交的拉取和数据点的分发。
 * 
 * 文件组织结构：
 * 对于需要进行缓存的文件：
 *     CACHE_DIR
 */
static void worker_loop(int worker_id, const string &execcpuset, concurrent_queue<message::client_task> &task_queue) {
#define REPORT_WORKER_STATE(state) \
    for (auto &[category, server] : judge_servers) server->report_worker_state(worker_id, worker_state::state)

    REPORT_WORKER_STATE(START);

    while (true) {
        try {
            // 从队列中读取评测信息
            message::client_task client_task;
            {
                if (!task_queue.try_pop(client_task)) {
                    if (!fetch_submission(task_queue))
                        usleep(10 * 1000);  // 10ms，这里必须等待，不可以忙等，否则会挤占返回评测结果的执行权
                    continue;
                }
            }

            REPORT_WORKER_STATE(JUDGING);

            judger &j = get_judger_by_type(client_task.submit->sub_type);
            j.judge(client_task, task_queue, execcpuset);

            REPORT_WORKER_STATE(IDLE);
        } catch (exception &ex) {
            LOG(ERROR) << "Worker " << worker_id << " has crashed, " << ex.what();
            REPORT_WORKER_STATE(CRASHED);
        }

        REPORT_WORKER_STATE(STOPPED);
    }
}

thread start_worker(int worker_id, const cpu_set_t &set, const string &execcpuset, concurrent_queue<message::client_task> &task_queue) {
    thread thd([worker_id, execcpuset, &task_queue] {
        worker_loop(worker_id, execcpuset, task_queue);
    });

    // 设置当前线程（客户端线程）的 CPU 亲和性，要求操作系统将 thd 线程放在指定的 cpuset 上运行
    int ret = pthread_setaffinity_np(thd.native_handle(), sizeof(cpu_set_t), &set);
    if (ret < 0) throw std::system_error();

    return thd;
}

}  // namespace judge
