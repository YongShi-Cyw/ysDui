#include "ysDui/controls/feedback/DuiBusyIndicator.hpp"

#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::feedback {
namespace {
constexpr int RotationMilliseconds = 1000;

struct CallbackState final
{
    DuiBusyIndicator* owner{};
};
}

class DuiBusyIndicator::Impl {
public:
    bool active{};
    int phase{};
    core::AnimationClock* clock{};
    core::AnimationClock::TaskId task{};
    std::shared_ptr<CallbackState> callbackState;
};

DuiBusyIndicator::DuiBusyIndicator() : busy_(std::make_unique<Impl>())
{
    busy_->callbackState = std::make_shared<CallbackState>();
    busy_->callbackState->owner = this;
}

DuiBusyIndicator::~DuiBusyIndicator()
{
    SetActive(false);
    busy_->callbackState->owner = nullptr;
}
DuiBusyIndicator::DuiBusyIndicator(DuiBusyIndicator&&) noexcept = default;
DuiBusyIndicator& DuiBusyIndicator::operator=(DuiBusyIndicator&&) noexcept = default;
void DuiBusyIndicator::SetActive(bool active)
{
    if (busy_->active == active)
        return;
    busy_->active = active;
    if (!active && busy_->clock && busy_->task)
        busy_->clock->Cancel(busy_->task);
    busy_->task = {};
    if (active)
        ScheduleNextRotation();
}
bool DuiBusyIndicator::Active() const { return busy_->active; }
void DuiBusyIndicator::SetAnimationClock(core::AnimationClock* clock)
{
    if (busy_->clock && busy_->task)
        busy_->clock->Cancel(busy_->task);
    busy_->clock = clock;
    busy_->task = {};
    if (busy_->active)
        ScheduleNextRotation();
}
void DuiBusyIndicator::SetPhase(int milliseconds) { busy_->phase = milliseconds % 1000; if (busy_->phase < 0) busy_->phase += 1000; }
void DuiBusyIndicator::ScheduleNextRotation()
{
    if (!busy_->active || !busy_->clock)
        return;
    const std::weak_ptr<CallbackState> callbackState = busy_->callbackState;
    busy_->task = busy_->clock->Schedule(RotationMilliseconds, [callbackState](double progress)
    {
        const std::shared_ptr<CallbackState> state = callbackState.lock();
        if (!state || !state->owner)
            return;
        state->owner->SetPhase(static_cast<int>(progress * RotationMilliseconds));
        if (progress >= 1.0)
        {
            state->owner->busy_->task = {};
            state->owner->ScheduleNextRotation();
        }
    });
}
core::Size DuiBusyIndicator::DesiredSize() const { return {24, 24}; }
void DuiBusyIndicator::Paint(render::Canvas& canvas, core::Rect dirty) const {
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty()) return;
    canvas.StrokeArc(bounds, 0, 360, Theme().Get(core::ThemeSlot::BusyTrack), 2.0f);
    if (busy_->active) canvas.StrokeArc(bounds, static_cast<float>(busy_->phase) * 0.36f, 270,
                                        Theme().Get(core::ThemeSlot::BusyIndicator), 2.0f);
}
} // namespace ysDui::controls::feedback
