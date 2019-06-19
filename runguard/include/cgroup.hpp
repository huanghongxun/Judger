#pragma once

#include <exception>
#include <string>

struct cgroup;
struct cgroup_controller;

struct cgroup_exception : public std::exception {
    cgroup_exception(std::string cgroup_op, int err);

    const char *what() const noexcept override;

    static void ensure(std::string cgroup_op, int err);
private:
    std::string errmsg;
};

/**
 * @brief 表示一个 cgroup 的 controller
 * 
 * RHEL 中可用的 controller 有：
 * 1. blkio - 对输入 / 输出访问存取块设备设定权限
 * 2. cpu - 使用 CPU 调度程序让 cgroup 的任务可以存取 CPU。它与 cpuacct 管控器一起挂载在同一 mount 上
 * 3. cpuacct - 自动生成 cgroup 中任务占用 CPU 资源的报告。它与 cpu 管控器一起挂载在同一 mount 上
 * 4. cpuset - 给 cgroup 中的任务分配独立 CPU（在多芯系统中）和内存节点
 * 5. devices - 允许或禁止 cgroup 中的任务存取设备
 * 6. freezer - 暂停或恢复 cgroup 中的任务
 * 7. memory - 对 cgroup 中的任务可用内存做出限制，并且自动生成任务占用内存资源报告
 * 8. net_cls - 使用等级识别符（classid）标记网络数据包，这让 Linux 流量控制器（tc 指令）可以识别来自特定 cgroup 任务的数据包
 * 9. perf_event - 允许使用 perf 工具来监控 cgroup
 * 10. hugetlb - 允许使用大篇幅的虚拟内存页，并且给这些内存页强制设定可用资源量
 * 
 * https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/resource_management_guide/ch-subsystems_and_tunable_parameters
 */
struct cgroup_ctrl {
    struct cgroup_controller *ctrl;

    /**
     * @brief 为 controller 添加设定
     */
    void add_value(const std::string &name, int64_t value);

    /**
     * @brief 为 controller 添加设定
     */
    void add_value(const std::string &name, std::string value);

    int64_t get_value_int64(const std::string &name);
};

/**
 * @brief 创建指定 cgroup 的管理器
 * 在析构时释放内存以确保没有内存泄漏
 */
struct cgroup_guard {
    /**
     * @brief 构造函数，调用 libcgroup 的创建函数
     * @param cgroup_name cgroup 的内核名称
     */
    cgroup_guard(const std::string &cgroup_name);

    /**
     * @brief 析构函数，调用 libcgroup 的释放函数
     */
    ~cgroup_guard();

    /**
     * @brief 在内核中创建这个 cgroup
     * cgroup_guard 在创建时只会记录 cgroup 的信息，而不会对内核中存储的 cgroup
     * 进行修改。通过 create_cgroup 能真正在内核中创建这个 cgroup。
     * 这里将 add_controller 函数、add_value 函数添加的数据也写入内核中。
     * cgroup 的所有者将通过预先调用 cgroup_set_uid_gid() 来实现，修改权限通过
     * cgroup_set_permissions 来实现。
     */
    void create_cgroup(int ignore_ownership);

    /**
     * @brief 创建一个新的 controller
     * @see cgroup_ctrl
     * @param name 控制器的名称，如 "freezer"
     * @return 创建好的 cgroup controller
     * @throw cgroup_exception 当创建失败时
     */
    cgroup_ctrl add_controller(const std::string &name);

    /**
     * @brief 从 cgroup 中获得指定的 controller
     * 必须是 add_controller 已添加过的或者根据 get_cgroup 从内核中获得的已有的 controller
     * @see cgroup_ctrl
     * @param name 控制器的名称，如 "freezer"
     * @return 已有的 cgroup controller
     * @throw cgroup_exception 当 controller 不存在时
     */
    cgroup_ctrl get_controller(const std::string &name);

    /**
     * 从内核中读入 cgroup 的所有信息。
     * 将会读入 cgroup 绑定的所有 controller、所有的 parameter 和对应的值。
     */
    void get_cgroup();

    /**
     * 将当前线程（进程）移入本 cgroup
     */
    void attach_task();

    /**
     * @brief 从内核中删除这个 cgroup。
     * 所有的进程都会被移入上一层的 cgroup。所有的子 cgroup 都会被删除。
     */
    void delete_cgroup();

    static void init();

private:
    struct cgroup *cg;
};
