#include "ysDui/controls/basic/DuiSegmentedControl.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiFocusVisual.hpp"

namespace ysDui::controls::basic {
namespace {
constexpr int DefaultHeight = 28;
constexpr int MinimumSegmentWidth = 48;
}

class DuiSegmentedControl::Impl {
public:
    std::vector<std::string> segments;
    std::vector<core::Rect> segmentBounds;
    std::function<void(int)> selectionChanged;
    render::DuiTextStyle textStyle{{60, 60, 70, 255}, {}, 9, false};
    core::Color background{240, 242, 246, 255};
    core::Color selected{45, 108, 223, 255};
    core::Color text{60, 60, 70, 255};
    core::Color selectedText{255, 255, 255, 255};
    bool textStyleOverride{};
    bool backgroundOverride{};
    bool selectedOverride{};
    bool textOverride{};
    bool selectedTextOverride{};
    int selectedIndex{-1};
};

DuiSegmentedControl::DuiSegmentedControl() : segmented_(std::make_unique<Impl>()) {}
DuiSegmentedControl::~DuiSegmentedControl() = default;
DuiSegmentedControl::DuiSegmentedControl(DuiSegmentedControl&&) noexcept = default;
DuiSegmentedControl& DuiSegmentedControl::operator=(DuiSegmentedControl&&) noexcept = default;
void DuiSegmentedControl::AddSegment(std::string text) {
    segmented_->segments.push_back(std::move(text));
    if (segmented_->selectedIndex < 0) segmented_->selectedIndex = 0;
    Layout(Bounds());
}
void DuiSegmentedControl::ClearSegments() { segmented_->segments.clear(); segmented_->segmentBounds.clear(); segmented_->selectedIndex = -1; }
int DuiSegmentedControl::SegmentCount() const { return static_cast<int>(segmented_->segments.size()); }
std::string DuiSegmentedControl::SegmentText(int index) const { return index >= 0 && index < SegmentCount() ? segmented_->segments[index] : std::string{}; }
void DuiSegmentedControl::SetSelectedIndex(int index, bool notify) {
    if (index < 0 || index >= SegmentCount() || index == segmented_->selectedIndex) return;
    segmented_->selectedIndex = index;
    if (notify && segmented_->selectionChanged) segmented_->selectionChanged(index);
}
int DuiSegmentedControl::SelectedIndex() const { return segmented_->selectedIndex; }
void DuiSegmentedControl::SetSelectionChangedHandler(std::function<void(int)> handler) { segmented_->selectionChanged = std::move(handler); }
void DuiSegmentedControl::SetTextStyle(render::DuiTextStyle style) { segmented_->textStyle = std::move(style); segmented_->textStyleOverride = true; }
void DuiSegmentedControl::SetBackgroundColor(core::Color color) { segmented_->background = color; segmented_->backgroundOverride = true; }
void DuiSegmentedControl::SetSelectedColor(core::Color color) { segmented_->selected = color; segmented_->selectedOverride = true; }
void DuiSegmentedControl::SetTextColor(core::Color color) { segmented_->text = color; segmented_->textOverride = true; }
void DuiSegmentedControl::SetSelectedTextColor(core::Color color) { segmented_->selectedText = color; segmented_->selectedTextOverride = true; }
core::Rect DuiSegmentedControl::SegmentRect(int index) const { return index >= 0 && index < static_cast<int>(segmented_->segmentBounds.size()) ? segmented_->segmentBounds[index] : core::Rect{}; }
core::Size DuiSegmentedControl::DesiredSize() const { return {(std::max)(120, MinimumSegmentWidth * SegmentCount()), DefaultHeight}; }
void DuiSegmentedControl::Layout(core::Rect bounds) {
    SetBounds(bounds);
    const int count = SegmentCount();
    segmented_->segmentBounds.assign(count, {});
    if (count == 0) return;
    const int width = bounds.Width();
    int left = bounds.left;
    for (int index = 0; index < count; ++index) {
        const int available = index + 1 == count ? bounds.right - left : width / count;
        const int right = left + (std::max)(MinimumSegmentWidth, available);
        segmented_->segmentBounds[index] = {left, bounds.top, right, bounds.bottom};
        left = right;
    }
}
bool DuiSegmentedControl::OnEvent(const core::Event& event) {
    if (!Enabled()) return false;
    if (event.type == core::EventType::PointerDown) {
        for (int index = 0; index < SegmentCount(); ++index) {
            if (!SegmentRect(index).Contains(event.position)) continue;
            SetSelectedIndex(index, true);
            return true;
        }
        return false;
    }
    if (event.type == core::EventType::KeyDown && SegmentCount() > 0) {
        int direction{};
        if (event.key == core::key::Left || event.key == core::key::Up)
            direction = -1;
        else if (event.key == core::key::Right || event.key == core::key::Down)
            direction = 1;
        else
            return false;
        const int selected = segmented_->selectedIndex < 0 ? 0 : segmented_->selectedIndex;
        SetSelectedIndex((selected + direction + SegmentCount()) % SegmentCount(), true);
        return true;
    }
    return false;
}
void DuiSegmentedControl::Paint(render::Canvas& canvas, core::Rect dirty) const {
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty()) return;
    const int radius = bounds.Height() / 2;
    const core::Color background = segmented_->backgroundOverride ? segmented_->background
        : Theme().Get(core::ThemeSlot::SegmentedBackground);
    const core::Color selectedColor = segmented_->selectedOverride ? segmented_->selected
        : Theme().Get(core::ThemeSlot::BrandPrimary);
    const core::Color textColor = segmented_->textOverride ? segmented_->text
        : Theme().Get(core::ThemeSlot::SegmentedText);
    const core::Color selectedText = segmented_->selectedTextOverride ? segmented_->selectedText
        : Theme().Get(core::ThemeSlot::TextOnPrimary);
    canvas.FillRoundedRect(bounds, radius, background);
    for (int index = 0; index < SegmentCount(); ++index) {
        const core::Rect segment = core::Rect::Intersect(SegmentRect(index), dirty);
        if (segment.Empty()) continue;
        const bool selected = index == segmented_->selectedIndex;
        if (selected) canvas.FillRoundedRect(segment, radius, selectedColor);
        auto style = segmented_->textStyle;
        style.color = selected ? selectedText : textColor;
        canvas.DrawText(segmented_->segments[index], segment, style, render::DuiTextAlignment::Center, false);
    }
    if (Focused() && Enabled())
        render::DrawThemedFocusRing(canvas, bounds, Theme());
}

} // namespace ysDui::controls::basic
