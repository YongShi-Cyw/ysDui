#include "ysDui/controls/input/DuiRangeSlider.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::input {
namespace {
constexpr int ThumbRadius = 7;      // 拇指半径
constexpr int ThumbGrabRadius = 10; // 拇指命中半径（略大于绘制半径，便于点中）
constexpr int TrackInset = 7;       // 轨道相对控件边缘的内缩
constexpr int TrackThickness = 4;   // 轨道厚度
} // namespace

class DuiRangeSlider::Impl {
public:
    int minimum{};
    int maximum{100};
    int low{};
    int high{100};
    int lineSize{1};
    bool vertical{};
    bool dragging{};
    DuiRangeThumb active{DuiRangeThumb::None};
    std::function<void(int, int)> changed;

    /** 按 lineSize 就近吸附并钳制到 [minimum, maximum]。 */
    [[nodiscard]] int Snap(int value) const
    {
        const int clamped = (std::clamp)(value, minimum, maximum);
        const int offset = clamped - minimum;
        const int snapped = minimum + (offset + lineSize / 2) / lineSize * lineSize;
        return (std::clamp)(snapped, minimum, maximum);
    }
};

DuiRangeSlider::DuiRangeSlider() : range_(std::make_unique<Impl>()) {}
DuiRangeSlider::~DuiRangeSlider() = default;
DuiRangeSlider::DuiRangeSlider(DuiRangeSlider&&) noexcept = default;
DuiRangeSlider& DuiRangeSlider::operator=(DuiRangeSlider&&) noexcept = default;

void DuiRangeSlider::SetRange(int minimum, int maximum)
{
    if (minimum > maximum)
        std::swap(minimum, maximum);
    range_->minimum = minimum;
    range_->maximum = maximum;
    SetValues(range_->low, range_->high, false);
}
int DuiRangeSlider::Minimum() const { return range_->minimum; }
int DuiRangeSlider::Maximum() const { return range_->maximum; }

void DuiRangeSlider::SetValues(int low, int high, bool notify)
{
    int nextLow = range_->Snap(low);
    int nextHigh = range_->Snap(high);
    if (nextLow > nextHigh)
        std::swap(nextLow, nextHigh);
    if (nextLow == range_->low && nextHigh == range_->high)
        return;
    range_->low = nextLow;
    range_->high = nextHigh;
    if (notify && range_->changed)
        range_->changed(range_->low, range_->high);
}
int DuiRangeSlider::Low() const { return range_->low; }
int DuiRangeSlider::High() const { return range_->high; }

void DuiRangeSlider::SetLineSize(int value) { range_->lineSize = (std::max)(1, value); }
int DuiRangeSlider::LineSize() const { return range_->lineSize; }
void DuiRangeSlider::SetVertical(bool vertical) { range_->vertical = vertical; }
bool DuiRangeSlider::Vertical() const { return range_->vertical; }
void DuiRangeSlider::SetValuesChangedHandler(std::function<void(int, int)> handler) { range_->changed = std::move(handler); }

int DuiRangeSlider::TrackSpan() const
{
    const core::Rect track = TrackRect();
    return (std::max)(0, range_->vertical ? track.Height() : track.Width());
}

core::Rect DuiRangeSlider::TrackRect() const
{
    const core::Rect bounds = Bounds();
    if (range_->vertical)
    {
        return {bounds.left + bounds.Width() / 2 - TrackThickness / 2, bounds.top + TrackInset,
                bounds.left + bounds.Width() / 2 + TrackThickness / 2, bounds.bottom - TrackInset};
    }
    return {bounds.left + TrackInset, bounds.top + bounds.Height() / 2 - TrackThickness / 2,
            bounds.right - TrackInset, bounds.top + bounds.Height() / 2 + TrackThickness / 2};
}

core::Point DuiRangeSlider::ThumbCenter(DuiRangeThumb thumb) const
{
    const core::Rect track = TrackRect();
    const int span = TrackSpan();
    const int range = range_->maximum - range_->minimum;
    const int value = thumb == DuiRangeThumb::High ? range_->high : range_->low;
    const int offset = range == 0 ? 0 : span * (value - range_->minimum) / range;
    if (range_->vertical)
        return {track.left + track.Width() / 2, track.top + offset};
    return {track.left + offset, track.top + track.Height() / 2};
}

DuiRangeThumb DuiRangeSlider::ActiveThumb() const { return range_->active; }

int DuiRangeSlider::ValueFromPoint(core::Point point) const
{
    const core::Rect track = TrackRect();
    const int span = TrackSpan();
    if (span <= 0)
        return range_->minimum;
    const int coordinate = range_->vertical ? point.y : point.x;
    const int start = range_->vertical ? track.top : track.left;
    const int clamped = (std::clamp)(coordinate - start, 0, span);
    return (std::clamp)(range_->minimum + (range_->maximum - range_->minimum) * clamped / span,
                        range_->minimum, range_->maximum);
}

DuiRangeThumb DuiRangeSlider::ThumbFromPoint(core::Point point) const
{
    const core::Point low = ThumbCenter(DuiRangeThumb::Low);
    const core::Point high = ThumbCenter(DuiRangeThumb::High);
    const auto distanceSquared = [](core::Point left, core::Point right)
    {
        const int dx = left.x - right.x;
        const int dy = left.y - right.y;
        return dx * dx + dy * dy;
    };
    constexpr int grabSquared = ThumbGrabRadius * ThumbGrabRadius;
    const int lowSquared = distanceSquared(point, low);
    const int highSquared = distanceSquared(point, high);
    const bool lowHit = lowSquared <= grabSquared;
    const bool highHit = highSquared <= grabSquared;
    if (lowHit && highHit)
        return lowSquared <= highSquared ? DuiRangeThumb::Low : DuiRangeThumb::High;
    if (lowHit)
        return DuiRangeThumb::Low;
    if (highHit)
        return DuiRangeThumb::High;
    return DuiRangeThumb::None;
}

