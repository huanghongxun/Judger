#include "client/client.hpp"
#include <unistd.h>
#include <fstream>
#include <thread>
#include "config.hpp"
#include "server/server.hpp"
#include "common/interprocess.hpp"
#include "common/stl_utils.hpp"
#include "common/messages.hpp"
#include "common/utils.hpp"

namespace judge::client {
using namespace std;

void start_client(const cpu_set_t &set, const string &execcpuset, message::queue &testcase_queue, message::queue &result_queue) {
    thread thd([&] {
        client(execcpuset, testcase_queue, result_queue);
    });

    thd.detach();

    // 设置当前线程（客户端线程）的 CPU 亲和性，要求操作系统将 thd 线程放在指定的 cpuset 上运行
    int ret = pthread_setaffinity_np(thd.native_handle(), sizeof(cpu_set_t), &set);
    if (ret < 0) throw std::system_error();
}

/**
 * @brief 执行程序评测任务
 * @param client_task 当前评测任务信息
 * @param submit 当前评测任务归属的选手提交信息
 * @param task 当前评测任务数据点的信息
 * @param execcpuset 当前评测任务能允许运行在那些 cpu 核心上
 * @param result_queue 评测结果队列
 */
static void judge(judge::message::client_task &client_task, judge::server::submission &submit, const judge::server::test_check &task, const string &execcpuset, message::queue &result_queue) {
    namespace ip = boost::interprocess;
    filesystem::path cachedir = CACHE_DIR / submit.category / submit.prob_id;
    filesystem::path workdir = RUN_DIR / submit.category / submit.prob_id / submit.sub_id;
    filesystem::path rundir = workdir / ("run-" + to_string(getpid()));

    judge::message::task_result result = {
        .judge_id = client_task.judge_id,
        .id = client_task.id,
        .type = client_task.type};

    filesystem::path stdin, stdout;

    // 获得输入输出数据
    if (task.is_random) {
        // 生成随机测试数据
        filesystem::path random_data_dir = cachedir / "random_data";
        // 随机目录的写入必须加锁
        ip::file_lock file_lock = lock_directory(random_data_dir);
        ip::scoped_lock scoped_lock(file_lock);
        int number = count_directories_in_directory(random_data_dir);
        if (number < MAX_RANDOM_DATA_NUM) {
            filesystem::create_directories(random_data_dir / to_string(number));

            filesystem::path randomdir = cachedir / "random";
            filesystem::path standarddir = cachedir / "standard";
            stdin = random_data_dir / to_string(number) / "input" / "testdata.in";
            stdout = random_data_dir / to_string(number) / "output" / "testdata.out";
            // random_generator.sh <random generator> <standard program> <stdin> <stdout>
            int ret = call_process(EXEC_DIR / "random_generator.sh", "-n", execcpuset, submit.random->get_run_path(randomdir), submit.standard->get_run_path(standarddir), stdin, stdout);
            switch (ret) {
                case E_SUCCESS: {
                    // 随机数据已经准备好
                } break;
                case E_RANDOM_GEN_ERROR: {
                    // 随机数据生成器出错，返回 RANDOM_GEN_ERROR 并携带错误信息
                    result.status = status::RANDOM_GEN_ERROR;
                    strcpy(result.path_to_error, (random_data_dir / to_string(number) / "random.out").c_str());
                    result_queue.send_as_pod(result);
                    return;
                } break;
                default: {  // INTERNAL_ERROR
                    // 随机数据生成器出错，返回 SYSTEM_ERROR 并携带错误信息
                    result.status = status::SYSTEM_ERROR;
                    strcpy(result.path_to_error, (random_data_dir / to_string(number) / "random.out").c_str());
                    result_queue.send_as_pod(result);
                    return;
                } break;
            }
        } else {
            int number = random(0, MAX_RANDOM_DATA_NUM - 1);
            stdin = random_data_dir / to_string(number) / "input" / "testdata.in";
            stdout = random_data_dir / to_string(number) / "output" / "testdata.out";
        }
    } else {
        // 下载标准测试数据
        filesystem::path standard_data_dir = cachedir / "standard_data";
        filesystem::path data_dir = standard_data_dir / to_string(task.testcase_id);
        ip::file_lock file_lock = lock_directory(standard_data_dir);
        ip::scoped_lock scoped_lock(file_lock);
        auto &test_data = submit.test_data[task.testcase_id];
        if (!filesystem::exists(data_dir)) {
            filesystem::create_directories(data_dir / "input");
            filesystem::create_directories(data_dir / "output");
            for (auto &asset : test_data.inputs)
                asset->fetch(data_dir / "input");
            for (auto &asset : test_data.outputs)
                asset->fetch(data_dir / "output");
        }
        if (!test_data.inputs.empty())  // inputs 的第一个元素是标准输入的测试数据
            stdin = data_dir / "input" / test_data.inputs[0]->name;
        if (!test_data.outputs.empty())  // outputs 的第一个元素是标准输出的测试数据
            stdout = data_dir / "output" / test_data.outputs[0]->name;
    }

    judge::server::judge_server &server = judge::server::get_judge_server_by_category(submit.category);
    auto &exec_mgr = server.get_executable_manager();

    // <check-script> <std.in> <std.out> <timelimit> <chrootdir> <workdir> <run-id> <run-script> <compare-script>
    auto check_script = exec_mgr.get_check_script(task.check_script);
    check_script->fetch(execcpuset, CHROOT_DIR);

    auto run_script = exec_mgr.get_run_script(submit.language);
    run_script->fetch(execcpuset, CHROOT_DIR);

    auto &compare_script = submit.compare;
    compare_script->fetch(execcpuset, cachedir / "compare", CHROOT_DIR);

    int ret = call_process(check_script->get_run_path(), "-n", execcpuset, stdin, stdout, submit.time_limit, CHROOT_DIR, workdir, getpid(), run_script->get_run_path(), compare_script->get_run_path(cachedir / "compare"));
    switch (ret) {
        case E_INTERNAL_ERROR:
            result.status = judge::status::SYSTEM_ERROR;
            break;
        case E_ACCEPTED:
            result.status = judge::status::ACCEPTED;
            break;
        case E_WRONG_ANSWER:
            result.status = judge::status::WRONG_ANSWER;
            break;
        case E_PRESENTATION_ERROR:
            result.status = judge::status::PRESENTATION_ERROR;
            break;
        case E_COMPARE_ERROR:
            result.status = judge::status::COMPARE_ERROR;
            break;
        case E_RUNTIME_ERROR:
            result.status = judge::status::RUNTIME_ERROR;
            break;
        case E_FLOATING_POINT:
            result.status = judge::status::FLOATING_POINT_ERROR;
            break;
        case E_SEG_FAULT:
            result.status = judge::status::SEGMENTATION_FAULT;
            break;
        case E_OUTPUT_LIMIT:
            result.status = judge::status::OUTPUT_LIMIT_EXCEEDED;
            break;
        case E_TIME_LIMIT:
            result.status = judge::status::TIME_LIMIT_EXCEEDED;
            break;
        case E_MEM_LIMIT:
            result.status = judge::status::MEMORY_LIMIT_EXCEEDED;
            break;
        default:
            result.status = judge::status::SYSTEM_ERROR;
            break;
    }
    result_queue.send_as_pod(result);
}

static judge::status compile(judge::server::program *program, const filesystem::path &workdir, const string &execcpuset) {
    try {
        // 将程序存放在 workdir 下，program->fetch 会自行组织 workdir 内的文件存储结构
        // 并编译程序，编译需要的运行环境就是全局的 CHROOT_DIR，这样可以获得比较完整的环境
        program->fetch(execcpuset, workdir, CHROOT_DIR);
        return judge::status::ACCEPTED;
    } catch (judge::server::compilation_error &ex) {
        return judge::status::COMPILATION_ERROR;
    } catch (exception &ex) {
        return judge::status::SYSTEM_ERROR;
    }
}

/**
 * @brief 执行程序编译任务
 * @param client_task 当前评测任务信息
 * @param submit 当前评测任务归属的选手提交信息
 * @param task 当前评测任务的编译信息
 * @param execcpuset 当前评测任务能允许运行在那些 cpu 核心上
 * @param result_queue 评测结果队列
 */
static void compile(judge::message::client_task &client_task, judge::server::submission &submit, const string &execcpuset, message::queue &result_queue) {
    filesystem::path cachedir = CACHE_DIR / submit.category / submit.prob_id;

    judge::message::task_result result = {
        .judge_id = client_task.judge_id,
        .id = client_task.id,
        .type = client_task.type};

        // TODO: add time and memory usage

    // 编译选手程序，一般情况下 submit.submission 都为非空
    if (submit.submission) {
        filesystem::path workdir = RUN_DIR / submit.category / submit.prob_id / submit.sub_id;
        result.status = compile(submit.submission.get(), workdir, execcpuset);
        if (result.status != status::ACCEPTED) goto end;
    }

    // 编译随机数据生成器
    if (submit.random) {
        filesystem::path randomdir = cachedir / "random";
        result.status = compile(submit.random.get(), randomdir, execcpuset);
        if (result.status != status::ACCEPTED) goto end;
    }

    // 编译标准程序
    if (submit.standard) {
        filesystem::path standarddir = cachedir / "standard";
        result.status = compile(submit.random.get(), standarddir, execcpuset);
        if (result.status != status::ACCEPTED) goto end;
    }
end:
    result_queue.send_as_pod(result);
}

void client(const string &execcpuset, message::queue &testcase_queue, message::queue &result_queue) {
    while (true) {
        {
            // 从队列中读取评测信息
            judge::message::client_task client_task;
            auto envelope = testcase_queue.recv_from_pod(client_task);
            if (envelope.success) {
                judge::server::submission &submit = *client_task.submit;
                if (client_task.test_check == nullptr)
                    compile(client_task, submit, execcpuset, result_queue);
                else
                    judge(client_task, submit, *client_task.test_check, execcpuset, result_queue);
            }
        }
        usleep(10 * 1000);  // sleep for 10ms.
    }
}

}  // namespace judge::client
