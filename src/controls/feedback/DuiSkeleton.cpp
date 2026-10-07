/**
 * 文件名：DuiSkeleton.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-31
 * 用途：实现骨架占位条的绘制。
 */
#include "ysDui/controls/feedback/DuiSkeleton.hpp"

#include <algorithm>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::feedback {
namespace {
int ClampPercent(int value) { return (std::clamp)(value, 1, 100); }
}

class DuiSkeleton::Impl {
public:
    int lineCount{3};
    int lineHeight{12};
    int gap{8};
    int cornerRadius{4};
    int lineWidthPercent{100};
};

DuiSkeleton::DuiSkeleton() : skeleton_(std::make_unique<Impl>()) {}
DuiSkeleton::~DuiSkeleton() = default;
DuiSkeleton::DuiSkeleton(DuiSkeleton&&) noexcept = default;
DuiSkeleton& DuiSkeleton::operator=(DuiSkeleton&&) noexcept = default;
void DuiSkeleton::SetLineCount(int count) { skeleton_->lineCount = (std::max)(0, count); }
int DuiSkeleton::LineCount() const { return skeleton_->lineCount; }
void DuiSkeleton::SetLineHeight(int pixels) { skeleton_->lineHeight = (std::max)(1, pixels); }
int DuiSkeleton::LineHeight() const { return skeleton_->lineHeight; }
void DuiSkeleton::SetGap(int pixels) { skeleton_->gap = (std::max)(0, pixels); }
int DuiSkeleton::Gap() const { return skeleton_->gap; }
void DuiSkeleton::SetCornerRadius(int pixels) { skeleton_->cornerRadius = (std::max)(0, pixels); }
int DuiSkeleton::CornerRadius() const { return skeleton_->cornerRadius; }
void DuiSkeleton::SetLineWidthPercent(int percent) { skeleton_->lineWidthPercent = ClampPercent(percent); }
int DuiSkeleton::LineWidthPercent() const { return skeleton_->lineWidthPercent; }
core::Size DuiSkeleton::DesiredSize() const
{
    return {240, skeleton_->lineCount * skeleton_->lineHeight
        + (std::max)(0, skeleton_->lineCount - 1) * skeleton_->gap};
}
void DuiSkeleton::Layout(core::Rect bounds) { SetBounds(bounds); }
void DuiSkeleton::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty())
        return;
    const core::Color fill = Theme().Get(core::ThemeSlot::SurfaceAlternateBackground);
    for (int index = 0; index < skeleton_->lineCount; ++index)
    {
        const int top = Bounds().top + index * (skeleton_->lineHeight + skeleton_->gap);
        const int width = Bounds().Width() * skeleton_->lineWidthPercent / 100;
        const core::Rect line{Bounds().left, top, Bounds().left + width, top + skeleton_->lineHeight};
        const core::Rect clipped = core::Rect::Intersect(line, dirty);
        if (!clipped.Empty())
            canvas.FillRoundedRect(clipped, skeleton_->cornerRadius, fill);
    }
}
} // namespace ysDui::controls::feedback
