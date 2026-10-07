/**
 * 文件名：DuiAnchor.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：锚点导航控件实现（轨道、游标、多级缩进与滚动同步）。
 */
#include "ysDui/controls/basic/DuiAnchor.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {
namespace {

constexpr int kTrackX = 8;           // 轨道中心相对左缘
constexpr int kTrackWidth = 2;       // 灰轨厚度
constexpr int kCursorWidth = 3;      // 活动游标厚度
constexpr int kTextGap = 14;         // 轨到文字间距
constexpr int kLevelIndent = 12;     // 每级缩进
constexpr int kRowHeightMedium = 30; // medium 行高
constexpr int kRowPadY = 4;          // 游标相对行顶底内缩

[[nodiscard]] int RowHeightFor(DuiAnchorSize size)
{
    switch (size)
    {
    case DuiAnchorSize::Small:
        return 26;
    case DuiAnchorSize::Large:
        return 34;
    case DuiAnchorSize::Medium:
    default:
        return kRowHeightMedium;
    }
}

[[nodiscard]] int PreferredWidthFor(DuiAnchorSize size)
{
    switch (size)
    {
    case DuiAnchorSize::Small:
        return 120;
    case DuiAnchorSize::Large:
        return 320;
    case DuiAnchorSize::Medium:
    default:
        return 200;
    }
}

[[nodiscard]] int PointSizeFor(DuiAnchorSize size)
{
    switch (size)
    {
    case DuiAnchorSize::Small:
        return 12;
    case DuiAnchorSize::Large:
        return 15;
    case DuiAnchorSize::Medium:
    default:
        return 13;
    }
}

} // namespace

class DuiAnchor::Impl
{
public:
    struct Row final {
        int index{};
        core::Rect bounds{};
    };

    std::vector<DuiAnchorItem> items;
    mutable std::vector<Row> rows;
    int activeIndex{-1};
    DuiAnchorSize size{DuiAnchorSize::Medium};
    int boundsTolerance{5};
    int targetOffset{};
    int hoverIndex{-1};
    std::function<void(std::string_view, std::string_view)> changeHandler;
    std::function<void(std::string_view, std::string_view, int)> clickHandler;
    std::function<void(int, std::string_view)> navigateHandler;

    void SetActiveIndexInternal(int index, bool notify)
    {
        if (index < -1)
            index = -1;
        if (index >= static_cast<int>(items.size()))
            index = static_cast<int>(items.size()) - 1;
        if (index == activeIndex)
            return;
        const std::string previous =
            (activeIndex >= 0 && activeIndex < static_cast<int>(items.size()))
            ? items[static_cast<std::size_t>(activeIndex)].href
            : std::string{};
        activeIndex = index;
        if (!notify || !changeHandler)
            return;
        const std::string current =
            (activeIndex >= 0 && activeIndex < static_cast<int>(items.size()))
            ? items[static_cast<std::size_t>(activeIndex)].href
            : std::string{};
        changeHandler(current, previous);
    }

    [[nodiscard]] int HitIndex(core::Point point, const core::Rect& bounds) const
    {
        if (!bounds.Contains(point) || items.empty())
            return -1;
        const int rowH = RowHeightFor(size);
        const int index = (point.y - bounds.top) / rowH;
        if (index < 0 || index >= static_cast<int>(items.size()))
            return -1;
        return index;
    }
};

DuiAnchor::DuiAnchor() : anchor_(std::make_unique<Impl>()) {}
DuiAnchor::~DuiAnchor() = default;
DuiAnchor::DuiAnchor(DuiAnchor&&) noexcept = default;
DuiAnchor& DuiAnchor::operator=(DuiAnchor&&) noexcept = default;

void DuiAnchor::SetItems(std::vector<DuiAnchorItem> items)
{
    anchor_->items = std::move(items);
    if (anchor_->activeIndex >= static_cast<int>(anchor_->items.size()))
        anchor_->activeIndex = anchor_->items.empty() ? -1
                                                      : static_cast<int>(anchor_->items.size()) - 1;
    if (anchor_->activeIndex < 0 && !anchor_->items.empty())
        anchor_->activeIndex = 0;
    anchor_->rows.clear();
    InvalidateLayout();
}

