/**
 * 文件名：DuiMarkdownViewState.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：DuiMarkdownView 划选与横向滚动状态（从 View.cpp 拆出以降耦合）。
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <string>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::controls::content {

constexpr int kHScrollBarHeight = 8; // 底栏横向滚动条高度（DIP）
constexpr int kHScrollThumbMin = 28; // 滑块最小宽度

/** 正文划选状态。 */
struct SelectionState final {
    mutable std::size_t anchor{};
    mutable std::size_t caret{};
    bool dragging{};

    [[nodiscard]] bool Has() const { return anchor != caret; }

    void Clear() { anchor = caret = 0; }

    void ClampTo(std::size_t plainSize) const
    {
        anchor = (std::min)(anchor, plainSize);
        caret = (std::min)(caret, plainSize);
    }

    [[nodiscard]] std::string Plain(const std::string& plainText) const
    {
        const std::size_t first = (std::min)(anchor, caret);
        const std::size_t last = (std::max)(anchor, caret);
        if (first >= last || first >= plainText.size())
            return {};
        return plainText.substr(first, last - first);
    }
};

/** 宽表横向滚动与底栏滑块。 */
struct HScrollState final {
    mutable int offsetX{};
    bool panning{};
    int panStartScreenX{};
    int panStartScrollX{};
    bool thumbDragging{};
    int thumbGrabX{};

    void Clamp(int contentWidth, int viewportWidth) const
    {
        const int maxScroll = (std::max)(0, contentWidth - (std::max)(1, viewportWidth));
        offsetX = (std::clamp)(offsetX, 0, maxScroll);
    }

    [[nodiscard]] int MaxScroll(int contentWidth, int viewportWidth) const
    {
        return (std::max)(0, contentWidth - (std::max)(1, viewportWidth));
    }

    [[nodiscard]] bool Needed(int contentWidth, int viewportWidth) const
    {
        return MaxScroll(contentWidth, viewportWidth) > 0;
    }

    [[nodiscard]] core::Rect TrackLocal(int contentWidth, int viewportWidth, int viewportHeight) const
    {
        if (!Needed(contentWidth, viewportWidth) || viewportHeight < kHScrollBarHeight + 4)
            return {};
        return {0, viewportHeight - kHScrollBarHeight, viewportWidth, viewportHeight};
    }

    [[nodiscard]] core::Rect ThumbLocal(int contentWidth, int viewportWidth, int viewportHeight) const
    {
        const core::Rect track = TrackLocal(contentWidth, viewportWidth, viewportHeight);
        if (track.Empty())
            return {};
        const int maxScroll = MaxScroll(contentWidth, viewportWidth);
        const int trackW = track.Width();
        int thumbW = trackW * viewportWidth / (std::max)(1, contentWidth);
        thumbW = (std::clamp)(thumbW, kHScrollThumbMin, trackW);
        const int travel = (std::max)(0, trackW - thumbW);
        const int thumbX = maxScroll > 0 ? travel * offsetX / maxScroll : 0;
        return {track.left + thumbX, track.top, track.left + thumbX + thumbW, track.bottom};
    }

    void SetFromThumb(int localX, int contentWidth, int viewportWidth, int viewportHeight) const
    {
        const core::Rect track = TrackLocal(contentWidth, viewportWidth, viewportHeight);
        const core::Rect thumb = ThumbLocal(contentWidth, viewportWidth, viewportHeight);
        if (track.Empty() || thumb.Empty())
            return;
        const int travel = (std::max)(0, track.Width() - thumb.Width());
        if (travel <= 0)
            return;
        const int desiredLeft = localX - thumbGrabX;
        const int clamped = (std::clamp)(desiredLeft - track.left, 0, travel);
        offsetX = MaxScroll(contentWidth, viewportWidth) * clamped / travel;
        Clamp(contentWidth, viewportWidth);
    }
};

} // namespace ysDui::controls::content
