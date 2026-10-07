#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::layout {

struct DuiFlowThickness final {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

struct DuiFlowItem final {
    core::Size desiredSize{};
    DuiFlowThickness margin{};
};

class DuiFlow final : public core::Control, public render::DuiRenderable {
public:
    DuiFlow();
    ~DuiFlow() override;
    DuiFlow(const DuiFlow&) = delete;
    DuiFlow& operator=(const DuiFlow&) = delete;
    DuiFlow(DuiFlow&&) noexcept;
    DuiFlow& operator=(DuiFlow&&) noexcept;

    void SetPadding(DuiFlowThickness padding);
    [[nodiscard]] DuiFlowThickness GetPadding() const;
    void SetGap(int pixels);
    [[nodiscard]] int GetGap() const;

    void AddChild(std::unique_ptr<core::Control> child, DuiFlowItem item);
    void SetItem(core::Control* child, DuiFlowItem item);
    [[nodiscard]] DuiFlowItem GetItem(core::Control* child) const;
    void Layout(core::Rect bounds);
    [[nodiscard]] core::Size DesiredSize() const override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> flow_;
};

} // namespace ysDui::controls::layout