void DuiAnchor::AddItem(DuiAnchorItem item)
{
    anchor_->items.push_back(std::move(item));
    if (anchor_->activeIndex < 0)
        anchor_->activeIndex = 0;
    anchor_->rows.clear();
    InvalidateLayout();
}

void DuiAnchor::ClearItems()
{
    anchor_->items.clear();
    anchor_->rows.clear();
    anchor_->activeIndex = -1;
    anchor_->hoverIndex = -1;
    InvalidateLayout();
}

const std::vector<DuiAnchorItem>& DuiAnchor::Items() const { return anchor_->items; }

void DuiAnchor::SetActiveHref(std::string_view href)
{
    for (std::size_t index = 0; index < anchor_->items.size(); ++index)
    {
        if (anchor_->items[index].href == href)
        {
            SetActiveIndex(static_cast<int>(index));
            return;
        }
    }
}

const std::string& DuiAnchor::ActiveHref() const
{
    static const std::string kEmpty;
    if (anchor_->activeIndex < 0
        || anchor_->activeIndex >= static_cast<int>(anchor_->items.size()))
        return kEmpty;
    return anchor_->items[static_cast<std::size_t>(anchor_->activeIndex)].href;
}

void DuiAnchor::SetActiveIndex(int index)
{
    const int previous = anchor_->activeIndex;
    anchor_->SetActiveIndexInternal(index, true);
    if (previous != anchor_->activeIndex)
        InvalidateLayout();
}

int DuiAnchor::ActiveIndex() const { return anchor_->activeIndex; }

void DuiAnchor::SetSize(DuiAnchorSize size)
{
    anchor_->size = size;
    InvalidateLayout();
}

DuiAnchorSize DuiAnchor::Size() const { return anchor_->size; }

int DuiAnchor::PreferredWidth() const { return PreferredWidthFor(anchor_->size); }

void DuiAnchor::SetBoundsTolerance(int pixels)
{
    anchor_->boundsTolerance = (std::max)(0, pixels);
}

int DuiAnchor::BoundsTolerance() const { return anchor_->boundsTolerance; }

void DuiAnchor::SetTargetOffset(int pixels) { anchor_->targetOffset = pixels; }

int DuiAnchor::TargetOffset() const { return anchor_->targetOffset; }

void DuiAnchor::SyncActiveFromScroll(int scrollY)
{
    const int previous = anchor_->activeIndex;
    if (anchor_->items.empty())
    {
        anchor_->SetActiveIndexInternal(-1, true);
        if (previous != anchor_->activeIndex)
            InvalidateLayout();
        return;
    }
    const int threshold = scrollY + anchor_->boundsTolerance;
    int chosen = 0;
    for (std::size_t index = 0; index < anchor_->items.size(); ++index)
    {
        if (anchor_->items[index].contentY <= threshold)
            chosen = static_cast<int>(index);
        else
            break;
    }
    anchor_->SetActiveIndexInternal(chosen, true);
    if (previous != anchor_->activeIndex)
        InvalidateLayout();
}

void DuiAnchor::SetChangeHandler(std::function<void(std::string_view, std::string_view)> handler)
{
    anchor_->changeHandler = std::move(handler);
}

void DuiAnchor::SetClickHandler(std::function<void(std::string_view, std::string_view, int)> handler)
{
    anchor_->clickHandler = std::move(handler);
}

void DuiAnchor::SetNavigateHandler(std::function<void(int, std::string_view)> handler)
{
    anchor_->navigateHandler = std::move(handler);
}

core::Size DuiAnchor::DesiredSize() const
{
    const int rowH = RowHeightFor(anchor_->size);
    const int height = (std::max)(rowH, rowH * static_cast<int>(anchor_->items.size()));
    return {PreferredWidth(), height};
}

void DuiAnchor::Layout(core::Rect bounds)
{
    core::Control::Layout(bounds);
}

