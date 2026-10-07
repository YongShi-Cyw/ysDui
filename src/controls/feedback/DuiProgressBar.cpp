#include "ysDui/controls/feedback/DuiProgressBar.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::feedback {

namespace {
struct DuiProgressBarCallbackState final
{
    DuiProgressBar* owner{};
};
}

class DuiProgressBar::Impl {
public:
    int minimum{};
    int maximum{100};
    int value{};
    std::string text;
    bool vertical{};
    bool marquee{};
    bool marqueeReverse{};
    int marqueePhase{};
    core::AnimationClock* clock{};
    core::AnimationClock::TaskId task{};
    std::shared_ptr<DuiProgressBarCallbackState> callbackState;
    core::Color background{232, 232, 236, 255};
    core::Color fill{45, 108, 223, 255};
    bool backgroundOverride{};
    bool fillOverride{};
};

namespace {
int normalizePhase(int phase)
{
    phase %= DuiProgressBar::MarqueePeriod;
    return phase < 0 ? phase + DuiProgressBar::MarqueePeriod : phase;
}
} // namespace

DuiProgressBar::DuiProgressBar() : progress_(std::make_unique<Impl>())
{
    progress_->callbackState = std::make_shared<DuiProgressBarCallbackState>();
    progress_->callbackState->owner = this;
}
DuiProgressBar::~DuiProgressBar()
{
    progress_->callbackState->owner = nullptr;
    CancelMarqueeAnimation();
}
void DuiProgressBar::SetRange(int minimum, int maximum) {
    if (minimum >= maximum) maximum = minimum + 1;
    progress_->minimum = minimum;
    progress_->maximum = maximum;
    SetValue(progress_->value);
}
int DuiProgressBar::Minimum() const { return progress_->minimum; }
int DuiProgressBar::Maximum() const { return progress_->maximum; }
void DuiProgressBar::SetValue(int value) { progress_->value = std::clamp(value, progress_->minimum, progress_->maximum); }
int DuiProgressBar::Value() const { return progress_->value; }
void DuiProgressBar::SetText(std::string text) { progress_->text = std::move(text); }
const std::string& DuiProgressBar::Text() const { return progress_->text; }
void DuiProgressBar::SetVertical(bool vertical) { progress_->vertical = vertical; }
bool DuiProgressBar::Vertical() const { return progress_->vertical; }
void DuiProgressBar::SetMarquee(bool marquee)
{
    progress_->marquee = marquee;
    if (marquee)
        StartMarqueeAnimation();
    else
        CancelMarqueeAnimation();
}
bool DuiProgressBar::Marquee() const { return progress_->marquee; }
void DuiProgressBar::SetAnimationClock(core::AnimationClock* clock)
{
    if (progress_->clock == clock)
        return;
    CancelMarqueeAnimation();
    progress_->clock = clock;
    StartMarqueeAnimation();
}
void DuiProgressBar::SetMarqueePhase(int phase) { progress_->marqueePhase = normalizePhase(phase); }
int DuiProgressBar::MarqueePhase() const { return progress_->marqueePhase; }
void DuiProgressBar::SetBackgroundColor(core::Color color) { progress_->background = color; progress_->backgroundOverride = true; }
void DuiProgressBar::SetFillColor(core::Color color) { progress_->fill = color; progress_->fillOverride = true; }

void DuiProgressBar::CancelMarqueeAnimation()
{
    if (progress_->clock != nullptr && progress_->task != 0)
        progress_->clock->Cancel(progress_->task);
    progress_->task = {};
}

void DuiProgressBar::StartMarqueeAnimation()
{
    if (!progress_->marquee || progress_->clock == nullptr || progress_->task != 0)
        return;
    const std::weak_ptr<DuiProgressBarCallbackState> callbackState = progress_->callbackState;
    progress_->task = progress_->clock->Schedule(MarqueePeriod, [callbackState](double progress)
    {
        const auto state = callbackState.lock();
        if (!state || state->owner == nullptr)
            return;
        int phase = static_cast<int>(progress * MarqueePeriod);
        if (state->owner->progress_->marqueeReverse)
            phase = MarqueePeriod - phase;
        state->owner->SetMarqueePhase((std::clamp)(phase, 0, MarqueePeriod - 1));
        if (progress >= 1.0)
        {
            state->owner->progress_->marqueeReverse = !state->owner->progress_->marqueeReverse;
            state->owner->progress_->task = {};
            state->owner->StartMarqueeAnimation();
        }
    });
}

core::Rect DuiProgressBar::ComputeFillRect() const {
    const core::Rect bounds = Bounds();
    if (bounds.Empty()) return {};
    const int total = progress_->vertical ? bounds.Height() : bounds.Width();
    const int filled = total * (progress_->value - progress_->minimum) / (progress_->maximum - progress_->minimum);
    return progress_->vertical ? core::Rect{bounds.left, bounds.top, bounds.right, bounds.top + filled}
                               : core::Rect{bounds.left, bounds.top, bounds.left + filled, bounds.bottom};
}
core::Rect DuiProgressBar::ComputeMarqueeRect() const {
    const core::Rect bounds = Bounds();
    if (bounds.Empty()) return {};
    const int total = progress_->vertical ? bounds.Height() : bounds.Width();
    const int block = std::min(MarqueeBlockPixels, total);
    const int offset = std::max(0, total - block) * progress_->marqueePhase / MarqueePeriod;
    return progress_->vertical ? core::Rect{bounds.left, bounds.top + offset, bounds.right, bounds.top + offset + block}
                               : core::Rect{bounds.left + offset, bounds.top, bounds.left + offset + block, bounds.bottom};
}
core::DuiAccessibilityData DuiProgressBar::CreateAccessibilityData() const {
    const std::string value = std::to_string(progress_->value);
    return {core::DuiAccessibilityRole::ProgressBar, {}, {value.begin(), value.end()}, {}, false, {}};
}
void DuiProgressBar::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible()) return;
    const core::Rect clipped = core::Rect::Intersect(Bounds(), dirty);
    if (clipped.Empty()) return;
    const core::Color background = progress_->backgroundOverride ? progress_->background
        : Theme().Get(core::ThemeSlot::ProgressTrack);
    const core::Color fill = progress_->fillOverride ? progress_->fill
        : Theme().Get(core::ThemeSlot::BrandPrimary);
    canvas.FillRect(clipped, background);
    const core::Rect progressRect = progress_->marquee ? ComputeMarqueeRect() : ComputeFillRect();
    const core::Rect progressClipped = core::Rect::Intersect(progressRect, dirty);
    if (!progressClipped.Empty()) canvas.FillRect(progressClipped, fill);
    if (!progress_->marquee)
    {
        const std::string text = progress_->text.empty()
            ? std::to_string((progress_->value - progress_->minimum) * 100 / (progress_->maximum - progress_->minimum)) + "%"
            : progress_->text;
        const render::DuiTextStyle darkText{Theme().Get(core::ThemeSlot::ProgressText), {}, 9, true};
        const render::DuiTextStyle lightText{Theme().Get(core::ThemeSlot::TextOnPrimary), {}, 9, true};
        canvas.DrawText(text, clipped, darkText, render::DuiTextAlignment::Center, false);
        if (!progressClipped.Empty())
        {
            canvas.PushClip(progressClipped);
            canvas.DrawText(text, clipped, lightText, render::DuiTextAlignment::Center, false);
            canvas.PopClip();
        }
    }
}

} // namespace ysDui::controls::feedback
