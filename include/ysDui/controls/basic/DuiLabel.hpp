#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiClipboard.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::basic {

class DuiLabel final : public core::Control, public render::DuiRenderable {
public:
    DuiLabel();
    ~DuiLabel() override;
    DuiLabel(const DuiLabel&) = delete;
    DuiLabel& operator=(const DuiLabel&) = delete;
    DuiLabel(DuiLabel&&) noexcept;
    DuiLabel& operator=(DuiLabel&&) noexcept;

    void SetText(std::string text);
    [[nodiscard]] const std::string& Text() const;
    void SetStyle(render::DuiTextStyle style);
    [[nodiscard]] const render::DuiTextStyle& Style() const;
    void SetAlignment(render::DuiTextAlignment alignment);
    [[nodiscard]] render::DuiTextAlignment Alignment() const;
    void SetWordWrap(bool wordWrap);
    [[nodiscard]] bool WordWrap() const;
    void SetSelectable(bool selectable);
    [[nodiscard]] bool Selectable() const;
    void SetSelected(bool selected);
    [[nodiscard]] bool Selected() const;
    void SetSelectionColor(core::Color color);
    [[nodiscard]] core::Color SelectionColor() const;
    void SetClipboard(ui::DuiClipboard* clipboard);
    [[nodiscard]] std::string SelectedText() const;
    void SetLinkTarget(std::string target);
    [[nodiscard]] const std::string& LinkTarget() const;
    void SetVisited(bool visited);
    [[nodiscard]] bool Visited() const;
    void SetLinkActivatedHandler(std::function<void(std::string_view)> handler);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    class Impl;
    std::unique_ptr<Impl> label_;
};

} // namespace ysDui::controls::basic
