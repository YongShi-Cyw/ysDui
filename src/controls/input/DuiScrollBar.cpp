#include "ysDui/controls/input/DuiScrollBar.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiPath.hpp"

namespace ysDui::controls::input {
namespace {
constexpr core::Color kThumbHoverColor{166, 166, 166, 255};
constexpr core::Color kButtonHoverColor{218, 218, 218, 255};
constexpr int kThumbThickness = 15;
constexpr int kButtonExtent = 18;
constexpr int kArrowViewportPixels = 12;
constexpr int kSvgViewBoxCenter = 512;
constexpr int kSvgViewBoxSize = 1024;

// 取自指定 SVG 向上箭头的外轮廓关键点，其他方向通过旋转生成。
constexpr std::array<core::Point, 14> kUpArrowOutline{{
    {512, 331}, {553, 346}, {830, 580}, {854, 625}, {838, 670}, {800, 694}, {749, 678},
    {512, 478}, {275, 678}, {224, 694}, {186, 670}, {170, 625}, {194, 580}, {471, 346},
}};

enum class ScrollBarPart {
    None,
    DecrementButton,
    Thumb,
    IncrementButton,
};

int axisLength(core::Rect bounds, bool horizontal)
{
    return horizontal ? bounds.Width() : bounds.Height();
}

int crossAxisLength(core::Rect bounds, bool horizontal)
{
    return horizontal ? bounds.Height() : bounds.Width();
}

int axisCoordinate(core::Point point, bool horizontal)
{
    return horizontal ? point.x : point.y;
}

int buttonExtent(core::Rect bounds, bool horizontal)
{
    return (std::min)(kButtonExtent, (std::max)(0, axisLength(bounds, horizontal)) / 2);
}

core::Rect decrementButtonRect(core::Rect bounds, bool horizontal)
{
    const int extent = buttonExtent(bounds, horizontal);
    if (horizontal) return {bounds.left, bounds.top, bounds.left + extent, bounds.bottom};
    return {bounds.left, bounds.top, bounds.right, bounds.top + extent};
}

core::Rect incrementButtonRect(core::Rect bounds, bool horizontal)
{
    const int extent = buttonExtent(bounds, horizontal);
    if (horizontal) return {bounds.right - extent, bounds.top, bounds.right, bounds.bottom};
    return {bounds.left, bounds.bottom - extent, bounds.right, bounds.bottom};
}

core::Rect movementTrackRect(core::Rect bounds, bool horizontal)
{
    const int extent = buttonExtent(bounds, horizontal);
    if (horizontal) return {bounds.left + extent, bounds.top, bounds.right - extent, bounds.bottom};
    return {bounds.left, bounds.top + extent, bounds.right, bounds.bottom - extent};
}

core::Rect fitThumbCrossAxis(core::Rect bounds, bool horizontal)
{
    const int crossLength = (std::max)(0, crossAxisLength(bounds, horizontal));
    const int thickness = (std::min)(kThumbThickness, crossLength);
    const int inset = (crossLength - thickness) / 2;
    if (horizontal)
        return {bounds.left, bounds.top + inset, bounds.right, bounds.top + inset + thickness};
    return {bounds.left + inset, bounds.top, bounds.left + inset + thickness, bounds.bottom};
}

ScrollBarPart partAt(core::Point point, core::Rect decrementButton,
                     core::Rect thumb, core::Rect incrementButton)
{
    if (decrementButton.Contains(point)) return ScrollBarPart::DecrementButton;
    if (incrementButton.Contains(point)) return ScrollBarPart::IncrementButton;
    if (thumb.Contains(point)) return ScrollBarPart::Thumb;
    return ScrollBarPart::None;
}

render::DuiPath arrowPath(core::Rect bounds, bool horizontal, bool increment)
{
    const int centerX = (bounds.left + bounds.right) / 2;
    const int centerY = (bounds.top + bounds.bottom) / 2;
    const int viewport = (std::min)(kArrowViewportPixels,
                                    (std::min)(bounds.Width(), bounds.Height()));
    const auto scale = [viewport](int coordinate)
    {
        const int scaled = coordinate * viewport;
        return scaled >= 0 ? (scaled + kSvgViewBoxSize / 2) / kSvgViewBoxSize
                           : (scaled - kSvgViewBoxSize / 2) / kSvgViewBoxSize;
    };
    const auto rotate = [horizontal, increment](core::Point point)
    {
        const int x = point.x - kSvgViewBoxCenter;
        const int y = point.y - kSvgViewBoxCenter;
        if (horizontal)
            return increment ? core::Point{-y, x} : core::Point{y, -x};
        return increment ? core::Point{-x, -y} : core::Point{x, y};
    };
    render::DuiPath path;
    for (std::size_t index{}; index < kUpArrowOutline.size(); ++index) {
        const core::Point point = rotate(kUpArrowOutline[index]);
        const core::Point destination{centerX + scale(point.x), centerY + scale(point.y)};
        if (index == 0)
            path.MoveTo(destination);
        else
            path.LineTo(destination);
    }
    path.Close();
    return path;
}
} // namespace

class DuiScrollBar::Impl {
public:
    bool horizontal{};
    int minimum{};
    int maximum{};
    int pageSize{1};
    int position{};
    int lineSize{1};
    bool dragging{};
    int dragOffset{};
    ScrollBarPart hoveredPart{ScrollBarPart::None};
    core::Rect arrowBounds;
    render::DuiPath decrementArrow;
    render::DuiPath incrementArrow;
    bool arrowHorizontal{};
    bool arrowsValid{};
    std::function<void(int)> valueChangedHandler;
    core::Color trackColor{240, 240, 240, 255};
    core::Color thumbColor{205, 205, 205, 255};
    bool trackColorOverride{};
    bool thumbColorOverride{};
};

DuiScrollBar::DuiScrollBar(bool horizontal) : scrollBar_(std::make_unique<Impl>()) { scrollBar_->horizontal = horizontal; }
DuiScrollBar::~DuiScrollBar() = default;
DuiScrollBar::DuiScrollBar(DuiScrollBar&&) noexcept = default;
DuiScrollBar& DuiScrollBar::operator=(DuiScrollBar&&) noexcept = default;

void DuiScrollBar::SetHorizontal(bool horizontal) { scrollBar_->horizontal = horizontal; }
bool DuiScrollBar::Horizontal() const { return scrollBar_->horizontal; }
void DuiScrollBar::SetRange(int minimum, int maximum) {
    scrollBar_->minimum = minimum;
    scrollBar_->maximum = std::max(minimum, maximum);
    SetPosition(scrollBar_->position, false);
}
int DuiScrollBar::Minimum() const { return scrollBar_->minimum; }
int DuiScrollBar::Maximum() const { return scrollBar_->maximum; }
void DuiScrollBar::SetPageSize(int pageSize) { scrollBar_->pageSize = std::max(1, pageSize); }
int DuiScrollBar::PageSize() const { return scrollBar_->pageSize; }
void DuiScrollBar::SetPosition(int position, bool notify) {
    const int next = std::clamp(position, scrollBar_->minimum, scrollBar_->maximum);
    if (next == scrollBar_->position) return;
    scrollBar_->position = next;
    if (notify && scrollBar_->valueChangedHandler) scrollBar_->valueChangedHandler(next);
}
int DuiScrollBar::Position() const { return scrollBar_->position; }
void DuiScrollBar::SetLineSize(int lineSize) { scrollBar_->lineSize = std::max(1, lineSize); }
int DuiScrollBar::LineSize() const { return scrollBar_->lineSize; }
void DuiScrollBar::SetValueChangedHandler(std::function<void(int)> handler) { scrollBar_->valueChangedHandler = std::move(handler); }
void DuiScrollBar::SetTrackColor(core::Color color) { scrollBar_->trackColor = color; scrollBar_->trackColorOverride = true; }
void DuiScrollBar::SetThumbColor(core::Color color) { scrollBar_->thumbColor = color; scrollBar_->thumbColorOverride = true; }

core::Rect DuiScrollBar::ComputeThumbRect() const
{
    const core::Rect trackBounds = movementTrackRect(Bounds(), scrollBar_->horizontal);
    const int track = std::max(0, axisLength(trackBounds, scrollBar_->horizontal));
    const int range = scrollBar_->maximum - scrollBar_->minimum;
    const int total = range + scrollBar_->pageSize;
    const int thumb = track <= MinimumThumbPixels ? track : std::clamp(total <= 0 ? track : track * scrollBar_->pageSize / total, MinimumThumbPixels, track);
    const int travel = track - thumb;
    const int offset = range <= 0 || travel <= 0 ? 0 : travel * (scrollBar_->position - scrollBar_->minimum) / range;
    if (scrollBar_->horizontal)
        return fitThumbCrossAxis({trackBounds.left + offset, trackBounds.top,
                                  trackBounds.left + offset + thumb, trackBounds.bottom}, true);
    return fitThumbCrossAxis({trackBounds.left, trackBounds.top + offset,
                              trackBounds.right, trackBounds.top + offset + thumb}, false);
}

bool DuiScrollBar::OnEvent(const core::Event& event)
{
    if (!Enabled()) return false;
    const core::Rect bounds = Bounds();
    const core::Rect decrementButton = decrementButtonRect(bounds, scrollBar_->horizontal);
    const core::Rect incrementButton = incrementButtonRect(bounds, scrollBar_->horizontal);
    const core::Rect thumb = ComputeThumbRect();
    if (event.type == core::EventType::PointerWheel && bounds.Contains(event.position)) {
        const int direction = event.wheelDelta > 0 ? -1 : event.wheelDelta < 0 ? 1 : 0;
        if (direction != 0) SetPosition(Position() + direction * LineSize() * 3);
        return direction != 0;
    }
    if (event.type == core::EventType::PointerMove && !scrollBar_->dragging) {
        scrollBar_->hoveredPart = partAt(event.position, decrementButton, thumb, incrementButton);
        SetHovered(bounds.Contains(event.position));
        return false;
    }
    if (event.type == core::EventType::PointerDown && bounds.Contains(event.position)) {
        scrollBar_->hoveredPart = partAt(event.position, decrementButton, thumb, incrementButton);
        SetHovered(true);
        if (scrollBar_->hoveredPart == ScrollBarPart::DecrementButton) {
            SetPosition(Position() - LineSize());
        } else if (scrollBar_->hoveredPart == ScrollBarPart::IncrementButton) {
            SetPosition(Position() + LineSize());
        } else if (scrollBar_->hoveredPart == ScrollBarPart::Thumb) {
            scrollBar_->dragging = true;
            scrollBar_->dragOffset = axisCoordinate(event.position, scrollBar_->horizontal) - (scrollBar_->horizontal ? thumb.left : thumb.top);
            SetCaptured(true);
        } else {
            SetPosition(Position() + (axisCoordinate(event.position, scrollBar_->horizontal) < (scrollBar_->horizontal ? thumb.left : thumb.top) ? -PageSize() : PageSize()));
        }
        return true;
    }
    if (event.type == core::EventType::PointerMove && scrollBar_->dragging) {
        const core::Rect trackBounds = movementTrackRect(bounds, scrollBar_->horizontal);
        const int track = axisLength(trackBounds, scrollBar_->horizontal);
        const int thumbLength = axisLength(thumb, scrollBar_->horizontal);
        const int travel = track - thumbLength;
        if (travel > 0 && Maximum() > Minimum()) {
            const int origin = scrollBar_->horizontal ? trackBounds.left : trackBounds.top;
            const int offset = std::clamp(axisCoordinate(event.position, scrollBar_->horizontal) - origin - scrollBar_->dragOffset, 0, travel);
            SetPosition(Minimum() + offset * (Maximum() - Minimum()) / travel);
        }
        return true;
    }
    if (event.type == core::EventType::PointerLeave) {
        scrollBar_->hoveredPart = ScrollBarPart::None;
        SetHovered(false);
        return false;
    }
    if (event.type == core::EventType::PointerCancel && scrollBar_->dragging) {
        scrollBar_->dragging = false;
        SetCaptured(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && scrollBar_->dragging) {
        scrollBar_->dragging = false;
        SetCaptured(false);
        scrollBar_->hoveredPart = partAt(event.position, decrementButton,
                                         ComputeThumbRect(), incrementButton);
        return true;
    }
    return false;
}

void DuiScrollBar::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible()) return;
    const core::Rect bounds = Bounds();
    const core::Color trackColor = scrollBar_->trackColorOverride ? scrollBar_->trackColor
        : Theme().Get(core::ThemeSlot::ScrollTrack);
    const core::Rect track = core::Rect::Intersect(bounds, dirty);
    if (!track.Empty()) canvas.FillRect(track, trackColor);
    const core::Rect decrementButton = decrementButtonRect(bounds, scrollBar_->horizontal);
    const core::Rect incrementButton = incrementButtonRect(bounds, scrollBar_->horizontal);
    const bool decrementHovered = Enabled() && Hovered()
        && scrollBar_->hoveredPart == ScrollBarPart::DecrementButton;
    const bool incrementHovered = Enabled() && Hovered()
        && scrollBar_->hoveredPart == ScrollBarPart::IncrementButton;
    const core::Rect visibleDecrementButton = core::Rect::Intersect(decrementButton, dirty);
    if (!visibleDecrementButton.Empty())
        canvas.FillRect(visibleDecrementButton, decrementHovered ? kButtonHoverColor : trackColor);
    const core::Rect visibleIncrementButton = core::Rect::Intersect(incrementButton, dirty);
    if (!visibleIncrementButton.Empty())
        canvas.FillRect(visibleIncrementButton, incrementHovered ? kButtonHoverColor : trackColor);
    const core::Rect thumb = core::Rect::Intersect(ComputeThumbRect(), dirty);
    const bool thumbHovered = Enabled() && (scrollBar_->dragging
        || (Hovered() && scrollBar_->hoveredPart == ScrollBarPart::Thumb));
    const core::Color thumbColor = scrollBar_->thumbColorOverride ? scrollBar_->thumbColor
        : thumbHovered ? kThumbHoverColor : Theme().Get(core::ThemeSlot::ScrollThumb);
    if (!thumb.Empty()) canvas.FillRect(thumb, thumbColor);
    const core::Color arrowColor = Theme().Get(Enabled() ? core::ThemeSlot::InputArrow
                                                         : core::ThemeSlot::TextDisabled);
    if (!scrollBar_->arrowsValid || scrollBar_->arrowBounds != bounds
        || scrollBar_->arrowHorizontal != scrollBar_->horizontal)
    {
        scrollBar_->arrowBounds = bounds;
        scrollBar_->arrowHorizontal = scrollBar_->horizontal;
        scrollBar_->decrementArrow = arrowPath(decrementButton, scrollBar_->horizontal, false);
        scrollBar_->incrementArrow = arrowPath(incrementButton, scrollBar_->horizontal, true);
        scrollBar_->arrowsValid = true;
    }
    if (!visibleDecrementButton.Empty())
        canvas.FillPath(scrollBar_->decrementArrow, arrowColor);
    if (!visibleIncrementButton.Empty())
        canvas.FillPath(scrollBar_->incrementArrow, arrowColor);
}

} // namespace ysDui::controls::input
