#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "server/asset.hpp"

/**
 * 这个头文件包含表示可以编译的可执行文件的类
 * 我们有两种可执行文件：
 * 1. executable: 是用来提供给评测系统使用的外置程序，用 zip 打包。
 *                比如比较脚本、运行脚本、测试脚本和编译脚本，
 *                这些脚本一般通过配置系统进行维护，是内部使用的。
 *                评测系统会允许 executable 获得 root 的执行权限，因此 executable 比较敏感。
 * 2. source_code：是表示外部导入进来的程序，比如选手的代码、
 *                 Matrix 的随机数据生成器、评测标准程序、比较程序。
 * 区别：
 * 1. executable 比较灵活，通过提供 build 脚本编译运行，不需要明确编程语言。
 * source_code 需要提供语言，在选择 Makefile 语言之后其实也可以表示成 executable。
 * 2. executable 一般不在保护模式下运行，且有 root 权限；而 source code 在保护模式下运行。
 */
namespace judge::server {
using namespace std;

/**
 * @brief 表示 program 编译错误
 * 可以表示 program 格式不正确，或者编译错误
 */
struct compilation_error : public runtime_error {
public:
    const string error_log;

    explicit compilation_error(const string &what, const string &error_log);
};

/**
 * @brief 表示 executable 编译错误，表示标准程序不合法
 */
struct executable_compilation_error : public compilation_error {
public:
    const string error_log;

    explicit executable_compilation_error(const string &what, const string &error_log);
};

/**
 * @brief 表示一个程序
 * 这个类负责下载代码、编译、生成可执行文件
 */
struct program {
    /**
     * @brief 下载并编译程序
     * @param cpuset 编译器所能使用的 CPU 核心
     * @param workdir 评测的工作路径，源代码将下载到这个文件夹中
     * @param chrootdir 配置好的 Linux 子系统环境
     */
    virtual void fetch(const string &cpuset, const filesystem::path &workdir, const filesystem::path &chrootdir) = 0;

    /**
     * @brief 获取程序的编译信息
     * @param workdir 评测的工作路径
     * @return 获得的编译信息
     */
    virtual string get_compilation_log(const filesystem::path &workdir) = 0;

    /**
     * @brief 获得程序的可执行文件路径
     * @param path 评测的工作路径
     * @return 可执行文件路径，有些程序可能必须要使用参数，你可以通过一个不需要参数的 bash 脚本来运行这个程序
     */
    virtual filesystem::path get_run_path(const filesystem::path &path) = 0;
};

/**
 * @brief 表示一个打包的外部程序
 * 每个 executable 的源代码根目录必须包含一个 run 程序，届时调用该 executable
 * 就是调用这个 ./run 程序。如果源代码需要编译，你可以在根目录放一个 ./build 脚本
 * 来执行编译命令，编译完后确保能生成 ./run 程序，或者自行编写一个 ./run bash
 * 脚本来执行你的程序。
 */
struct executable : program {
    filesystem::path dir, runpath;
    string id, md5sum;

    /**
     * @brief executable 的构造函数
     * @param id executable 的 id，用来索引（比如下载时需要 executable 的 name）
     * @param workdir 统一存放所有 executable 的位置，你只需要给定一个全局的常量即可
     * @param asset 这个 executable 的获取方式，可以是本地复制，或者远程下载
     * @param md5sum 如果 executable 是远程的，那么我们需要比对哈希值来验证下载下来的 zip 压缩包是否无损
     */
    executable(const string &id, const filesystem::path &workdir, asset_uptr &&asset, const string &md5sum = "");

    /**
     * @brief 获取并编译 executable
     * executable 有自己的文件存放路径，因此不使用 fetch 传入的 path
     */
    void fetch(const string &cpuset, const filesystem::path &dir, const filesystem::path &chrootdir) override;

    /**
     * @brief 获取并编译 executable
     */
    void fetch(const string &cpuset, const filesystem::path &chrootdir);

    string get_compilation_log(const filesystem::path &workdir) override;

    /**
     * @brief 获得 executable 的可执行文件路径
     * executable 有自己的文件存放路径，因此不使用传入的 path 来计算可执行文件路径
     */
    filesystem::path get_run_path(const filesystem::path & = filesystem::path()) override;

protected:
    filesystem::path md5path, deploypath, buildpath;
    asset_uptr asset;
};

/**
 * @brief 表示 executable 的本地复制方式
 * 本地复制方式支持从 execdir 文件夹内获得已经配置好的 executables
 */
struct local_executable_asset : public asset {
    filesystem::path dir, execdir;

    local_executable_asset(const string &type, const string &id, const filesystem::path &execdir);

    void fetch(const filesystem::path &dir) override;
};

/**
 * @brief 表示 executable 的远程下载方式
 * 远程下载方式从远程服务器下载 executable 的 zip 压缩包
 * （通过 remote_asset 来进行下载），之后会验证压缩包正确。
 * 压缩包的 md5 哈希值来确保压缩包正确，并予以解压
 */
struct remote_executable_asset : public asset {
    string md5sum;
    asset_uptr remote_asset;

