#include "ysDui/controls/layout/DuiSplitter.hpp"

#include "ysDui/render/DuiPaintChildren.hpp"
#include "ysDui/core/DuiTheme.hpp"

#include <algorithm>
#include <utility>

namespace ysDui::controls::layout {
namespace {
constexpr int MinimumDragThickness = 7;
int nonNegative(int value)
{
    return std::max(0, value);
}

core::Rect DragRect(core::Rect bar, DuiSplitterOrientation orientation)
{
    const int thickness = orientation == DuiSplitterOrientation::Vertical ? bar.Width() : bar.Height();
    const int leading = (std::max)(0, MinimumDragThickness - thickness) / 2;
    const int trailing = (std::max)(0, MinimumDragThickness - thickness) - leading;
    if (orientation == DuiSplitterOrientation::Vertical)
        return {bar.left - leading, bar.top, bar.right + trailing, bar.bottom};
    return {bar.left, bar.top - leading, bar.right, bar.bottom + trailing};
}
} // namespace

class DuiSplitter::Impl {
public:
    DuiSplitterOrientation orientation{DuiSplitterOrientation::Vertical};
    int barThickness{4};
    int firstMinimum{40};
    int secondMinimum{40};
    int targetPixels{100};
    int appliedPixels{100};
    double pendingFraction{-1.0};
    bool dragging{};
    int dragStartCoordinate{};
    int dragStartPixels{};
    core::Control* panes[2]{};
    std::function<void(int)> valueChangedHandler;
    core::Color barColor{238, 238, 240, 255};
    bool barColorOverride{};
};

DuiSplitter::DuiSplitter() : splitter_(std::make_unique<Impl>())
{
    SetPointerCursor(core::DuiPointerCursor::ResizeHorizontal);
}
DuiSplitter::~DuiSplitter() = default;
DuiSplitter::DuiSplitter(DuiSplitter&&) noexcept = default;
DuiSplitter& DuiSplitter::operator=(DuiSplitter&&) noexcept = default;

void DuiSplitter::SetOrientation(DuiSplitterOrientation orientation)
{
    splitter_->orientation = orientation;
    SetPointerCursor(orientation == DuiSplitterOrientation::Vertical
        ? core::DuiPointerCursor::ResizeHorizontal
        : core::DuiPointerCursor::ResizeVertical);
    Layout(Bounds());
}
DuiSplitterOrientation DuiSplitter::GetOrientation() const { return splitter_->orientation; }
void DuiSplitter::SetBarThickness(int pixels) { splitter_->barThickness = std::max(1, pixels); Layout(Bounds()); }
int DuiSplitter::GetBarThickness() const { return splitter_->barThickness; }
void DuiSplitter::SetMinSizes(int first, int second) { splitter_->firstMinimum = nonNegative(first); splitter_->secondMinimum = nonNegative(second); Layout(Bounds()); }
void DuiSplitter::SetSplitPixels(int first) { splitter_->pendingFraction = -1.0; splitter_->targetPixels = nonNegative(first); Layout(Bounds()); }
int DuiSplitter::SplitPixels() const { return splitter_->appliedPixels; }
double DuiSplitter::SplitFraction() const
{
    const core::Rect bounds = Bounds();
    const int available = splitter_->orientation == DuiSplitterOrientation::Vertical
        ? bounds.Width() : bounds.Height();
    const int track = available - splitter_->barThickness;
    if (track <= 0)
        return 0.0;
    return static_cast<double>(splitter_->appliedPixels) / static_cast<double>(track);
}
void DuiSplitter::SetSplitFraction(double fraction) { splitter_->pendingFraction = std::clamp(fraction, 0.0, 1.0); Layout(Bounds()); }
void DuiSplitter::SetValueChangedHandler(std::function<void(int)> handler) { splitter_->valueChangedHandler = std::move(handler); }
void DuiSplitter::SetBarColor(core::Color color) { splitter_->barColor = color; splitter_->barColorOverride = true; }

void DuiSplitter::SetPane(int index, std::unique_ptr<core::Control> pane)
{
    if (index < 0 || index > 1) return;
    if (splitter_->panes[index] != nullptr) {
        (void)RemoveChild(splitter_->panes[index]);
        splitter_->panes[index] = nullptr;
    }
    if (pane != nullptr) {
        splitter_->panes[index] = pane.get();
        AddChild(std::move(pane));
    }
    Layout(Bounds());
}

core::Control* DuiSplitter::GetPane(int index) const
{
    return index >= 0 && index < 2 ? splitter_->panes[index] : nullptr;
}

void DuiSplitter::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    const int available = splitter_->orientation == DuiSplitterOrientation::Vertical ? bounds.Width() : bounds.Height();
    const int track = available - splitter_->barThickness;
    if (splitter_->pendingFraction >= 0.0 && track > 0) {
        splitter_->targetPixels = static_cast<int>(track * splitter_->pendingFraction);
        splitter_->pendingFraction = -1.0;
    }
    const int maximum = std::max(splitter_->firstMinimum, track - splitter_->secondMinimum);
    splitter_->appliedPixels = std::clamp(splitter_->targetPixels, splitter_->firstMinimum, maximum);

