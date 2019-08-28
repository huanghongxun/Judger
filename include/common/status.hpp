#pragma once

namespace judge {

/**
 * @brief 表示数据点或整个提交的评测结果
 * 
 */
enum class status {
    /**
     * @brief 用户程序正在等待队列中，还未编译
     */
    PENDING = 0,

    /**
     * @brief 用户程序正在运行
     * 编译完成且所有测试数据点还没有完成评测
     */
    RUNNING = 1,

    /**
     * @brief 用户程序本测试点评测通过，或者编译通过（还未评测）
     * 如果没有打开 PE 评测选项且为模糊比较模式时若有空白字符不匹配的情况也会返回 AC。
     */
    ACCEPTED = 2,

    /**
     * @brief 用户程序本测试点没有拿到全部分数。
     */
    PARTIAL_CORRECT = 3,

    /**
     * @brief 用户程序编译错误
     * 用户程序无法通过编译
     */
    COMPILATION_ERROR = 4,

    /**
     * @brief 标准程序、随机数据生成器、脚本编译错误
     * 这个为出题时的错误，但仍然需要返回给服务器
     */
    EXECUTABLE_COMPILATION_ERROR = 5,

    /**
     * @brief 表示当前评测点的依赖评测点没有满足依赖要求，当前测试点失败
     */
    DEPENDENCY_NOT_SATISFIED = 6,

    /**
     * @brief 答案错误
     * 一般是答案错误。
     * 在没开启 PE 评测选项且为精确比较时若有空白字符不匹配的情况也会返回 WA。
     */
    WRONG_ANSWER = 7,

    /**
     * @brief 用户程序出现运行时错误
     * 出现了 SIG_SEGV、SIG_FPE 以外信号而崩溃的运行时错误。
     * 或者在不支持 SF、FPE 的评测系统中，表示所有的运行时错误。
     */
    RUNTIME_ERROR = 8,

    /**
     * @brief 用户程序运行时间超出限制
     * 根据评测选项，可以比较时钟时间、用户时间、系统时间、CPU 时间。
     */
    TIME_LIMIT_EXCEEDED = 9,

    /**
     * @brief 用户程序运行内存超限
     * 仅支持 Java（通过检查输出中是否包含 OutOfMemoryError）
     * 由于 cgroup 限制内存使用会导致在内存不足时调用 malloc 返回
     * NULL，或者是 new 抛出 bad_alloc 异常，此时内存超限会被认为是
     * RE（或者是 SIG_SEGV），如果用户程序自行捕获了该错误，则评测
     * 结果可能变成其他值。
     */
    MEMORY_LIMIT_EXCEEDED = 10,

    /**
     * @brief 用户程序写文件内容过多
     */
    OUTPUT_LIMIT_EXCEEDED = 11,

    /**
     * @brief 格式错误
     * 一般是行末空格\文末空行和标准输出不符。
     * 需要手动开启 PE 的评测选项才会返回 PE。
     */
    PRESENTATION_ERROR = 12,

    /**
     * @brief 访问受限的系统调用
     * 本评测系统不会返回该结果，因为所有的系统调用都是允许的。
     * 应用程序的权限控制依赖 cgroup 和 unshare，而不使用
     * setcomp 和 ptrace
     */
    RESTRICT_FUNCTION = 13,

    /**
     * @brief 表示超出比赛时间的提交
     * 服务器可能会发送超出比赛时间的提交，评测系统也要处理这种提交。
     * 对于 Sicily，这个是 Out Of Contest Time
     * 对于 DOMjudge，这个是 TOO LATE
     */
    OUT_OF_CONTEST_TIME = 14,

    /**
     * @brief 评测系统正在编译程序
     * server 将编译任务派发出去等待 client 编译完成的状态
     */
    COMPILING = 15,

    /**
     * @brief 应用程序出现段错误，一般是访问了非法内存地址导致的。
     * 捕捉到段 SIG_SEGV 信号时返回该评测结果。
     * 对于不支持段错误的评测系统，需要将该项标记为运行时错误。
     */
    SEGMENTATION_FAULT = 16,

    /**
     * @brief 浮点运算错误，一般是除零错误。
     * 捕捉到 SIG_FPE 信号时会返回该评测结果。
     * 对于不支持段错误的评测系统，需要将该项标记为运行时错误。
     */
    FLOATING_POINT_ERROR = 17,

    /**
     * @brief 随机测试生成器或者标准程序运行错误
     * 如果评测系统不支持此项，应当把此项标记为内部错误。
     */
    RANDOM_GEN_ERROR = 18,

    /**
     * @brief 比较程序出错或超时
     * 如果评测系统不支持此项，应当把此项标记为内部错误。
     */
    COMPARE_ERROR = 19,

    /**
     * @brief 内部错误，评测系统出错
     * 比如 runguard 或 server 或 client 出错。
     */
    SYSTEM_ERROR = 20
};

const char *get_display_message(status);

}  // namespace judge
