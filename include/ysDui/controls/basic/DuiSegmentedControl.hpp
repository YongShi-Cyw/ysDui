#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::basic {

class DuiSegmentedControl final : public core::Control, public render::DuiRenderable {
public:
    DuiSegmentedControl();
    ~DuiSegmentedControl() override;
    DuiSegmentedControl(const DuiSegmentedControl&) = delete;
    DuiSegmentedControl& operator=(const DuiSegmentedControl&) = delete;
    DuiSegmentedControl(DuiSegmentedControl&&) noexcept;
    DuiSegmentedControl& operator=(DuiSegmentedControl&&) noexcept;

    void AddSegment(std::string text);
    void ClearSegments();
    [[nodiscard]] int SegmentCount() const;
    [[nodiscard]] std::string SegmentText(int index) const;
    void SetSelectedIndex(int index, bool notify = false);
    [[nodiscard]] int SelectedIndex() const;
    void SetSelectionChangedHandler(std::function<void(int)> handler);
    void SetTextStyle(render::DuiTextStyle style);
    void SetBackgroundColor(core::Color color);
    void SetSelectedColor(core::Color color);
    void SetTextColor(core::Color color);
    void SetSelectedTextColor(core::Color color);
    [[nodiscard]] core::Rect SegmentRect(int index) const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> segmented_;
};

} // namespace ysDui::controls::basic
