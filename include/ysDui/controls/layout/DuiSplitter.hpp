#pragma once

#include <functional>
#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::layout {

enum class DuiSplitterOrientation {
    Vertical,
    Horizontal,
};

class DuiSplitter final : public core::Control, public render::DuiRenderable {
public:
    DuiSplitter();
    ~DuiSplitter() override;
    DuiSplitter(const DuiSplitter&) = delete;
    DuiSplitter& operator=(const DuiSplitter&) = delete;
    DuiSplitter(DuiSplitter&&) noexcept;
    DuiSplitter& operator=(DuiSplitter&&) noexcept;

    void SetOrientation(DuiSplitterOrientation orientation);
    [[nodiscard]] DuiSplitterOrientation GetOrientation() const;
    void SetBarThickness(int pixels);
    [[nodiscard]] int GetBarThickness() const;
    void SetMinSizes(int first, int second);
    void SetSplitPixels(int first);
    [[nodiscard]] int SplitPixels() const;
    /**
     * @return 第一个窗格占轨道的比例；轨道无宽度时返回 0。
     * 用途：持久化布局时记录比例（像素值在宿主尺寸变化后会走形）。
     */
    [[nodiscard]] double SplitFraction() const;
    void SetSplitFraction(double fraction);
    void SetPane(int index, std::unique_ptr<core::Control> pane);
    [[nodiscard]] core::Control* GetPane(int index) const;
    void SetValueChangedHandler(std::function<void(int)> handler);
    void SetBarColor(core::Color color);

    void Layout(core::Rect bounds);
    [[nodiscard]] core::Rect BarRect() const;
    core::Control* HitTest(core::Point point) override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> splitter_;
};

} // namespace ysDui::controls::layout
