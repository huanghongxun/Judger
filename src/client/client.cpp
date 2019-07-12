#include "client/client.hpp"
#include <glog/logging.h>
#include <unistd.h>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include "client/judge.hpp"
#include "common/interprocess.hpp"
#include "common/io_utils.hpp"
#include "common/messages.hpp"
#include "common/stl_utils.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "server/server.hpp"

namespace judge::client {
using namespace std;

thread start_client(const cpu_set_t &set, const string &execcpuset, message::queue &testcase_queue) {
    thread thd([&] {
        client(execcpuset, testcase_queue);
    });

    // 设置当前线程（客户端线程）的 CPU 亲和性，要求操作系统将 thd 线程放在指定的 cpuset 上运行
    int ret = pthread_setaffinity_np(thd.native_handle(), sizeof(cpu_set_t), &set);
    if (ret < 0) throw std::system_error();

    return thd;
}

/**
 * @brief 执行程序评测任务
 * @param client_task 当前评测任务信息
 * @param submit 当前评测任务归属的选手提交信息
 * @param task 当前评测任务数据点的信息
 * @param execcpuset 当前评测任务能允许运行在那些 cpu 核心上
 * @param result_queue 评测结果队列
 */
static judge::message::task_result judge(judge::message::client_task &client_task, judge::server::submission &submit, judge::server::test_check &task, const string &execcpuset) {
    namespace ip = boost::interprocess;
    filesystem::path cachedir = CACHE_DIR / submit.category / submit.prob_id;
    filesystem::path workdir = RUN_DIR / submit.category / submit.prob_id / submit.sub_id;
    filesystem::path rundir = workdir / ("run-" + to_string(getpid()));

    judge::message::task_result result{client_task.judge_id, client_task.id, client_task.type};
    result.run_dir = rundir;

    judge::server::judge_server &server = judge::server::get_judge_server_by_category(submit.category);
    auto &exec_mgr = server.get_executable_manager();

    // <check-script> <std.in> <std.out> <timelimit> <chrootdir> <workdir> <run-id> <run-script> <compare-script>
    auto check_script = exec_mgr.get_check_script(task.check_script);
    check_script->fetch(execcpuset, CHROOT_DIR);

    auto run_script = exec_mgr.get_run_script(submit.language);
    run_script->fetch(execcpuset, CHROOT_DIR);

    auto &compare_script = submit.compare;
    compare_script->fetch(execcpuset, cachedir / "compare", CHROOT_DIR);

    filesystem::path datadir;

    int depends_on = task.depends_on;
    server::test_check *father = nullptr;
    if (depends_on >= 0) father = &submit.test_cases[depends_on];

    // 获得输入输出数据
    // TODO: 根据 submission 的 last_update 更新数据
    if (task.is_random) {
        // 生成随机测试数据
        filesystem::path random_data_dir = cachedir / "random_data";

        if (father && father->is_random) {  // 如果父测试也是随机测试，那么使用同一个测试数据组
            int number = father->testcase_id;
            if (number < 0) LOG(FATAL) << "Unknown test case";
            datadir = random_data_dir / to_string(number);
        } else {
            // 随机目录的写入必须加锁
            ip::file_lock file_lock = lock_directory(random_data_dir);
            ip::scoped_lock scoped_lock(file_lock);
            int number = count_directories_in_directory(random_data_dir);
            if (number < MAX_RANDOM_DATA_NUM) {
                filesystem::create_directories(random_data_dir / to_string(number));

                filesystem::path randomdir = cachedir / "random";
                filesystem::path standarddir = cachedir / "standard";
                datadir = random_data_dir / to_string(number);
                task.testcase_id = number;  // 标记当前测试点使用了哪个随机测试
                // random_generator.sh <random generator> <standard program> <timelimit> <memlimit> <chrootdir> <workdir> <run>
                int ret = call_process(EXEC_DIR / "random_generator.sh", "-n", execcpuset, submit.random->get_run_path(randomdir), submit.standard->get_run_path(standarddir), submit.time_limit, submit.memory_limit, CHROOT_DIR, datadir, run_script->get_run_path());
                switch (ret) {
                    case E_SUCCESS: {
                        // 随机数据已经准备好
                    } break;
                    case E_RANDOM_GEN_ERROR: {
                        // 随机数据生成器出错，返回 RANDOM_GEN_ERROR 并携带错误信息
                        result.status = status::RANDOM_GEN_ERROR;
                        result.error_log = read_file_content(datadir / "random.out", "No information");
                        return result;
                    } break;
                    default: {  // INTERNAL_ERROR
                        // 随机数据生成器出错，返回 SYSTEM_ERROR 并携带错误信息
                        result.status = status::SYSTEM_ERROR;
                        result.error_log = read_file_content(datadir / "random.out", "No information");
                        return result;
                    } break;
                }
            }
        }
        else {
            int number = random(0, MAX_RANDOM_DATA_NUM - 1);
            task.testcase_id = number;  // 标记当前测试点使用了哪个随机测试
            datadir = random_data_dir / to_string(number);
        }
    } else {
        // 下载标准测试数据
        filesystem::path standard_data_dir = cachedir / "standard_data";
        datadir = standard_data_dir / to_string(task.testcase_id);
        ip::file_lock file_lock = lock_directory(standard_data_dir);
        ip::scoped_lock scoped_lock(file_lock);
        auto &test_data = submit.test_data[task.testcase_id];
        if (!filesystem::exists(datadir)) {
            filesystem::create_directories(datadir / "input");
            filesystem::create_directories(datadir / "output");
            for (auto &asset : test_data.inputs)
                asset->fetch(datadir / "input");
            for (auto &asset : test_data.outputs)
                asset->fetch(datadir / "output");
        }
    }

    result.data_dir = datadir;
    int ret = call_process(check_script->get_run_path(), "-n", execcpuset, datadir, submit.time_limit, submit.memory_limit, CHROOT_DIR, workdir, getpid(), run_script->get_run_path(), compare_script->get_run_path(cachedir / "compare"));
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
    auto metadata = read_runguard_result(rundir / "program.meta");
    result.run_time = metadata.wall_time;
    result.memory_used = metadata.memory / 1024;
    return result;
}

static void compile(judge::server::program *program, const filesystem::path &workdir, const string &execcpuset, judge::message::task_result &task_result) {
    try {
        // 将程序存放在 workdir 下，program->fetch 会自行组织 workdir 内的文件存储结构
        // 并编译程序，编译需要的运行环境就是全局的 CHROOT_DIR，这样可以获得比较完整的环境
        program->fetch(execcpuset, workdir, CHROOT_DIR);
        task_result.status = judge::status::ACCEPTED;
    } catch (judge::server::executable_compilation_error &ex) {
        task_result.status = judge::status::EXECUTABLE_COMPILATION_ERROR;
    } catch (judge::server::compilation_error &ex) {
        task_result.status = judge::status::COMPILATION_ERROR;
    } catch (exception &ex) {
        task_result.status = judge::status::SYSTEM_ERROR;
    }
    task_result.error_log = program->get_compilation_log(workdir);
}

/**
 * @brief 执行程序编译任务
 * @param client_task 当前评测任务信息
 * @param submit 当前评测任务归属的选手提交信息
 * @param task 当前评测任务的编译信息
 * @param execcpuset 当前评测任务能允许运行在那些 cpu 核心上
 * @param result_queue 评测结果队列
 */
static judge::message::task_result compile(judge::message::client_task &client_task, judge::server::submission &submit, const string &execcpuset) {
    filesystem::path cachedir = CACHE_DIR / submit.category / submit.prob_id;

    judge::message::task_result result{client_task.judge_id, client_task.id, client_task.type};

    // 编译选手程序，submit.submission 都为非空
    if (submit.submission) {
        filesystem::path workdir = RUN_DIR / submit.category / submit.prob_id / submit.sub_id;
        compile(submit.submission.get(), workdir, execcpuset, result);
        auto metadata = read_runguard_result(workdir / "compile" / "compile.meta");
        result.run_time = metadata.wall_time;
        result.memory_used = metadata.memory / 1024;
        if (result.status != status::ACCEPTED) return result;
    }

    // 编译随机数据生成器
    if (submit.random) {
        filesystem::path randomdir = cachedir / "random";
        compile(submit.random.get(), randomdir, execcpuset, result);
        if (result.status == status::COMPILATION_ERROR)
            result.status = status::EXECUTABLE_COMPILATION_ERROR;
        if (result.status != status::ACCEPTED) return result;
    }

    // 编译标准程序
    if (submit.standard) {
        filesystem::path standarddir = cachedir / "standard";
        compile(submit.random.get(), standarddir, execcpuset, result);
        if (result.status == status::COMPILATION_ERROR)
            result.status = status::EXECUTABLE_COMPILATION_ERROR;
        if (result.status != status::ACCEPTED) return result;
    }
    return result;
}

void client(const string &execcpuset, message::queue &testcase_queue) {
    while (true) {
        // 从队列中读取评测信息
        message::client_task client_task;
        {
            auto envelope = testcase_queue.recv_from_pod(client_task, message::queue::NO_WAIT);
            if (!envelope.success) {
                if (!server::fetch_submission(testcase_queue))
                    usleep(10 * 1000);  // 10ms，这里必须等待，不可以忙等，否则会挤占 process 函数的执行权
                continue;
            }
        }

        judge::server::submission &submit = *client_task.submit;
        judge::message::task_result result;

        try {
            if (client_task.test_check == nullptr)
                result = compile(client_task, submit, execcpuset);
            else
                result = judge(client_task, submit, *client_task.test_check, execcpuset);
        } catch (exception &ex) {
            result = {client_task.judge_id, client_task.id, client_task.type};
            result.status = status::SYSTEM_ERROR;
            result.error_log = ex.what();
        }

        server::process(testcase_queue, result);
    }
}

}  // namespace judge::client
