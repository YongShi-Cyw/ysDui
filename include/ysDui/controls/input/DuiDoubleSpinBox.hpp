#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiTextInput.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

class DuiDoubleSpinBox final : public core::Control, public render::DuiRenderable {
public:
    DuiDoubleSpinBox();
    ~DuiDoubleSpinBox() override;
    DuiDoubleSpinBox(const DuiDoubleSpinBox&) = delete;
    DuiDoubleSpinBox& operator=(const DuiDoubleSpinBox&) = delete;
    DuiDoubleSpinBox(DuiDoubleSpinBox&&) noexcept;
    DuiDoubleSpinBox& operator=(DuiDoubleSpinBox&&) noexcept;

    void SetRange(double minimum, double maximum);
    void SetStep(double step);
    void SetDecimals(int decimals);
    void SetWrap(bool wrap);
    void SetValue(double value, bool notify = false);
    [[nodiscard]] double Value() const;
    [[nodiscard]] double Minimum() const;
    [[nodiscard]] double Maximum() const;
    [[nodiscard]] int Decimals() const;
    [[nodiscard]] static double ClampOrWrap(double value, double minimum, double maximum, bool wrap);
    [[nodiscard]] static bool TryParseDouble(std::string_view text, double& value);
    void SetValueChangedHandler(std::function<void(double)> handler);
    [[nodiscard]] core::Rect UpRect() const;
    [[nodiscard]] core::Rect DownRect() const;
    [[nodiscard]] core::Rect TextRect() const;
    /**
     * 获取浮点微调框的默认建议尺寸。
     * @return 与旧版默认样式一致的 120 x 23 像素尺寸。
     */
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    void SetTextInput(ui::DuiTextInput* input);
    [[nodiscard]] ui::DuiTextInput* TextInput() const;
    void CommitTextInput(bool notify = true);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> doubleSpinBox_;
};

} // namespace ysDui::controls::input
