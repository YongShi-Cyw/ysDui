/**
 * 文件名：DuiMarkdownImagePipeline.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：Markdown 同步短缓存与异步本地图片管线。
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "ysDui/controls/media/DuiAsyncImageLoader.hpp"
#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/render/DuiImage.hpp"

namespace ysDui::controls::content {

/** 同步短缓存 + 异步本地图管线。 */
struct MarkdownImagePipeline final {
    std::function<std::shared_ptr<const render::DuiImage>(std::string_view src)> provider;
    media::DuiAsyncImageLoader* asyncLoader{};
    mutable std::unordered_map<std::string, std::shared_ptr<const render::DuiImage>> syncCache;
    mutable std::unordered_map<std::string, std::shared_ptr<const render::DuiImage>> asyncImages;
    mutable std::unordered_set<std::string> asyncFailed;
    mutable std::unordered_map<std::string, std::uint64_t> asyncPending;
    mutable std::unordered_map<std::uint64_t, std::string> asyncPendingById;
    core::AnimationClock* clock{};
    core::AnimationClock::TaskId pollTask{};
    bool pollTaskActive{};
    std::function<void()> onAsyncReady; // 由 View 注入：轮询并 InvalidateLayout

    void CancelPoll();
    void Clear();
    void SchedulePoll();
    bool Poll();
    [[nodiscard]] std::shared_ptr<const render::DuiImage> Resolve(std::string_view src) const;
};

} // namespace ysDui::controls::content
