#pragma once

#include <filesystem>
#include <memory>

namespace judge {

/**
 * @class asset
 * @brief 远程或本地的文件，比如测试数据，或者源代码
 */
struct asset {
    /**
     * @brief 该远程文件在本地的文件名
     * @note 对于测试数据，这个 name 将是选手程序直接打开的文件路径名。
     *      比如 name="folder/disk.in"，那么选手可以通过 fopen("folder/disk.in", "r") 来打开文件
     */
    std::string name;

    asset(const std::string &name);

    /**
     * @brief 获取远程文件
     * @param path 要下载到的目录
     * @note 该函数在下载过程中将阻塞
     * @note 你必须为这个函数加锁
     * @note 你可以利用该函数实现远程文件的缓存
     * @note 该函数会抛出异常
     */
    virtual void fetch(const std::filesystem::path &dir) = 0;
};

/**
 * @brief 表示一个已经在本地的资源文件（不需要下载）
 */
struct local_asset : public asset {
    std::filesystem::path path;

    local_asset(const std::string &name, const std::filesystem::path &path);

    void fetch(const std::filesystem::path &dir) override;
};

/**
 * @brief 表示已经知道内容的文本资源（不需要下载）
 */
struct text_asset : public asset {
    std::string text;

    text_asset(const std::string &name, const std::string &text);

    void fetch(const std::filesystem::path &dir) override;
};

/**
 * @brief 从远程下载的资源文件（只支持 GET 请求下载）
 */
struct remote_asset : public asset {
    std::string url;

    remote_asset(const std::string &name, const std::string &url_get);

    void fetch(const std::filesystem::path &dir) override;
};

typedef std::shared_ptr<asset> asset_ptr;
typedef std::unique_ptr<asset> asset_uptr;

}  // namespace judge
