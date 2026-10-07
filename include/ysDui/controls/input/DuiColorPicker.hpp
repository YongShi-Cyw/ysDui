#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiColorChooser.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::input {

class DuiColorPicker final : public core::Control, public render::DuiRenderable {
public:
    DuiColorPicker();
    ~DuiColorPicker() override;
    DuiColorPicker(const DuiColorPicker&) = delete;
    DuiColorPicker& operator=(const DuiColorPicker&) = delete;
    DuiColorPicker(DuiColorPicker&&) = delete;
    DuiColorPicker& operator=(DuiColorPicker&&) = delete;

    void SetColor(core::Color color, bool notify = false);
    [[nodiscard]] core::Color Color() const;
    void SetColorChangedHandler(std::function<void(core::Color)> handler);

    [[nodiscard]] static std::string FormatColorHex(core::Color color);
    [[nodiscard]] static core::Color ParseColorHex(std::string_view text);

    void SetPopupHost(ui::IPopupHost* popup);
    void SetColorChooser(ui::DuiColorChooser* chooser);
    [[nodiscard]] bool PopupOpen() const;
    void OpenPopup();
    void ClosePopup();

    [[nodiscard]] core::Rect SwatchRect() const;
    [[nodiscard]] core::Rect ArrowRect() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> colorPicker_;
};

} // namespace ysDui::controls::input
