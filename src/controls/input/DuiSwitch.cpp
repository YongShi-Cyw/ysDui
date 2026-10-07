#include "ysDui/controls/input/DuiSwitch.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::input {
namespace {
constexpr int AnimationMilliseconds = 150;
constexpr int DefaultWidth = 46;
constexpr int DefaultHeight = 24;
constexpr int KnobInset = 4;
constexpr double DisabledOpacity = 0.4;

struct CallbackState final { DuiSwitch* owner{}; };
core::Color Blend(core::Color from, core::Color to, double value) {
    value = std::clamp(value, 0.0, 1.0);
    const auto blend = [value](int a, int b) { return static_cast<unsigned char>(a + (b - a) * value); };
    return {blend(from.red, to.red), blend(from.green, to.green), blend(from.blue, to.blue), blend(from.alpha, to.alpha)};
}

double EaseOutCubic(double value)
{
    const double remaining = 1.0 - std::clamp(value, 0.0, 1.0);
    return 1.0 - remaining * remaining * remaining;
}
}

class DuiSwitch::Impl {
public:
    bool checked{};
    bool animated{true};
    double progress{};
    core::AnimationClock* clock{};
    core::AnimationClock::TaskId task{};
    std::shared_ptr<CallbackState> callbackState;
    std::function<void(bool)> valueChangedHandler;
    core::Color onColor{7, 193, 96, 255};
    core::Color offColor{229, 229, 229, 255};
    core::Color knobColor{255, 255, 255, 255};
    bool onColorOverride{};
    bool offColorOverride{};
    bool knobColorOverride{};
};