    remote_executable_asset(asset_uptr &&remote_asset, const string &md5sum);

    void fetch(const filesystem::path &dir) override;
};

/**
 * @brief 外置脚本的全局管理器
 * 本类将允许调用者访问全局配置的所有外置脚本，比如允许访问基础的编译脚本、运行脚本、检查脚本、比较脚本。
 */
struct executable_manager {
    /**
     * @brief 根据语言获取编译脚本
     * @param language 编程语言，必须和该编译脚本的外置脚本名一致
     * @return 尚未完成下载、编译的编译脚本信息 executable 类，需要手动调用 fetch 来进行下载
     */
    virtual unique_ptr<executable> get_compile_script(const string &language) const = 0;

    /**
     * @brief 根据语言获取运行脚本
     * @param language 编程语言，必须和该编译脚本的外置脚本名一致
     * @return 尚未完成下载、编译的编译脚本信息 executable 类，需要手动调用 fetch 来进行下载
     */
    virtual unique_ptr<executable> get_run_script(const string &language) const = 0;

    /**
     * @brief 根据名称获取测试脚本
     * @param name 测试脚本名
     * @return 尚未完成下载、编译的编译脚本信息 executable 类，需要手动调用 fetch 来进行下载
     */
    virtual unique_ptr<executable> get_check_script(const string &name) const = 0;

    /**
     * @brief 根据语言获取编译脚本
     * @param name 比较脚本名，可能的名称有 diff-ignore-tailing-space，diff-all
     * @return 尚未完成下载、编译的编译脚本信息 executable 类，需要手动调用 fetch 来进行下载
     */
    virtual unique_ptr<executable> get_compare_script(const string &name) const = 0;
};

/**
 * @brief 外置脚本的全局管理器，其中外置脚本直接从本地预设中查找.
 */
struct local_executable_manager : public executable_manager {
    /**
     * @brief 构造函数
     * @param workdir 存放 executable 的临时文件夹，用于存放编译后的程序
     * @param execdir 存放 executable 的本地预设文件夹
     */
    local_executable_manager(const filesystem::path &workdir, const filesystem::path &execdir);

    unique_ptr<executable> get_compile_script(const string &language) const;
    unique_ptr<executable> get_run_script(const string &language) const;
    unique_ptr<executable> get_check_script(const string &name) const;
    unique_ptr<executable> get_compare_script(const string &name) const;

private:
    // 存放 executable 的临时文件夹，用于存放编译后的程序
    filesystem::path workdir;
    // 存放 executable 的本地预设文件夹
    filesystem::path execdir;
};

/**
 * @class source_code
 */
struct source_code : program {
    /**
     * @brief 该程序使用的语言
     * @note 不能为空，这涉及判题时使用的编译器与执行器
     * 因此对于 source_code，其会自行查找 get_run_script 和 get_compile_script
     */
    string language;

    /**
     * @brief 应用程序入口
     * 对于 Java，entry_point 为应用程序主类。若空，默认为 Main
     * 对于 Python，entry_point 为默认的 python 脚本。若空，为 source_files 的第一个文件
     */
    string entry_point;

    /**
     * @brief 该程序的所有源文件的文件系统路径
     * @note 不可为空
     * @code{.json}
     * ["somedir/framework.cpp", "somedir/source.cpp"]
     * @endcode
     */
    vector<asset_uptr> source_files;

    /**
     * @brief 该程序的所有不参与编译（比如 c/c++ 的头文件）的文件的文件系统路径
     * @note 由于 gcc/g++ 在编译时会自行判断传入的源文件是不是头文件，因此没有
     * 必要专门将头文件放在这里，这个变量仅在文件必须不能传给编译器时使用。（对于
     * sicily 的 framework 评测，两个源文件为 framework.cpp, source.cpp，但是
     * source.cpp 是由 framework.cpp include 进来参与编译，如果将 source.cpp
     * 加入到编译器输入中将会产生链接错误）
     * @code{.json}
     * ["somedir/source.cpp"]
     * @endcode
     */
    vector<asset_uptr> assist_files;

    /**
     * @brief 该程序的额外编译命令
     * @note 这个参数将传送给 compile script，请参见这些脚本以获得更多信息
     * 
     * 示例（对于 gcc）:
     * -g -lcgroup -Wno-long-long -nostdinc -nostdinc++
     */
    vector<string> compile_command;

    source_code(executable_manager &exec_mgr);

    void fetch(const string &cpuset, const filesystem::path &dir, const filesystem::path &chrootdir) override;
    string get_compilation_log(const filesystem::path &workdir) override;
    filesystem::path get_run_path(const filesystem::path &path) override;

protected:
    executable_manager &exec_mgr;
};

}  // namespace judge::server
