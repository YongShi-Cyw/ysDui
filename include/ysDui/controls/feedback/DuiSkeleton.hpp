/**
 * 文件名：DuiSkeleton.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-31
 * 用途：提供数据加载期间使用的骨架占位控件。
 */
#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::feedback {

class DuiSkeleton final : public core::Control, public render::DuiRenderable {
public:
    DuiSkeleton();
    ~DuiSkeleton() override;
    DuiSkeleton(const DuiSkeleton&) = delete;
    DuiSkeleton& operator=(const DuiSkeleton&) = delete;
    DuiSkeleton(DuiSkeleton&&) noexcept;
    DuiSkeleton& operator=(DuiSkeleton&&) noexcept;

    void SetLineCount(int count);
    [[nodiscard]] int LineCount() const;
    void SetLineHeight(int pixels);
    [[nodiscard]] int LineHeight() const;
    void SetGap(int pixels);
    [[nodiscard]] int Gap() const;
    void SetCornerRadius(int pixels);
    [[nodiscard]] int CornerRadius() const;
    void SetLineWidthPercent(int percent);
    [[nodiscard]] int LineWidthPercent() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> skeleton_;
};

} // namespace ysDui::controls::feedback
