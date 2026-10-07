#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiTextInput.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

class DuiSpinBox final : public core::Control, public render::DuiRenderable {
public:
    DuiSpinBox(); ~DuiSpinBox() override;
    DuiSpinBox(const DuiSpinBox&) = delete; DuiSpinBox& operator=(const DuiSpinBox&) = delete;
    DuiSpinBox(DuiSpinBox&&) noexcept; DuiSpinBox& operator=(DuiSpinBox&&) noexcept;
    void SetRange(int minimum, int maximum); void SetStep(int step); void SetValue(int value, bool notify = false); void SetWrap(bool wrap);
    [[nodiscard]] int Value() const; [[nodiscard]] int Minimum() const; [[nodiscard]] int Maximum() const;
    [[nodiscard]] static int ClampOrWrap(int value, int minimum, int maximum, bool wrap);
    [[nodiscard]] static bool TryParseInt(std::string_view text, int& value);
    void SetValueChangedHandler(std::function<void(int)> handler);
    [[nodiscard]] core::Rect UpRect() const; [[nodiscard]] core::Rect DownRect() const;
    [[nodiscard]] core::Rect TextRect() const;
    /**
     * 获取微调框的默认建议尺寸。
     * @return 与旧版默认样式一致的 120 x 23 像素尺寸。
     */
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    void SetTextInput(ui::DuiTextInput* input);
    [[nodiscard]] ui::DuiTextInput* TextInput() const;
    void CommitTextInput(bool notify = true);
    bool OnEvent(const core::Event& event) override; void Paint(render::Canvas& canvas, core::Rect dirty) const override;
private: class Impl; std::unique_ptr<Impl> spinBox_;
};
} // namespace ysDui::controls::input
