#pragma once

#include <memory>
#include <string>

#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::feedback {

class DuiProgressBar final : public core::Control, public render::DuiRenderable {
public:
    static constexpr int MarqueeBlockPixels = 60;
    static constexpr int MarqueePeriod = 1200;

    DuiProgressBar();
    ~DuiProgressBar() override;
    DuiProgressBar(const DuiProgressBar&) = delete;
    DuiProgressBar& operator=(const DuiProgressBar&) = delete;
    DuiProgressBar(DuiProgressBar&&) = delete;
    DuiProgressBar& operator=(DuiProgressBar&&) = delete;

    void SetRange(int minimum, int maximum);
    [[nodiscard]] int Minimum() const;
    [[nodiscard]] int Maximum() const;
    void SetValue(int value);
    [[nodiscard]] int Value() const;
    void SetText(std::string text);
    [[nodiscard]] const std::string& Text() const;
    void SetVertical(bool vertical);
    [[nodiscard]] bool Vertical() const;
    void SetMarquee(bool marquee);
    [[nodiscard]] bool Marquee() const;
    void SetAnimationClock(core::AnimationClock* clock);
    void SetMarqueePhase(int phase);
    [[nodiscard]] int MarqueePhase() const;
    void SetBackgroundColor(core::Color color);
    void SetFillColor(core::Color color);
    [[nodiscard]] core::Rect ComputeFillRect() const;
    [[nodiscard]] core::Rect ComputeMarqueeRect() const;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    void StartMarqueeAnimation();
    void CancelMarqueeAnimation();

    class Impl;
    std::unique_ptr<Impl> progress_;
};

} // namespace ysDui::controls::feedback
