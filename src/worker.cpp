#include "worker.hpp"
#include <Python.h>
#include <glog/logging.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/stacktrace.hpp>
#include <functional>
#include "common/defer.hpp"
#include "common/exceptions.hpp"

namespace judge {
using namespace std;
using namespace judge::server;

// 停止 worker 的标记
static volatile bool stop = false;

void stop_workers() {
    stop = true;
}

static mutex server_mutex;
static unsigned global_judge_id = 0;
// 键为一个唯一的 judge_id
static map<unsigned, unique_ptr<submission>> submissions;

static map<string, unique_ptr<judge_server>> judge_servers;

void register_judge_server(unique_ptr<judge_server> &&judge_server) {
    string category = judge_server->category();
    judge_servers.insert({category, move(judge_server)});
}

static vector<unique_ptr<monitor>> monitors;

void register_monitor(unique_ptr<monitor> &&monitor) {
    monitors.push_back(move(monitor));
}

static void call_monitor(int worker_id, function<void(monitor &)> callback) {
    try {
        for (auto &monitor : monitors) callback(*monitor);
    } catch (std::exception &ex) {
        LOG(ERROR) << "Worker " << worker_id << " has crashed when reporting monitoring information, " << ex.what();
    }
}

void report_error(const std::string &message) {
    call_monitor(-1, [&](monitor &m) { m.report_error(message); });
}

vector<unique_ptr<submission>> finished_submissions;

/**
 * @brief 提交结束，要求释放 submission 所占内存
 */
static void finish_submission(submission &submit) {
    scoped_lock guard(server_mutex);
    unsigned judge_id = submit.judge_id;
    finished_submissions.push_back(move(submissions.at(judge_id)));
    submissions.erase(judge_id);
}

static map<string, unique_ptr<judger>> judgers;

void register_judger(unique_ptr<judger> &&judger) {
    string type = judger->type();
    judger->on_judge_finished(finish_submission);
    judgers.insert({type, move(judger)});
}

/**
 * @brief 根据 type 来获取评测器
 * 这个函数可以并发调用。你必须确保 register_judger 在 worker 启动之前就被注册完成
 * @return type 对应的评测器
 */
static judger &get_judger_by_type(const string &type) {
    // 由于 judgers 只会在 server 启动时注册，因此之后都不会修改
    // 因此不需要担心并发问题
    return *judgers.at(type).get();
}

static void report_failure(unique_ptr<submission> &submit) {
    auto &judge_server = judge_servers.at(submit->category);
    judge_server->summarize_invalid(*submit.get());
}

static bool fetch_submission_nolock(int worker_id, concurrent_queue<message::client_task> &task_queue) {
    bool success = false;  // 是否成功拉到评测任务
    // 尝试从服务器拉取提交，每次都向所有的评测服务器拉取评测任务
    for (auto &[category, server] : judge_servers) {
        unique_ptr<judge::submission> submission;
        try {
            if (server->fetch_submission(submission)) {
                submission->judge_server = server.get();
                if (!judgers.count(submission->sub_type))
                    throw runtime_error("Unrecognized submission type " + submission->sub_type);
                if (judgers[submission->sub_type]->verify(*submission)) {
                    unsigned judge_id = global_judge_id++;
                    submission->judge_id = judge_id;
                    call_monitor(worker_id, [&](monitor &m) { m.start_submission(*submission); });
                    submissions[judge_id] = move(submission);
                    judgers[submissions[judge_id]->sub_type]->distribute(task_queue, *submissions[judge_id]);
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
static bool fetch_submission(int worker_id, concurrent_queue<message::client_task> &task_queue) {
    scoped_lock guard(server_mutex);
    return fetch_submission_nolock(worker_id, task_queue);
}

/**
 * @brief 评测客户端程序函数
 * 评测客户端负责从消息队列中获取评测服务端要求评测的数据点，
 * 数据点信息包括时间限制、测试数据、选手代码等信息。
 * @param core_id 当前 worker 占有的 CPU id
 * @param task_queue 评测服务端发送评测信息的队列
 * 
 * 选手代码、测试数据、随机数据生成器、标准程序、SPJ 等资源的
 * 下载均由客户端完成。服务端只完成提交的拉取和数据点的分发。
 * 
 * 文件组织结构：
 * 对于需要进行缓存的文件：
 *     CACHE_DIR
 */
static void worker_loop(size_t core_id, concurrent_queue<message::client_task> &task_queue, concurrent_queue<message::core_request> &core_queue) {
    call_monitor(core_id, [&](monitor &m) { m.worker_state_changed(core_id, worker_state::START, ""); });

    while (true) {
        {
            message::core_request core_request;
            if (core_queue.try_pop(core_request)) {
                {
                    scoped_lock lock(*core_request.write_lock);
                    core_request.core_ids->push_back(core_id);
                    core_request.lock->count_down();
                }
                {
                    unique_lock lock(*core_request.mut);
                    core_request.cv->wait(lock);
                }
            }

            // 从队列中读取评测信息
            message::client_task client_task;
            {
                if (!task_queue.try_pop(client_task)) {
                    if (stop) {
                        // 如果需要停止 worker，在评测队列为空时自然退出 worker。
                        // 因为 stop 导致不再获取提交时，不会产生新的评测任务。
                        // 可能存在极限情况：try_pop 之后另一个 worker 推送了
                        // 新评测任务，此时另一个 worker 来完成提交的评测。
                        break;
                    }

                    if (!fetch_submission(core_id, task_queue))
                        usleep(10 * 1000);  // 10ms，这里必须等待，不可以忙等，否则会挤占返回评测结果的执行权
                    continue;
                }
            }

            call_monitor(core_id, [&](monitor &m) { m.start_judge_task(core_id, client_task); });
            defer {
                // 使用 defer 是希望即使评测崩溃也可以发送 end_judge_task 避免监控爆炸
                call_monitor(core_id, [&](monitor &m) { m.end_judge_task(core_id, client_task); });
            };
            call_monitor(core_id, [&](monitor &m) { m.worker_state_changed(core_id, worker_state::JUDGING, ""); });

            {
                judger &j = get_judger_by_type(client_task.submit->sub_type);
                vector<size_t> cpus = {core_id};
                if (client_task.cores > 1 /* TODO: check if cores <= workers */) {
                    message::core_request request;
                    boost::latch latch(client_task.cores - 1);
                    std::mutex write_lock, mut;
                    std::condition_variable cv;
                    request.core_ids = &cpus;
                    request.lock = &latch;
                    request.write_lock = &write_lock;
                    request.cv = &cv;
                    request.mut = &mut;

                    for (size_t i = 1; i < client_task.cores; ++i)
                        core_queue.push(request);
                    latch.wait();
                }
                vector<string> execcpuset;
                for (size_t i : cpus) execcpuset.push_back(to_string(i));
                j.judge(client_task, task_queue, boost::algorithm::join(execcpuset, ","));
            }

            call_monitor(core_id, [&](monitor &m) { m.worker_state_changed(core_id, worker_state::IDLE, ""); });
        }

        scoped_lock guard(server_mutex);
        for (auto &submit : finished_submissions)
            call_monitor(core_id, [&](monitor &m) { m.end_submission(*submit); });
        finished_submissions.clear();
    }

    call_monitor(core_id, [&](monitor &m) { m.worker_state_changed(core_id, worker_state::STOPPED, ""); });
}

thread start_worker(size_t core_id, concurrent_queue<message::client_task> &task_queue, concurrent_queue<message::core_request> &core_queue) {
    thread thd([core_id, &task_queue, &core_queue] {
        worker_loop(core_id, task_queue, core_queue);
    });

    // 设置当前线程（客户端线程）的 CPU 亲和性，要求操作系统将 thd 线程放在指定的 cpuset 上运行
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core_id, &set);
    int ret = pthread_setaffinity_np(thd.native_handle(), sizeof(cpu_set_t), &set);
    if (ret < 0) throw std::system_error();

    return thd;
}

}  // namespace judge
