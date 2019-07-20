#include "client/client.hpp"
#include <glog/logging.h>
#include <unistd.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fstream>
#include "client/judge.hpp"
#include "common/interprocess.hpp"
#include "common/io_utils.hpp"
#include "common/stl_utils.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "server/server.hpp"

namespace judge::client {
using namespace std;

thread start_client(const cpu_set_t &set, const string &execcpuset, concurrent_queue<message::client_task> &testcase_queue) {
    thread thd([execcpuset, &testcase_queue] {
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
static message::task_result judge(message::client_task &client_task, server::submission &submit, server::test_check &task, const string &execcpuset) {
    namespace ip = boost::interprocess;
    string uuid = boost::lexical_cast<string>(boost::uuids::random_generator()());
    filesystem::path cachedir = CACHE_DIR / submit.category / submit.prob_id;               // 题目的缓存文件夹
    filesystem::path workdir = RUN_DIR / submit.category / submit.prob_id / submit.sub_id;  // 本提交的工作文件夹
    filesystem::path rundir = workdir / ("run-" + uuid);                                    // 本测试点的运行文件夹

    message::task_result result{client_task.judge_id, client_task.id, client_task.type};
    result.run_dir = rundir;

    server::judge_server &server = server::get_judge_server_by_category(submit.category);
    auto &exec_mgr = server.get_executable_manager();

    // <check-script> <std.in> <std.out> <timelimit> <chrootdir> <workdir> <run-id> <run-script> <compare-script>
    auto check_script = exec_mgr.get_check_script(task.check_script);
    check_script->fetch(execcpuset, CHROOT_DIR);

    auto run_script = exec_mgr.get_run_script(task.run_script);
    run_script->fetch(execcpuset, CHROOT_DIR);

    unique_ptr<judge::server::program> exec_compare_script = task.compare_script.empty() ? nullptr : exec_mgr.get_compare_script(task.compare_script);
    auto &compare_script = task.compare_script.empty() ? submit.compare : exec_compare_script;
    if (!compare_script) { // 没有 submit.compare，但却存在需求 submit.compare 的 task 时报错
        result.status = status::SYSTEM_ERROR;
        result.error_log = "no compare script";
        return result;
    }

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
                // random_generator.sh <random_gen> <std_program> <timelimit> <chrootdir> <datadir> <run>
                int ret = call_process(EXEC_DIR / "random_generator.sh", "-n", execcpuset, submit.random->get_run_path(randomdir), submit.standard->get_run_path(standarddir), task.time_limit, CHROOT_DIR, datadir, run_script->get_run_path());
                switch (ret) {
                    case E_SUCCESS: {
                        // 随机数据已经准备好
                    } break;
                    case E_RANDOM_GEN_ERROR: {
                        // 随机数据生成器出错，返回 RANDOM_GEN_ERROR 并携带错误信息
                        result.status = status::RANDOM_GEN_ERROR;
                        result.error_log = read_file_content(datadir / "system.out", "No information");
                        return result;
                    } break;
                    default: {  // INTERNAL_ERROR
                        // 随机数据生成器出错，返回 SYSTEM_ERROR 并携带错误信息
                        result.status = status::SYSTEM_ERROR;
                        result.error_log = read_file_content(datadir / "system.out", "No information");
                        return result;
                    } break;
                }
            } else {
                int number = random(0, MAX_RANDOM_DATA_NUM - 1);
                task.testcase_id = number;  // 标记当前测试点使用了哪个随机测试
                datadir = random_data_dir / to_string(number);
            }
        }
    } else {
        // 下载标准测试数据
        filesystem::path standard_data_dir = cachedir / "standard_data";

        if (father && !father->is_random && father->testcase_id >= 0) {  // 如果父测试也是标准测试，那么使用同一个测试数据组
            int number = father->testcase_id;
            if (number < 0) LOG(FATAL) << "Unknown test case";
            datadir = standard_data_dir / to_string(task.testcase_id);
            // FIXME: 我们假定父测试存在的时候数据必定存在，如果数据被清除掉可能会导致问题
        } else {
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
    }

    // result 记录的 data_dir 保存实际使用的测试数据
    result.data_dir = datadir;

    if (USE_DATA_DIR) {  // 如果要拷贝测试数据，我们随机 UUID 并创建文件夹拷贝数据
        filesystem::path newdir = DATA_DIR / uuid;
        filesystem::copy(datadir, newdir, filesystem::copy_options::recursive);
        datadir = newdir;
    }

    map<string, string> env;
    if (task.file_limit > 0) env["FILELIMIT"] = to_string(task.file_limit);
    if (task.memory_limit > 0) env["MEMLIMIT"] = to_string(task.memory_limit);
    if (task.proc_limit > 0) env["PROCLIMIT"] = to_string(task.proc_limit);

    // 调用 check script 来执行真正的评测，这里会调用 run script 运行选手程序，调用 compare script 运行比较器，并返回评测结果
    int ret = call_process_env(env,
                               check_script->get_run_path() / "run",
                               "-n", execcpuset,
                               datadir, task.time_limit, CHROOT_DIR, workdir,
                               uuid,
                               run_script->get_run_path(),
                               compare_script->get_run_path(cachedir / "compare"),
                               boost::algorithm::join(submit.submission->source_files | boost::adaptors::transformed([](auto &a) { return a->name; }), ":"),
                               boost::algorithm::join(submit.submission->assist_files | boost::adaptors::transformed([](auto &a) { return a->name; }), ":"));
    result.error_log = read_file_content(rundir / "system.out", "No detailed information");
    switch (ret) {
        case E_INTERNAL_ERROR:
            result.status = status::SYSTEM_ERROR;
            break;
        case E_ACCEPTED:
            result.status = status::ACCEPTED;
            result.score = 1;
            break;
        case E_WRONG_ANSWER:
            result.status = status::WRONG_ANSWER;
            break;
        case E_PARTIAL_CORRECT:
            result.status = status::PARTIAL_CORRECT;
            {
                // 比较器会将评分（0~1 的分数）存到 score.txt 中。
                // 不会出现文件不存在的情况，否则 check script 将返回 COMPARE_ERROR
                // feedback 文件夹的内容参考 check script
                ifstream fin(rundir / "feedback" / "score.txt");
                int numerator, denominator;
                fin >> numerator >> denominator;  // 文件中第一个数字是分子，第二个数字是分母
                result.score = {numerator, denominator};
            }
            break;
        case E_PRESENTATION_ERROR:
            result.status = status::PRESENTATION_ERROR;
            break;
        case E_COMPARE_ERROR:
            result.status = status::COMPARE_ERROR;
            break;
        case E_RUNTIME_ERROR:
            result.status = status::RUNTIME_ERROR;
            break;
        case E_FLOATING_POINT:
            result.status = status::FLOATING_POINT_ERROR;
            break;
        case E_SEG_FAULT:
            result.status = status::SEGMENTATION_FAULT;
            break;
        case E_OUTPUT_LIMIT:
            result.status = status::OUTPUT_LIMIT_EXCEEDED;
            break;
        case E_TIME_LIMIT:
            result.status = status::TIME_LIMIT_EXCEEDED;
            break;
        case E_MEM_LIMIT:
            result.status = status::MEMORY_LIMIT_EXCEEDED;
            break;
        default:
            result.status = status::SYSTEM_ERROR;
            break;
    }

    if (USE_DATA_DIR) {  // 评测结束后删除拷贝的评测数据
        filesystem::remove(datadir);
    }

    auto metadata = read_runguard_result(rundir / "program.meta");
    result.run_time = metadata.wall_time;  // TODO: 支持题目选择 cpu_time 或者 wall_time 进行时间
    result.memory_used = metadata.memory / 1024;
    return result;
}

static void compile(server::program *program, const filesystem::path &workdir, const string &execcpuset, message::task_result &task_result) {
    try {
        // 将程序存放在 workdir 下，program->fetch 会自行组织 workdir 内的文件存储结构
        // 并编译程序，编译需要的运行环境就是全局的 CHROOT_DIR，这样可以获得比较完整的环境
        program->fetch(execcpuset, workdir, CHROOT_DIR);
        task_result.status = status::ACCEPTED;
    } catch (server::executable_compilation_error &ex) {
        task_result.status = status::EXECUTABLE_COMPILATION_ERROR;
        task_result.error_log = program->get_compilation_log(workdir);
    } catch (server::compilation_error &ex) {
        task_result.status = status::COMPILATION_ERROR;
        task_result.error_log = program->get_compilation_log(workdir);
    } catch (exception &ex) {
        task_result.status = status::SYSTEM_ERROR;
        task_result.error_log = ex.what();
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
static message::task_result compile(message::client_task &client_task, server::submission &submit, const string &execcpuset) {
    filesystem::path cachedir = CACHE_DIR / submit.category / submit.prob_id;

    message::task_result result{client_task.judge_id, client_task.id, client_task.type};

    // 编译选手程序，submit.submission 都为非空，否则在 server.cpp 中的 fetch_submission 会阻止该提交的评测
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
        compile(submit.standard.get(), standarddir, execcpuset, result);
        if (result.status == status::COMPILATION_ERROR)
            result.status = status::EXECUTABLE_COMPILATION_ERROR;
        if (result.status != status::ACCEPTED) return result;
    }

    // 编译比较器
    if (submit.compare) {
        filesystem::path comparedir = cachedir / "compare";
        compile(submit.compare.get(), comparedir, execcpuset, result);
        if (result.status == status::COMPILATION_ERROR)
            result.status = status::EXECUTABLE_COMPILATION_ERROR;
        if (result.status != status::ACCEPTED) return result;
    }
    return result;
}

void client(const string &execcpuset, concurrent_queue<message::client_task> &testcase_queue) {
    while (true) {
        // 从队列中读取评测信息
        message::client_task client_task;
        {
            if (!testcase_queue.try_pop(client_task)) {
                if (!server::fetch_submission(testcase_queue))
                    usleep(10 * 1000);  // 10ms，这里必须等待，不可以忙等，否则会挤占 process 函数的执行权
                continue;
            }
        }

        server::submission &submit = *client_task.submit;
        message::task_result result;

        try {
            if (client_task.type == message::client_task::COMPILE_TYPE)
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