bool DuiAnchor::OnEvent(const core::Event& event)
{
    if (!Enabled() || !EffectivelyVisible())
        return false;

    if (event.type == core::EventType::PointerMove)
    {
        const int hit = anchor_->HitIndex(event.position, Bounds());
        if (hit != anchor_->hoverIndex)
        {
            anchor_->hoverIndex = hit;
            InvalidateLayout();
        }
        return hit >= 0;
    }

    if (event.type == core::EventType::PointerLeave)
    {
        if (anchor_->hoverIndex >= 0)
        {
            anchor_->hoverIndex = -1;
            InvalidateLayout();
        }
        return false;
    }

    if (event.type != core::EventType::PointerDown || !Bounds().Contains(event.position))
        return false;

    const int index = anchor_->HitIndex(event.position, Bounds());
    if (index < 0)
        return false;

    const DuiAnchorItem& item = anchor_->items[static_cast<std::size_t>(index)];
    SetActiveIndex(index);
    if (anchor_->clickHandler)
        anchor_->clickHandler(item.href, item.title, index);
    if (anchor_->navigateHandler)
    {
        const int y = (std::max)(0, item.contentY - anchor_->targetOffset);
        anchor_->navigateHandler(y, item.href);
    }
    return true;
}

void DuiAnchor::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect clipped = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || clipped.Empty())
        return;

    const core::Rect bounds = Bounds();
    const int rowH = RowHeightFor(anchor_->size);
    const core::Color trackColor = Theme().Get(core::ThemeSlot::BorderLight);
    const core::Color activeColor = Theme().Get(core::ThemeSlot::BrandPrimary);
    const core::Color textColor = Theme().Get(core::ThemeSlot::TextDefault);
    const core::Color subtleColor = Theme().Get(core::ThemeSlot::TextSubtle);
    const core::Color linkColor = Theme().Get(core::ThemeSlot::TextLink);

    // 灰轨只覆盖条目区域，不延伸到控件空白底部
    const int trackLeft = bounds.left + kTrackX - kTrackWidth / 2;
    const int trackBottom = (std::min)(
        bounds.bottom,
        bounds.top + rowH * static_cast<int>(anchor_->items.size()));
    if (trackBottom > bounds.top)
        canvas.FillRect({trackLeft, bounds.top, trackLeft + kTrackWidth, trackBottom}, trackColor);

    anchor_->rows.clear();
    anchor_->rows.reserve(anchor_->items.size());

    render::DuiTextStyle style;
    style.pointSize = PointSizeFor(anchor_->size);
    style.family = "Segoe UI, Microsoft YaHei UI, Arial";

    for (std::size_t index = 0; index < anchor_->items.size(); ++index)
    {
        const DuiAnchorItem& item = anchor_->items[index];
        const int top = bounds.top + static_cast<int>(index) * rowH;
        const int bottom = (std::min)(bounds.bottom, top + rowH);
        if (top >= bounds.bottom)
            break;

        const int level = (std::clamp)(item.level, 1, 6);
        const int textLeft = bounds.left + kTrackX + kTextGap + (level - 1) * kLevelIndent;
        const core::Rect rowBounds{bounds.left, top, bounds.right, bottom};
        anchor_->rows.push_back({static_cast<int>(index), rowBounds});

        const bool active = static_cast<int>(index) == anchor_->activeIndex;
        if (active)
        {
            const int cursorLeft = bounds.left + kTrackX - kCursorWidth / 2;
            canvas.FillRect({cursorLeft, top + kRowPadY, cursorLeft + kCursorWidth, bottom - kRowPadY},
                            activeColor.alpha != 0 ? activeColor : linkColor);
        }

        style.color = active ? (linkColor.alpha != 0 ? linkColor : activeColor) : textColor;
        if (!active && static_cast<int>(index) == anchor_->hoverIndex)
            style.color = subtleColor.alpha != 0 ? subtleColor : textColor;

        const core::Rect textBounds{textLeft, top, bounds.right - 4, bottom};
        if (!core::Rect::Intersect(textBounds, clipped).Empty() && !item.title.empty())
            canvas.DrawText(item.title, textBounds, style, render::DuiTextAlignment::Start, false);
    }
}

core::DuiAccessibilityData DuiAnchor::CreateAccessibilityData() const
{
    core::DuiAccessibilityData data;
    data.role = core::DuiAccessibilityRole::List;
    data.name = "锚点导航";
    data.value = ActiveHref();
    data.description = "页面锚点目录";
    data.keyboardFocusable = true;
    data.patterns = core::DuiAccessibilityPattern::Selection;
    return data;
}

} // namespace ysDui::controls::basic
