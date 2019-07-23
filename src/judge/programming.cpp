#include "judge/programming.hpp"
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
#include "common/interprocess.hpp"
#include "common/io_utils.hpp"
#include "common/stl_utils.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "runguard.hpp"
#include "worker.hpp"
#include "server/judge_server.hpp"

namespace judge {

test_case_data::test_case_data() {}

test_case_data::test_case_data(test_case_data &&other)
    : inputs(move(other.inputs)), outputs(move(other.outputs)) {}

judge_task_result::judge_task_result() : score(0) {}
judge_task_result::judge_task_result(size_t id, uint8_t type)
    : id(id), type(type), score(0), run_time(0), memory_used(0) {}

/**
 * @brief 执行程序评测任务
 * @param client_task 当前评测任务信息
 * @param submit 当前评测任务归属的选手提交信息
 * @param task 当前评测任务数据点的信息
 * @param execcpuset 当前评测任务能允许运行在那些 cpu 核心上
 * @param result_queue 评测结果队列
 */
static judge_task_result judge_impl(const message::client_task &client_task, programming_submission &submit, judge_task &task, const string &execcpuset) {
    namespace ip = boost::interprocess;
    string uuid = boost::lexical_cast<string>(boost::uuids::random_generator()());
    filesystem::path cachedir = CACHE_DIR / submit.category / submit.prob_id;               // 题目的缓存文件夹
    filesystem::path workdir = RUN_DIR / submit.category / submit.prob_id / submit.sub_id;  // 本提交的工作文件夹
    filesystem::path rundir = workdir / ("run-" + uuid);                                    // 本测试点的运行文件夹

    judge_task_result result{client_task.id, client_task.type};
    result.run_dir = rundir;

    server::judge_server &server = get_judge_server_by_category(submit.category);
    auto &exec_mgr = server.get_executable_manager();

    // <check-script> <std.in> <std.out> <timelimit> <chrootdir> <workdir> <run-id> <run-script> <compare-script>
    auto check_script = exec_mgr.get_check_script(task.check_script);
    check_script->fetch(execcpuset, CHROOT_DIR);

    auto run_script = exec_mgr.get_run_script(task.run_script);
    run_script->fetch(execcpuset, CHROOT_DIR);

    unique_ptr<judge::program> exec_compare_script = task.compare_script.empty() ? nullptr : exec_mgr.get_compare_script(task.compare_script);
    auto &compare_script = task.compare_script.empty() ? submit.compare : exec_compare_script;
    if (!compare_script) {  // 没有 submit.compare，但却存在需求 submit.compare 的 task 时报错
        result.status = status::SYSTEM_ERROR;
        result.error_log = "no compare script";
        return result;
    }

    compare_script->fetch(execcpuset, cachedir / "compare", CHROOT_DIR);

    filesystem::path datadir;

    int depends_on = task.depends_on;
    judge_task *father = nullptr;
    if (depends_on >= 0) father = &submit.judge_tasks[depends_on];

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

    auto metadata = client::read_runguard_result(rundir / "program.meta");
    result.run_time = metadata.wall_time;  // TODO: 支持题目选择 cpu_time 或者 wall_time 进行时间
    result.memory_used = metadata.memory / 1024;
    return result;
}

static void compile(judge::program *program, const filesystem::path &workdir, const string &execcpuset, judge_task_result &task_result) {
    try {
        // 将程序存放在 workdir 下，program->fetch 会自行组织 workdir 内的文件存储结构
        // 并编译程序，编译需要的运行环境就是全局的 CHROOT_DIR，这样可以获得比较完整的环境
        program->fetch(execcpuset, workdir, CHROOT_DIR);
        task_result.status = status::ACCEPTED;
    } catch (executable_compilation_error &ex) {
        task_result.status = status::EXECUTABLE_COMPILATION_ERROR;
        task_result.error_log = string(ex.what()) + "\n" + program->get_compilation_log(workdir);
    } catch (compilation_error &ex) {
        task_result.status = status::COMPILATION_ERROR;
        task_result.error_log = string(ex.what()) + "\n" + program->get_compilation_log(workdir);
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
static judge_task_result compile(const message::client_task &client_task, programming_submission &submit, judge_task &task, const string &execcpuset) {
    filesystem::path cachedir = CACHE_DIR / submit.category / submit.prob_id;

    judge_task_result result{client_task.id, client_task.type};

    // 编译选手程序，submit.submission 都为非空，否则在 server.cpp 中的 fetch_submission 会阻止该提交的评测
    if (submit.submission) {
        filesystem::path workdir = RUN_DIR / submit.category / submit.prob_id / submit.sub_id;
        compile(submit.submission.get(), workdir, execcpuset, result);
        auto metadata = client::read_runguard_result(workdir / "compile" / "compile.meta");
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

string programming_judger::type() {
    return "programming";
}

bool programming_judger::verify(unique_ptr<submission> &submit) {
    auto sub = dynamic_cast<programming_submission *>(submit.get());
    if (!sub) return false;
    LOG(INFO) << "Judging submission [" << sub->category << "-" << sub->prob_id << "-" << sub->sub_id << "]";

    // 检查 judge_server 获取的 sub 是否包含编译任务，且确保至多一个编译任务
    bool has_compile_case = false;
    for (size_t i = 0; i < sub->judge_tasks.size(); ++i) {
        auto &judge_task = sub->judge_tasks[i];

        if (judge_task.depends_on >= (int)i) {  // 如果每个任务都只依赖前面的任务，那么这个图将是森林，确保不会出现环
            LOG(WARNING) << "Submission from [" << sub->category << "-" << sub->prob_id << "-" << sub->sub_id << "] may contains circular dependency.";
            return false;
        }

        if (judge_task.check_script == "compile") {
            if (!has_compile_case) {
                has_compile_case = true;
            } else {
                LOG(WARNING) << "Submission from [" << sub->category << "-" << sub->prob_id << "-" << sub->sub_id << "] has multiple compilation subtasks.";
                return false;
            }
        }
    }

    if (!sub->submission) return false;

    // 检查是否存在可以直接评测的测试点，如果不存在则直接返回
    bool sent_testcase = false;
    for (size_t i = 0; i < sub->judge_tasks.size(); ++i)
        if (sub->judge_tasks[i].depends_on < 0) {
            sent_testcase = true;
            break;
        }

    if (!sent_testcase) {
        // 如果不存在评测任务，直接返回
        LOG(WARNING) << "Submission from [" << sub->category << "-" << sub->prob_id << "-" << sub->sub_id << "] does not have entry test task.";
        return false;
    }

    return true;
}

bool programming_judger::distribute(concurrent_queue<message::client_task> &task_queue, unique_ptr<submission> &submit) {
    auto sub = dynamic_cast<programming_submission *>(submit.get());
    // 初始化当前提交的所有评测任务状态为 PENDING
    sub->results.resize(sub->judge_tasks.size());
    for (size_t i = 0; i < sub->results.size(); ++i) {
        sub->results[i].status = judge::status::PENDING;
        sub->results[i].id = i;
        sub->results[i].type = sub->judge_tasks[i].check_type;
    }

    // 寻找没有依赖的评测点，并发送评测消息
    for (size_t i = 0; i < sub->judge_tasks.size(); ++i) {
        if (sub->judge_tasks[i].depends_on < 0) {  // 不依赖任何任务的任务可以直接开始评测
            judge::message::client_task client_task = {
                .submit = submit.get(),
                .id = i,
                .type = sub->judge_tasks[i].check_type};
            task_queue.push(client_task);
        }
    }
    return true;
}

static void summarize(programming_submission &submit) {
    auto &judge_server = get_judge_server_by_category(submit.category);
    judge_server.summarize(submit);

    filesystem::path workdir = RUN_DIR / submit.category / submit.prob_id / submit.sub_id;
    try {
        filesystem::remove_all(workdir);
    } catch (exception &e) {
        LOG(ERROR) << "Unable to delete directory " << workdir << ":" << e.what();
    }
}

/**
 * @brief 处理 client 返回的评测结果
 * 
 * @param task_result client 返回的评测结果
 */
void programming_judger::process(concurrent_queue<message::client_task> &testcase_queue, programming_submission &submit, const judge_task_result &result) {
    // 记录测试信息
    submit.results[result.id] = result;

    DLOG(INFO) << "Testcase [" << submit.category << "-" << submit.prob_id << "-" << submit.sub_id
               << ", status: " << (int)result.status << ", runtime: " << result.run_time
               << ", memory: " << result.memory_used << ", run_dir: " << result.run_dir
               << ", data_dir: " << result.data_dir << "]";

    for (size_t i = 0; i < submit.judge_tasks.size(); ++i) {
        judge_task &kase = submit.judge_tasks[i];
        // 寻找依赖当前评测点的评测点
        if (kase.depends_on == (int)result.id) {
            bool satisfied;
            switch (kase.depends_cond) {
                case judge_task::depends_condition::ACCEPTED:
                    satisfied = result.status == status::ACCEPTED;
                    break;
                case judge_task::depends_condition::PARTIAL_CORRECT:
                    satisfied = result.status == status::PARTIAL_CORRECT ||
                                result.status == status::ACCEPTED;
                    break;
                case judge_task::depends_condition::NON_TIME_LIMIT:
                    satisfied = result.status != status::SYSTEM_ERROR &&
                                result.status != status::COMPARE_ERROR &&
                                result.status != status::COMPILATION_ERROR &&
                                result.status != status::DEPENDENCY_NOT_SATISFIED &&
                                result.status != status::TIME_LIMIT_EXCEEDED &&
                                result.status != status::EXECUTABLE_COMPILATION_ERROR &&
                                result.status != status::OUT_OF_CONTEST_TIME &&
                                result.status != status::RANDOM_GEN_ERROR;
                    break;
            }

            if (satisfied) {
                judge::message::client_task client_task = {
                    .submit = &submit,
                    .id = i,
                    .type = submit.judge_tasks[i].check_type};
                testcase_queue.push(client_task);
            } else {
                judge_task_result next_result = result;
                next_result.status = status::DEPENDENCY_NOT_SATISFIED;
                next_result.id = i;
                next_result.type = kase.check_type;
                process(testcase_queue, submit, next_result);
            }
        }
    }

    size_t finished = ++submit.finished;
    // 如果当前提交的所有测试点都完成测试，则返回评测结果
    if (finished == submit.judge_tasks.size()) {
        summarize(submit);
        fire_judge_finished(submit);
        return;  // 跳过本次评测过程
    } else if (finished > submit.judge_tasks.size()) {
        LOG(ERROR) << "Test case exceeded [" << submit.category << "-" << submit.prob_id << "-" << submit.sub_id << "]";
        return;  // 跳过本次评测过程
    } else {
        // TODO: 发送 incomplete 的评测报告
    }
}

void programming_judger::judge(const message::client_task &client_task, concurrent_queue<message::client_task> &task_queue, const string &execcpuset) {
    auto submit = dynamic_cast<programming_submission *>(client_task.submit);
    judge_task &task = submit->judge_tasks[client_task.id];
    judge_task_result result;

    try {
        if (task.check_script == "compile")
            result = compile(client_task, *submit, task, execcpuset);
        else
            result = judge_impl(client_task, *submit, task, execcpuset);
    } catch (exception &ex) {
        result = {client_task.id, client_task.type};
        result.status = status::SYSTEM_ERROR;
        result.error_log = ex.what();
    }

    lock_guard<mutex> guard(mut);
    process(task_queue, *submit, result);
}

}  // namespace judge
