#pragma once

#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::basic {

class DuiStatusBar final : public core::Control, public render::DuiRenderable {
public:
    static constexpr int DefaultHeight = 22;

    DuiStatusBar();
    ~DuiStatusBar() override;
    DuiStatusBar(const DuiStatusBar&) = delete;
    DuiStatusBar& operator=(const DuiStatusBar&) = delete;
    DuiStatusBar(DuiStatusBar&&) noexcept;
    DuiStatusBar& operator=(DuiStatusBar&&) noexcept;

    [[nodiscard]] int AddPane(int width, bool spring = false);
    void ClearPanes();
    [[nodiscard]] int PaneCount() const;
    void SetPaneText(int index, std::string text);
    [[nodiscard]] std::string PaneText(int index) const;
    void SetPaneWidth(int index, int width);
    void SetPaneSpring(int index, bool spring);
    [[nodiscard]] core::Rect ComputePaneRect(int index, core::Rect bounds) const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void SetStyle(render::DuiTextStyle style);
    void SetBackgroundColor(core::Color color);
    void SetBorderColor(core::Color color);

    void Layout(core::Rect bounds);
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> statusBar_;
};

} // namespace ysDui::controls::basic
