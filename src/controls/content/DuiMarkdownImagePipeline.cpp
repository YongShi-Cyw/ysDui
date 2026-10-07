/**
 * 文件名：DuiMarkdownImagePipeline.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：Markdown 图片同步缓存与异步本地加载实现。
 */
#include "DuiMarkdownImagePipeline.hpp"

#include <algorithm>
#include <cctype>

namespace ysDui::controls::content {
namespace {

constexpr std::size_t kMaxImageCache = 16; // 图片短缓存上限（按 src）
constexpr int kAsyncImagePollMs = 16;      // 异步图片轮询间隔

[[nodiscard]] bool IsRemoteOrDataSrc(std::string_view src)
{
    auto startsWithCi = [](std::string_view text, std::string_view prefix) -> bool
    {
        if (text.size() < prefix.size())
            return false;
        for (std::size_t i = 0; i < prefix.size(); ++i)
        {
            const auto a = static_cast<unsigned char>(text[i]);
            const auto b = static_cast<unsigned char>(prefix[i]);
            if (std::tolower(a) != std::tolower(b))
                return false;
        }
        return true;
    };
    return startsWithCi(src, "http://") || startsWithCi(src, "https://")
        || startsWithCi(src, "mailto:") || startsWithCi(src, "javascript:")
        || startsWithCi(src, "data:");
}

[[nodiscard]] std::string NormalizeLocalImagePath(std::string_view src)
{
    auto startsWithCi = [](std::string_view text, std::string_view prefix) -> bool
    {
        if (text.size() < prefix.size())
            return false;
        for (std::size_t i = 0; i < prefix.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(text[i]))
                != std::tolower(static_cast<unsigned char>(prefix[i])))
                return false;
        }
        return true;
    };
    if (startsWithCi(src, "file://"))
    {
        std::string_view path = src.substr(7);
        if (path.size() >= 3 && path.front() == '/' && path[2] == ':')
            path.remove_prefix(1);
        return std::string(path);
    }
    return std::string(src);
}

} // namespace

void MarkdownImagePipeline::CancelPoll()
{
    if (clock != nullptr && pollTaskActive)
    {
        clock->Cancel(pollTask);
        pollTaskActive = false;
    }
}

void MarkdownImagePipeline::Clear()
{
    if (asyncLoader != nullptr)
    {
        for (const auto& [id, src] : asyncPendingById)
        {
            (void)src;
            asyncLoader->Cancel(id);
        }
    }
    asyncPending.clear();
    asyncPendingById.clear();
    asyncImages.clear();
    asyncFailed.clear();
    syncCache.clear();
    CancelPoll();
}

void MarkdownImagePipeline::SchedulePoll()
{
    if (clock == nullptr || pollTaskActive || asyncPending.empty())
        return;
    pollTask = clock->Schedule(kAsyncImagePollMs, [this](double)
    {
        pollTaskActive = false;
        if (onAsyncReady)
            onAsyncReady();
    });
    pollTaskActive = true;
}

bool MarkdownImagePipeline::Poll()
{
    if (asyncLoader == nullptr)
        return false;
    const auto results = asyncLoader->Poll();
    bool changed = false;
    for (const media::DuiAsyncImageResult& result : results)
    {
        const auto it = asyncPendingById.find(result.requestId);
        if (it == asyncPendingById.end())
            continue;
        const std::string src = it->second;
        asyncPendingById.erase(it);
        asyncPending.erase(src);
        if (result.error == media::DuiAsyncImageError::Succeeded && result.image != nullptr)
        {
            asyncImages[src] = result.image;
            asyncFailed.erase(src);
        }
        else if (result.error != media::DuiAsyncImageError::Cancelled)
        {
            asyncFailed.insert(src);
        }
        changed = true;
    }
    if (!asyncPending.empty())
        SchedulePoll();
    return changed;
}

std::shared_ptr<const render::DuiImage> MarkdownImagePipeline::Resolve(std::string_view src) const
{
    const std::string key(src);
    if (provider)
    {
        if (const auto it = syncCache.find(key); it != syncCache.end())
            return it->second;
        auto image = provider(src);
        if (syncCache.size() >= kMaxImageCache)
            syncCache.clear();
        syncCache.emplace(key, image);
        if (image != nullptr)
            return image;
    }

    if (asyncLoader == nullptr || key.empty() || IsRemoteOrDataSrc(key))
        return nullptr;
    if (const auto it = asyncImages.find(key); it != asyncImages.end())
        return it->second;
    if (asyncFailed.contains(key) || asyncPending.contains(key))
        return nullptr;

    const std::string path = NormalizeLocalImagePath(key);
    if (path.empty())
        return nullptr;
    const std::uint64_t requestId = asyncLoader->Submit(path, 0);
    if (requestId == 0)
        return nullptr;
    asyncPending.emplace(key, requestId);
    asyncPendingById.emplace(requestId, key);
    const_cast<MarkdownImagePipeline*>(this)->SchedulePoll();
    if (const_cast<MarkdownImagePipeline*>(this)->Poll())
    {
        if (const auto ready = asyncImages.find(key); ready != asyncImages.end())
            return ready->second;
    }
    return nullptr;
}

} // namespace ysDui::controls::content
