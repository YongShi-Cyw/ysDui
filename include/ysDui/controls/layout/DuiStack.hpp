#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::layout {

enum class DuiStackOrientation {
    Horizontal,
    Vertical,
};

enum class DuiStackMode {
    WeightedLayout,
    Pages,
};

struct DuiThickness final {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

struct DuiStackItem final {
    int mainLength{-1};
    int weight{1};
    DuiThickness margin{};
};

class DuiStack final : public core::Control, public render::DuiRenderable {
public:
    DuiStack();
    ~DuiStack() override;
    DuiStack(const DuiStack&) = delete;
    DuiStack& operator=(const DuiStack&) = delete;
    DuiStack(DuiStack&&) noexcept;
    DuiStack& operator=(DuiStack&&) noexcept;

    void SetOrientation(DuiStackOrientation orientation);
    [[nodiscard]] DuiStackOrientation GetOrientation() const;
    void SetPadding(DuiThickness padding);
    [[nodiscard]] DuiThickness GetPadding() const;
    void SetGap(int pixels);
    [[nodiscard]] int GetGap() const;

    void SetMode(DuiStackMode mode);
    [[nodiscard]] DuiStackMode GetMode() const;

    void AddChild(std::unique_ptr<core::Control> child, DuiStackItem item);
    void SetItem(core::Control* child, DuiStackItem item);
    [[nodiscard]] DuiStackItem GetItem(core::Control* child) const;

    void AddPage(std::unique_ptr<core::Control> page);
    [[nodiscard]] std::unique_ptr<core::Control> RemovePage(int index);
    [[nodiscard]] int PageCount() const;
    void SetCurrentIndex(int index);
    [[nodiscard]] int CurrentIndex() const;
    void Layout(core::Rect bounds);
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> stack_;
};

} // namespace ysDui::controls::layout
