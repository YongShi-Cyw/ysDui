#pragma once

#include <functional>
#include <memory>

#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

class DuiSwitch final : public core::Control, public render::DuiRenderable {
public:
    DuiSwitch();
    ~DuiSwitch() override;
    DuiSwitch(const DuiSwitch&) = delete;
    DuiSwitch& operator=(const DuiSwitch&) = delete;
    DuiSwitch(DuiSwitch&&) noexcept;
    DuiSwitch& operator=(DuiSwitch&&) noexcept;

    void SetChecked(bool checked);
    void SetChecked(bool checked, bool animate, bool notify);
    [[nodiscard]] bool Checked() const;
    void SetAnimationClock(core::AnimationClock* clock);
    void SetAnimated(bool animated);
    [[nodiscard]] bool Animated() const;
    [[nodiscard]] double AnimationProgress() const;
    void SetValueChangedHandler(std::function<void(bool)> handler);
    void SetOnColor(core::Color color);
    /** @return 开启状态的轨道颜色。 */
    [[nodiscard]] core::Color OnColor() const;
    void SetOffColor(core::Color color);
    /** @return 关闭状态的轨道颜色。 */
    [[nodiscard]] core::Color OffColor() const;
    void SetKnobColor(core::Color color);
    /** @return 滑块颜色。 */
    [[nodiscard]] core::Color KnobColor() const;
    /** @return 96 DPI 下与旧控件一致的默认尺寸。 */
    [[nodiscard]] core::Size DesiredSize() const override;
    [[nodiscard]] core::Rect ComputeKnobRect() const;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    class Impl;
    std::unique_ptr<Impl> switch_;
};

} // namespace ysDui::controls::input
