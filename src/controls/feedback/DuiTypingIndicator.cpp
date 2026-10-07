/**
 * 文件名：DuiTypingIndicator.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途："正在输入"指示器的动画与绘制实现。
 */
#include "ysDui/controls/feedback/DuiTypingIndicator.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::feedback {
namespace {
/** 一个脉冲周期的时长（毫秒）。 */
constexpr int kPeriodMilliseconds = 1200;
/** 相邻点的相位错开比例：整周期被点数与错步共同分摊。 */
constexpr double kPhaseStep = 0.18;
/** 点的亮度下限（暗态）。 */
constexpr double kDimFactor = 0.3;
/** 无时钟时的静态亮度。 */
constexpr double kStaticFactor = 0.6;

struct CallbackState final
{
    DuiTypingIndicator* owner{};
};
} // namespace

class DuiTypingIndicator::Impl {
public:
    bool active{};
    int phase{};
    int dotCount{3};
    int dotRadius{3};
    int gap{5};
    core::Color color{140, 140, 148, 255};
    bool colorOverride{};
    core::AnimationClock* clock{};
    core::AnimationClock::TaskId task{};
    std::shared_ptr<CallbackState> callbackState;
};

DuiTypingIndicator::DuiTypingIndicator() : typing_(std::make_unique<Impl>())
{
    typing_->callbackState = std::make_shared<CallbackState>();
    typing_->callbackState->owner = this;
}

DuiTypingIndicator::~DuiTypingIndicator()
{
    SetActive(false);
    typing_->callbackState->owner = nullptr;
}
DuiTypingIndicator::DuiTypingIndicator(DuiTypingIndicator&&) noexcept = default;
DuiTypingIndicator& DuiTypingIndicator::operator=(DuiTypingIndicator&&) noexcept = default;

void DuiTypingIndicator::SetActive(bool active)
{
    if (typing_->active == active)
        return;
    typing_->active = active;
    if (!active && typing_->clock && typing_->task)
        typing_->clock->Cancel(typing_->task);
    typing_->task = {};
    if (active)
        ScheduleNextTick();
}
bool DuiTypingIndicator::Active() const { return typing_->active; }

void DuiTypingIndicator::SetAnimationClock(core::AnimationClock* clock)
{
    if (typing_->clock && typing_->task)
        typing_->clock->Cancel(typing_->task);
    typing_->clock = clock;
    typing_->task = {};
    if (typing_->active)
        ScheduleNextTick();
}

void DuiTypingIndicator::SetPhase(int milliseconds)
{
    typing_->phase = milliseconds % kPeriodMilliseconds;
    if (typing_->phase < 0)
        typing_->phase += kPeriodMilliseconds;
}
int DuiTypingIndicator::Phase() const { return typing_->phase; }

void DuiTypingIndicator::SetDotCount(int count) { typing_->dotCount = (std::max)(1, count); }
int DuiTypingIndicator::DotCount() const { return typing_->dotCount; }
void DuiTypingIndicator::SetDotRadius(int pixels) { typing_->dotRadius = (std::max)(1, pixels); }
int DuiTypingIndicator::DotRadius() const { return typing_->dotRadius; }
void DuiTypingIndicator::SetGap(int pixels) { typing_->gap = (std::max)(0, pixels); }
int DuiTypingIndicator::Gap() const { return typing_->gap; }
void DuiTypingIndicator::SetColor(core::Color color) { typing_->color = color; typing_->colorOverride = true; }
core::Color DuiTypingIndicator::Color() const
{
    return typing_->colorOverride ? typing_->color : Theme().Get(core::ThemeSlot::TextSubtle);
}

void DuiTypingIndicator::ScheduleNextTick()
{
    if (!typing_->active || !typing_->clock)
        return;
    const std::weak_ptr<CallbackState> callbackState = typing_->callbackState;
    typing_->task = typing_->clock->Schedule(kPeriodMilliseconds, [callbackState](double progress)
    {
        const std::shared_ptr<CallbackState> state = callbackState.lock();
        if (!state || !state->owner)
            return;
        state->owner->SetPhase(static_cast<int>(progress * kPeriodMilliseconds));
        if (progress >= 1.0)
        {
            state->owner->typing_->task = {};
            state->owner->ScheduleNextTick();
        }
    });
}

double DuiTypingIndicator::DotPulse(int index) const
{
    if (!typing_->active)
        return kStaticFactor;
    // 相位随点序号后移，形成波浪式的依次脉冲
    const double position = static_cast<double>(typing_->phase) / kPeriodMilliseconds;
    const double shifted = position - index * kPhaseStep;
    const double normalized = shifted - std::floor(shifted);
    // 半周期正弦：0 → 暗，0.5 → 亮
    const double wave = 0.5 + 0.5 * std::sin(normalized * 2.0 * 3.14159265358979323846);
    return kDimFactor + (1.0 - kDimFactor) * wave;
}

core::Size DuiTypingIndicator::DesiredSize() const
{
    const int diameter = typing_->dotRadius * 2;
    return {typing_->dotCount * diameter + (typing_->dotCount - 1) * typing_->gap, diameter};
}

void DuiTypingIndicator::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty())
        return;
    const core::Color base = Color();
    const int diameter = typing_->dotRadius * 2;
    int left = bounds.left;
    const int centerY = (bounds.top + bounds.bottom) / 2;
    for (int index = 0; index < typing_->dotCount; ++index)
    {
        const core::Rect dot{left, centerY - typing_->dotRadius, left + diameter,
                             centerY - typing_->dotRadius + diameter};
        // 亮度作用于 alpha：底色保持，仅透明度随脉冲变化
        core::Color color = base;
        color.alpha = static_cast<unsigned char>(
            (std::clamp)(static_cast<double>(base.alpha) * DotPulse(index), 0.0, 255.0));
        canvas.FillEllipse(dot, color);
        left += diameter + typing_->gap;
    }
}

core::DuiAccessibilityData DuiTypingIndicator::CreateAccessibilityData() const
{
    return {core::DuiAccessibilityRole::ProgressBar, "Typing", {}, {}, false, {}};
}

} // namespace ysDui::controls::feedback