core::Size DuiRangeSlider::DesiredSize() const { return {160, 20}; }

bool DuiRangeSlider::OnEvent(const core::Event& event)
{
    if (!Enabled())
        return false;

    if (event.type == core::EventType::PointerDown && Bounds().Contains(event.position))
    {
        DuiRangeThumb thumb = ThumbFromPoint(event.position);
        if (thumb == DuiRangeThumb::None)
        {
            // 未点中拇指：按就近原则吸附到最近端，并把该端拖到按下位置
            const int value = ValueFromPoint(event.position);
            thumb = (std::abs(value - range_->low) <= std::abs(value - range_->high))
                ? DuiRangeThumb::Low : DuiRangeThumb::High;
            if (thumb == DuiRangeThumb::Low)
                SetValues(value, range_->high, true);
            else
                SetValues(range_->low, value, true);
        }
        range_->active = thumb;
        range_->dragging = true;
        SetCaptured(true);
        return true;
    }
    if (event.type == core::EventType::PointerMove && range_->dragging)
    {
        const int value = ValueFromPoint(event.position);
        if (range_->active == DuiRangeThumb::High)
            SetValues(range_->low, value, true);
        else
            SetValues(value, range_->high, true);
        return true;
    }
    if ((event.type == core::EventType::PointerUp || event.type == core::EventType::PointerCancel)
        && range_->dragging)
    {
        range_->dragging = false;
        range_->active = DuiRangeThumb::None;
        SetCaptured(false);
        return true;
    }
    if (event.type == core::EventType::PointerWheel)
    {
        // 滚轮作用于最近端，避免需要先点选拇指
        const int value = ValueFromPoint(event.position);
        const bool lowNearer = std::abs(value - range_->low) <= std::abs(value - range_->high);
        const int step = event.wheelDelta > 0 ? range_->lineSize : -range_->lineSize;
        if (lowNearer)
            SetValues(range_->low + step, range_->high, true);
        else
            SetValues(range_->low, range_->high + step, true);
        return true;
    }
    if (event.type == core::EventType::KeyDown)
    {
        const DuiRangeThumb target = range_->active != DuiRangeThumb::None
            ? range_->active : DuiRangeThumb::Low;
        if (event.key == core::key::Left || event.key == core::key::Down)
        {
            if (target == DuiRangeThumb::Low)
                SetValues(range_->low - range_->lineSize, range_->high, true);
            else
                SetValues(range_->low, range_->high - range_->lineSize, true);
            return true;
        }
        if (event.key == core::key::Right || event.key == core::key::Up)
        {
            if (target == DuiRangeThumb::Low)
                SetValues(range_->low + range_->lineSize, range_->high, true);
            else
                SetValues(range_->low, range_->high + range_->lineSize, true);
            return true;
        }
    }
    return false;
}

void DuiRangeSlider::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::Rect track = core::Rect::Intersect(TrackRect(), dirty);
    if (track.Empty())
        return;

    const bool enabled = Enabled();
    const core::Color trackColor = enabled ? Theme().Get(core::ThemeSlot::SliderTrack)
                                           : Theme().Get(core::ThemeSlot::ProgressTrack);
    const core::Color fillColor = enabled ? Theme().Get(core::ThemeSlot::BrandPrimary)
                                          : Theme().Get(core::ThemeSlot::ControlBorder);
    const core::Color thumbColor = enabled ? Theme().Get(core::ThemeSlot::SurfaceBackground)
                                           : Theme().Get(core::ThemeSlot::SliderDisabledThumb);
    const core::Color borderColor = enabled ? Theme().Get(core::ThemeSlot::SliderBorder)
                                            : Theme().Get(core::ThemeSlot::SliderDisabledBorder);

    canvas.FillRoundedRect(track, TrackThickness / 2, trackColor);

    // 区间填充：两个拇指之间
    const core::Point low = ThumbCenter(DuiRangeThumb::Low);
    const core::Point high = ThumbCenter(DuiRangeThumb::High);
    core::Rect selection = TrackRect();
    if (range_->vertical)
    {
        selection.top = low.y;
        selection.bottom = high.y;
    }
    else
    {
        selection.left = low.x;
        selection.right = high.x;
    }
    const core::Rect clippedSelection = core::Rect::Intersect(selection, dirty);
    if (!clippedSelection.Empty())
        canvas.FillRoundedRect(clippedSelection, TrackThickness / 2, fillColor);

    // 两个拇指：后画被按住的，保证其高亮压在上层
    const auto paintThumb = [&](core::Point center)
    {
        const core::Rect thumbBounds{center.x - ThumbRadius, center.y - ThumbRadius,
                                     center.x + ThumbRadius + 1, center.y + ThumbRadius + 1};
        if (core::Rect::Intersect(thumbBounds, dirty).Empty())
            return;
        canvas.FillEllipse(thumbBounds, thumbColor);
        canvas.StrokeRoundedRect(thumbBounds, ThumbRadius, borderColor, 1.0F);
    };
    if (range_->active == DuiRangeThumb::High)
    {
        paintThumb(low);
        paintThumb(high);
    }
    else
    {
        paintThumb(high);
        paintThumb(low);
    }
}

core::DuiAccessibilityData DuiRangeSlider::CreateAccessibilityData() const
{
    const std::string value = std::to_string(range_->low) + " - " + std::to_string(range_->high);
    return {core::DuiAccessibilityRole::Slider, {}, value, {}, false, {}};
}

} // namespace ysDui::controls::input