    const auto layoutPane = [](core::Control* pane, core::Rect paneBounds)
    {
        if (pane != nullptr)
            pane->Layout(paneBounds);
    };

    if (splitter_->orientation == DuiSplitterOrientation::Vertical) {
        layoutPane(splitter_->panes[0], {bounds.left, bounds.top,
                                        bounds.left + splitter_->appliedPixels, bounds.bottom});
        layoutPane(splitter_->panes[1], {bounds.left + splitter_->appliedPixels + splitter_->barThickness,
                                        bounds.top, bounds.right, bounds.bottom});
    } else {
        layoutPane(splitter_->panes[0], {bounds.left, bounds.top, bounds.right,
                                        bounds.top + splitter_->appliedPixels});
        layoutPane(splitter_->panes[1], {bounds.left,
                                        bounds.top + splitter_->appliedPixels + splitter_->barThickness,
                                        bounds.right, bounds.bottom});
    }
}

core::Rect DuiSplitter::BarRect() const
{
    const core::Rect bounds = Bounds();
    if (splitter_->orientation == DuiSplitterOrientation::Vertical)
        return {bounds.left + splitter_->appliedPixels, bounds.top, bounds.left + splitter_->appliedPixels + splitter_->barThickness, bounds.bottom};
    return {bounds.left, bounds.top + splitter_->appliedPixels, bounds.right, bounds.top + splitter_->appliedPixels + splitter_->barThickness};
}

core::Control* DuiSplitter::HitTest(core::Point point)
{
    const bool overBar = EffectivelyVisible() && Enabled()
        && DragRect(BarRect(), splitter_->orientation).Contains(point);
    SetHovered(overBar);
    if (!EffectivelyVisible() || !Enabled() || !Bounds().Contains(point)) return nullptr;
    if (splitter_->dragging || overBar) return this;
    return core::Control::HitTest(point);
}

bool DuiSplitter::OnEvent(const core::Event& event)
{
    if (!Enabled()) return false;
    if (event.type == core::EventType::PointerMove)
    {
        SetHovered(splitter_->dragging
            || DragRect(BarRect(), splitter_->orientation).Contains(event.position));
    }
    if (event.type == core::EventType::PointerDown
        && DragRect(BarRect(), splitter_->orientation).Contains(event.position)) {
        splitter_->dragging = true;
        splitter_->dragStartCoordinate = splitter_->orientation == DuiSplitterOrientation::Vertical ? event.position.x : event.position.y;
        splitter_->dragStartPixels = splitter_->appliedPixels;
        SetCaptured(true);
        return true;
    }
    if (event.type == core::EventType::PointerMove && splitter_->dragging) {
        const int coordinate = splitter_->orientation == DuiSplitterOrientation::Vertical ? event.position.x : event.position.y;
        splitter_->targetPixels = nonNegative(splitter_->dragStartPixels + coordinate - splitter_->dragStartCoordinate);
        const int previousPixels = splitter_->appliedPixels;
        Layout(Bounds());
        if (splitter_->appliedPixels != previousPixels && splitter_->valueChangedHandler)
            splitter_->valueChangedHandler(splitter_->appliedPixels);
        return true;
    }
    if (event.type == core::EventType::PointerCancel && splitter_->dragging) {
        splitter_->dragging = false;
        SetHovered(false);
        SetCaptured(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && splitter_->dragging) {
        splitter_->dragging = false;
        SetHovered(DragRect(BarRect(), splitter_->orientation).Contains(event.position));
        SetCaptured(false);
        if (splitter_->valueChangedHandler) splitter_->valueChangedHandler(splitter_->appliedPixels);
        return true;
    }
    return false;
}

void DuiSplitter::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible()) return;
    render::PaintChildren(*this, canvas, dirty);
    const core::Rect clipped = core::Rect::Intersect(BarRect(), dirty);
    if (!clipped.Empty())
        canvas.FillRect(clipped, Hovered() || splitter_->dragging
            ? Theme().Get(core::ThemeSlot::SplitterHover)
            : splitter_->barColorOverride ? splitter_->barColor : Theme().Get(core::ThemeSlot::SplitterBar));
}

} // namespace ysDui::controls::layout
