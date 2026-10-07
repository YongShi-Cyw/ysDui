/**
 * 文件名：DuiAsyncImageLoader.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的异步图像加载、缓存和结果轮询接口。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/render/DuiImage.hpp"

namespace ysDui::controls::media {

/** 异步图像请求的处理结果。 */
enum class DuiAsyncImageError
{
    // 成功：已取得可由调用方使用的图像。
    Succeeded,
    // 解码失败：平台提供的同步解码函数未返回图像。
    DecodeFailed,
    // 已取消：请求尚未完成时被调用方撤销。
    Cancelled,
};

/**
 * 已完成异步图像请求的数据。
 */
struct DuiAsyncImageResult final
{
    std::uint64_t requestId{};
    std::uint64_t userToken{};
    std::shared_ptr<const render::DuiImage> image;
    DuiAsyncImageError error{DuiAsyncImageError::DecodeFailed};
};

/**
 * 平台无关的异步图像加载器。
 *
 * 解码函数在一个后台线程执行，调用方通过 Poll 在自己的线程上取得完成结果。
 * 相同路径的并发请求共用一次解码；成功结果按路径进行 LRU 缓存。
 */
class DuiAsyncImageLoader final
{
public:
    /** 用于同步解码单个图像文件的函数类型。 */
    using Decoder = std::function<std::shared_ptr<const render::DuiImage>(const std::string& path)>;

    /** 创建加载器并启动后台解码线程。 */
    DuiAsyncImageLoader();
    /** 停止后台线程并释放缓存。 */
    ~DuiAsyncImageLoader();
    DuiAsyncImageLoader(const DuiAsyncImageLoader&) = delete;
    DuiAsyncImageLoader& operator=(const DuiAsyncImageLoader&) = delete;
    DuiAsyncImageLoader(DuiAsyncImageLoader&&) = delete;
    DuiAsyncImageLoader& operator=(DuiAsyncImageLoader&&) = delete;

    /**
     * 设置平台同步解码函数。
     * @param decoder 在后台线程执行的解码函数。
     */
    void SetDecoder(Decoder decoder);
    /**
     * 提交图像加载请求。
     * @param path 图像文件路径。
     * @param userToken 调用方关联到请求的不透明标记。
     * @return 非零请求标识；未配置解码器或路径为空时返回零。
     */
    [[nodiscard]] std::uint64_t Submit(std::string path, std::uint64_t userToken = 0);
    /**
     * 取消尚未投递结果的请求。
     * @param requestId Submit 返回的请求标识。
     */
    void Cancel(std::uint64_t requestId);
    /**
     * 获取已经完成的请求结果。
     * @return 按完成顺序排列的结果，并从加载器内部队列移除。
     */
    [[nodiscard]] std::vector<DuiAsyncImageResult> Poll();
    /**
     * 设置成功图像的 LRU 缓存容量。
     * @param capacity 最大缓存条目数；零表示禁用缓存。
     */
    void SetCacheCapacity(std::size_t capacity);
    /** 清空已缓存的成功图像。 */
    void ClearCache();

private:
    class Impl;
    std::unique_ptr<Impl> loader_;
};

} // namespace ysDui::controls::media
