#pragma once

#include <functional>
#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

class DuiSlider final : public core::Control, public render::DuiRenderable {
public:
    DuiSlider();
    ~DuiSlider() override;
    DuiSlider(const DuiSlider&) = delete;
    DuiSlider& operator=(const DuiSlider&) = delete;
    DuiSlider(DuiSlider&&) noexcept;
    DuiSlider& operator=(DuiSlider&&) noexcept;
    void SetRange(int minimum, int maximum);
    void SetValue(int value, bool notify = false);
    [[nodiscard]] int Minimum() const;
    [[nodiscard]] int Maximum() const;
    [[nodiscard]] int Value() const;
    void SetLineSize(int value);
    void SetVertical(bool vertical);
    void SetValueChangedHandler(std::function<void(int)> handler);
    [[nodiscard]] core::Rect TrackRect() const;
    [[nodiscard]] core::Point ThumbCenter() const;
    [[nodiscard]] int ValueFromPoint(core::Point point) const;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    class Impl;
    std::unique_ptr<Impl> slider_;
};

} // namespace ysDui::controls::input
