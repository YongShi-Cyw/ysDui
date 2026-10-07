#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiPathPicker.hpp"
#include "ysDui/ui/DuiTextInput.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::input {

class DuiPathEdit final : public core::Control, public render::DuiRenderable {
public:
    DuiPathEdit();
    ~DuiPathEdit() override;
    DuiPathEdit(const DuiPathEdit&) = delete;
    DuiPathEdit& operator=(const DuiPathEdit&) = delete;
    DuiPathEdit(DuiPathEdit&&) = delete;
    DuiPathEdit& operator=(DuiPathEdit&&) = delete;

    void SetPathPicker(ui::DuiPathPicker* picker);
    [[nodiscard]] ui::DuiPathPicker* PathPicker() const;
    void SetTextInput(ui::DuiTextInput* input);
    [[nodiscard]] ui::DuiTextInput* TextInput() const;
    void SetBrowseMode(ui::DuiPathPickerMode mode);
    [[nodiscard]] ui::DuiPathPickerMode BrowseMode() const;
    void SetPath(std::string path, bool notify = false);
    [[nodiscard]] std::string Path() const;
    void SetPlaceholder(std::string text);
    void SetBrowseText(std::string text);
    void SetBrowseWidth(int pixels);
    [[nodiscard]] core::Rect TextRect() const;
    [[nodiscard]] core::Rect BrowseRect() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void SetPathChangedHandler(std::function<void(std::string_view)> handler);
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;
private:
    void Browse();

    class Impl;
    std::unique_ptr<Impl> pathEdit_;
};

} // namespace ysDui::controls::input
