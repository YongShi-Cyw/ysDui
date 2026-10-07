#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiTextInput.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

class DuiSearchBox final : public core::Control, public render::DuiRenderable {
public:
    DuiSearchBox();
    ~DuiSearchBox() override;
    DuiSearchBox(const DuiSearchBox&) = delete;
    DuiSearchBox& operator=(const DuiSearchBox&) = delete;
    DuiSearchBox(DuiSearchBox&&) noexcept;
    DuiSearchBox& operator=(DuiSearchBox&&) noexcept;
    void SetTextInput(ui::DuiTextInput* input);
    [[nodiscard]] ui::DuiTextInput* TextInput() const;
    void SetText(std::string text, bool notify = false);
    [[nodiscard]] std::string Text() const;
    void SetPlaceholder(std::string text);
    void SetReadOnly(bool value);
    void SetMaxLength(int length);
    void SetGlyphStripWidth(int width);
    void SetClearStripWidth(int width);
    [[nodiscard]] bool ClearShowing() const;
    [[nodiscard]] core::Rect ClearRect() const;
    void SetTextChangedHandler(std::function<void(std::string_view)> handler);
    void Layout(core::Rect bounds);
    [[nodiscard]] core::Size DesiredSize() const override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;
private:
    class Impl;
    std::unique_ptr<Impl> searchBox_;
};

} // namespace ysDui::controls::input
