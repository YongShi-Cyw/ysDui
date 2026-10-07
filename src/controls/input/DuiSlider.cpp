#include "ysDui/controls/input/DuiSlider.hpp"
#include <algorithm>
#include <utility>
#include "ysDui/core/DuiTheme.hpp"
namespace ysDui::controls::input {
class DuiSlider::Impl { public: int minimum{}; int maximum{100}; int value{}; int lineSize{1}; bool vertical{}; bool dragging{}; std::function<void(int)> changed; };
DuiSlider::DuiSlider() : slider_(std::make_unique<Impl>()) {}
DuiSlider::~DuiSlider() = default;
DuiSlider::DuiSlider(DuiSlider&&) noexcept = default;
DuiSlider& DuiSlider::operator=(DuiSlider&&) noexcept = default;
void DuiSlider::SetRange(int minimum, int maximum) { slider_->minimum = minimum; slider_->maximum = (std::max)(minimum, maximum); SetValue(slider_->value); }
void DuiSlider::SetValue(int value, bool notify) {
    const int clamped = (std::clamp)(value, slider_->minimum, slider_->maximum);
    const int offset = clamped - slider_->minimum;
    const int snapped = slider_->minimum + (offset + slider_->lineSize / 2) / slider_->lineSize * slider_->lineSize;
    const int next = (std::clamp)(snapped, slider_->minimum, slider_->maximum);
    if (next == slider_->value) return;
    slider_->value = next;
    if (notify && slider_->changed) slider_->changed(next);
}
int DuiSlider::Minimum() const { return slider_->minimum; } int DuiSlider::Maximum() const { return slider_->maximum; } int DuiSlider::Value() const { return slider_->value; }
void DuiSlider::SetLineSize(int value) { slider_->lineSize = (std::max)(1, value); } void DuiSlider::SetVertical(bool vertical) { slider_->vertical = vertical; }
void DuiSlider::SetValueChangedHandler(std::function<void(int)> handler) { slider_->changed = std::move(handler); }
core::Rect DuiSlider::TrackRect() const { const auto b = Bounds(); return slider_->vertical ? core::Rect{b.left + b.Width() / 2 - 2, b.top + 7, b.left + b.Width() / 2 + 2, b.bottom - 7} : core::Rect{b.left + 7, b.top + b.Height() / 2 - 2, b.right - 7, b.top + b.Height() / 2 + 2}; }
core::Point DuiSlider::ThumbCenter() const { const auto t = TrackRect(); const int span = slider_->vertical ? t.Height() : t.Width(); const int range = slider_->maximum - slider_->minimum; const int offset = range == 0 ? 0 : span * (slider_->value - slider_->minimum) / range; return slider_->vertical ? core::Point{(t.left + t.right) / 2, t.top + offset} : core::Point{t.left + offset, (t.top + t.bottom) / 2}; }
int DuiSlider::ValueFromPoint(core::Point point) const { const auto t = TrackRect(); const int span = slider_->vertical ? t.Height() : t.Width(); if (span <= 0) return slider_->minimum; const int coordinate = slider_->vertical ? point.y : point.x; const int start = slider_->vertical ? t.top : t.left; return (std::clamp)(slider_->minimum + (slider_->maximum - slider_->minimum) * (std::clamp)(coordinate - start, 0, span) / span, slider_->minimum, slider_->maximum); }
core::DuiAccessibilityData DuiSlider::CreateAccessibilityData() const { const std::string value = std::to_string(slider_->value); return {core::DuiAccessibilityRole::Slider, {}, {value.begin(), value.end()}, {}, false, {}}; }
bool DuiSlider::OnEvent(const core::Event& event) { if (!Enabled()) return false; if (event.type == core::EventType::PointerDown && Bounds().Contains(event.position)) { slider_->dragging = true; SetCaptured(true); SetValue(ValueFromPoint(event.position), true); return true; } if (event.type == core::EventType::PointerMove && slider_->dragging) { SetValue(ValueFromPoint(event.position), true); return true; } if (event.type == core::EventType::PointerCancel && slider_->dragging) { slider_->dragging = false; SetCaptured(false); return true; } if (event.type == core::EventType::PointerUp && slider_->dragging) { slider_->dragging = false; SetCaptured(false); return true; } if (event.type == core::EventType::PointerWheel) { SetValue(slider_->value + (event.wheelDelta > 0 ? slider_->lineSize : -slider_->lineSize), true); return true; } if (event.type == core::EventType::KeyDown) { if (event.key == core::key::Left || event.key == core::key::Down) { SetValue(slider_->value - slider_->lineSize, true); return true; } if (event.key == core::key::Right || event.key == core::key::Up) { SetValue(slider_->value + slider_->lineSize, true); return true; } } return false; }
void DuiSlider::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const auto track = core::Rect::Intersect(TrackRect(), dirty);
    if (!EffectivelyVisible() || track.Empty()) return;
    const core::Color trackColor = Enabled() ? Theme().Get(core::ThemeSlot::SliderTrack)
                                              : Theme().Get(core::ThemeSlot::ProgressTrack);
    const core::Color fillColor = Enabled() ? Theme().Get(core::ThemeSlot::BrandPrimary)
                                             : Theme().Get(core::ThemeSlot::ControlBorder);
    const core::Color thumbColor = Enabled() ? Theme().Get(core::ThemeSlot::SurfaceBackground)
                                              : Theme().Get(core::ThemeSlot::SliderDisabledThumb);
    const core::Color borderColor = Enabled() ? Theme().Get(core::ThemeSlot::SliderBorder)
                                               : Theme().Get(core::ThemeSlot::SliderDisabledBorder);
    canvas.FillRoundedRect(track, 2, trackColor);
    const auto thumb = ThumbCenter();
    auto fill = track;
    if (slider_->vertical)
        fill.bottom = thumb.y;
    else
        fill.right = thumb.x;
    if (!fill.Empty()) canvas.FillRoundedRect(fill, 2, fillColor);
    const core::Rect thumbBounds{thumb.x - 7, thumb.y - 7, thumb.x + 8, thumb.y + 8};
    canvas.FillEllipse(thumbBounds, thumbColor);
    canvas.StrokeRoundedRect(thumbBounds, 7, borderColor, 1.0F);
}
} // namespace ysDui::controls::input