DuiSwitch::DuiSwitch() : switch_(std::make_unique<Impl>()) { switch_->callbackState = std::make_shared<CallbackState>(); switch_->callbackState->owner = this; }
DuiSwitch::~DuiSwitch() {
    if (!switch_) return;
    if (switch_->clock && switch_->task) switch_->clock->Cancel(switch_->task);
    switch_->callbackState->owner = nullptr;
}
DuiSwitch::DuiSwitch(DuiSwitch&& other) noexcept
    : core::Control(std::move(other)), switch_(std::move(other.switch_)) {
    if (switch_) switch_->callbackState->owner = this;
}
DuiSwitch& DuiSwitch::operator=(DuiSwitch&& other) noexcept {
    if (this == &other) return *this;
    if (switch_) {
        if (switch_->clock && switch_->task) switch_->clock->Cancel(switch_->task);
        switch_->callbackState->owner = nullptr;
    }
    core::Control::operator=(std::move(other));
    switch_ = std::move(other.switch_);
    if (switch_) switch_->callbackState->owner = this;
    return *this;
}
void DuiSwitch::SetChecked(bool checked) { SetChecked(checked, true, false); }
void DuiSwitch::SetChecked(bool checked, bool animate, bool notify) {
    const double target = checked ? 1.0 : 0.0;
    const bool changed = switch_->checked != checked;
    if (!changed && (animate || switch_->progress == target)) return;
    if (switch_->clock && switch_->task) switch_->clock->Cancel(switch_->task);
    switch_->task = {};
    switch_->checked = checked;
    if (!animate || !switch_->clock || switch_->progress == target) switch_->progress = target;
    else {
        const double from = switch_->progress;
        const std::weak_ptr<CallbackState> state = switch_->callbackState;
        switch_->task = switch_->clock->Schedule(AnimationMilliseconds, [state, from, target](double fraction) {
            const auto locked = state.lock();
            if (!locked || !locked->owner) return;
            locked->owner->switch_->progress = from + (target - from) * EaseOutCubic(fraction);
            if (fraction >= 1.0) locked->owner->switch_->task = {};
        });
    }
    if (changed && notify && switch_->valueChangedHandler) switch_->valueChangedHandler(checked);
}
bool DuiSwitch::Checked() const { return switch_->checked; }
void DuiSwitch::SetAnimationClock(core::AnimationClock* clock) { if (switch_->clock && switch_->task) switch_->clock->Cancel(switch_->task); switch_->clock = clock; switch_->task = {}; }
void DuiSwitch::SetAnimated(bool animated) { switch_->animated = animated; }
bool DuiSwitch::Animated() const { return switch_->animated; }
double DuiSwitch::AnimationProgress() const { return switch_->progress; }
void DuiSwitch::SetValueChangedHandler(std::function<void(bool)> handler) { switch_->valueChangedHandler = std::move(handler); }
void DuiSwitch::SetOnColor(core::Color color) { switch_->onColor = color; switch_->onColorOverride = true; }
core::Color DuiSwitch::OnColor() const { return switch_->onColorOverride ? switch_->onColor : Theme().Get(core::ThemeSlot::ToggleOn); }
void DuiSwitch::SetOffColor(core::Color color) { switch_->offColor = color; switch_->offColorOverride = true; }
core::Color DuiSwitch::OffColor() const { return switch_->offColorOverride ? switch_->offColor : Theme().Get(core::ThemeSlot::ToggleOff); }
void DuiSwitch::SetKnobColor(core::Color color) { switch_->knobColor = color; switch_->knobColorOverride = true; }
core::Color DuiSwitch::KnobColor() const { return switch_->knobColorOverride ? switch_->knobColor : Theme().Get(core::ThemeSlot::SurfaceBackground); }
core::Size DuiSwitch::DesiredSize() const { return {DefaultWidth, DefaultHeight}; }
core::Rect DuiSwitch::ComputeKnobRect() const {
    const core::Rect bounds = Bounds();
    const int radius = bounds.Height() / 2;
    const int knobRadius = (std::max)(1, radius - KnobInset);
    const int side = knobRadius * 2 + 1;
    const int top = bounds.top + (bounds.Height() - side) / 2;
    const int leftCenter = bounds.left + KnobInset + knobRadius;
    const int rightCenter = (std::max)(leftCenter, bounds.right - KnobInset - knobRadius - 1);
    const int center = leftCenter + static_cast<int>((rightCenter - leftCenter) * switch_->progress);
    const int left = center - knobRadius;
    return {left, top, left + side, top + side};
}
core::DuiAccessibilityData DuiSwitch::CreateAccessibilityData() const {
    return {core::DuiAccessibilityRole::Switch, {}, switch_->checked ? "true" : "false", {}, false, {}};
}
bool DuiSwitch::OnEvent(const core::Event& event) {
    if (!Enabled()) return false;
    if (event.type == core::EventType::KeyDown
        && (event.key == core::key::Space || event.key == core::key::Enter)) {
        SetChecked(!switch_->checked, switch_->animated, true);
        return true;
    }
    const bool contains = Bounds().Contains(event.position);
    if (event.type == core::EventType::PointerMove) {
        SetHovered(contains);
        return Captured();
    }
    if (event.type == core::EventType::PointerDown && contains) {
        SetHovered(true);
        SetCaptured(true);
        return true;
    }
    if (event.type == core::EventType::PointerCancel && Captured()) {
        SetCaptured(false);
        SetHovered(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && Captured()) {
        SetCaptured(false);
        SetHovered(contains);
        if (contains) {
            SetChecked(!switch_->checked, switch_->animated, true);
        }
        return true;
    }
    return false;
}
void DuiSwitch::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible()) return;
    const core::Rect bounds = Bounds();
    const core::Rect clip = core::Rect::Intersect(bounds, dirty);
    if (clip.Empty()) return;
    const core::Color disabledSurface = Theme().Get(core::ThemeSlot::SurfaceBackground);
    core::Color trackColor = Blend(OffColor(), OnColor(), switch_->progress);
    core::Color knobColor = KnobColor();
    core::Color knobBorder = Theme().Get(core::ThemeSlot::SwitchKnobBorder);
    if (!Enabled()) {
        trackColor = Blend(disabledSurface, trackColor, DisabledOpacity);
        knobColor = Blend(disabledSurface, knobColor, DisabledOpacity);
        knobBorder = Blend(disabledSurface, knobBorder, DisabledOpacity);
    }
    canvas.PushClip(clip);
    canvas.FillRoundedRect(bounds, bounds.Height() / 2, trackColor);
    const core::Rect knob = ComputeKnobRect();
    canvas.FillEllipse(knob, knobColor);
    canvas.StrokeArc(knob, 0.0F, 360.0F, knobBorder, 1.0F);
    canvas.PopClip();
}

} // namespace ysDui::controls::input
