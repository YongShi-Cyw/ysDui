#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::layout {

enum class DuiDockSide {
    Top,
    Bottom,
    Left,
    Right,
    Fill,
};

struct DuiDockPadding final {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

class DuiDock final : public core::Control, public render::DuiRenderable {
public:
    DuiDock();
    ~DuiDock() override;
    DuiDock(const DuiDock&) = delete;
    DuiDock& operator=(const DuiDock&) = delete;
    DuiDock(DuiDock&&) noexcept;
    DuiDock& operator=(DuiDock&&) noexcept;

    void SetPadding(DuiDockPadding padding);
    [[nodiscard]] DuiDockPadding GetPadding() const;
    void SetGap(int pixels);
    [[nodiscard]] int GetGap() const;

    void AddDocked(std::unique_ptr<core::Control> child, DuiDockSide side, int size = 0);
    void SetDock(core::Control* child, DuiDockSide side, int size = 0);
    [[nodiscard]] DuiDockSide GetDock(core::Control* child) const;
    [[nodiscard]] int GetDockSize(core::Control* child) const;
    void Layout(core::Rect bounds);
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> dock_;
};

} // namespace ysDui::controls::layout
