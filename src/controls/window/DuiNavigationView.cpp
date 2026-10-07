/**
 * 文件名：DuiNavigationView.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：左侧导航绘制、命中选择与内容区排列。
 */
#include "ysDui/controls/window/DuiNavigationView.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiPaintChildren.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::window {
namespace {
constexpr int kExpandedPaneWidth = 220; // 展开侧栏宽度
constexpr int kCompactPaneWidth = 48; // 紧凑侧栏宽度
constexpr int kItemHeight = 36; // 单项行高
} // namespace

class DuiNavigationView::Impl {
public:
    struct Item final {
        std::string text;
        int id{};
    };
    std::vector<Item> items;
    std::function<void(int)> changed;
    DuiNavigationPaneDisplayMode mode{DuiNavigationPaneDisplayMode::Left};
    int selected{-1};
};

DuiNavigationView::DuiNavigationView() : nav_(std::make_unique<Impl>()) {}
DuiNavigationView::~DuiNavigationView() = default;
DuiNavigationView::DuiNavigationView(DuiNavigationView&&) noexcept = default;
DuiNavigationView& DuiNavigationView::operator=(DuiNavigationView&&) noexcept = default;

int DuiNavigationView::AddItem(std::string text, int id)
{
    nav_->items.push_back({std::move(text), id});
    if (nav_->selected < 0)
        nav_->selected = 0;
    InvalidateLayout();
    return ItemCount() - 1;
}
int DuiNavigationView::ItemCount() const { return static_cast<int>(nav_->items.size()); }
std::string DuiNavigationView::ItemText(int index) const
{
    return index >= 0 && index < ItemCount() ? nav_->items[index].text : std::string{};
}
void DuiNavigationView::SetSelectedIndex(int index, bool notify)
{
    if (index < -1 || index >= ItemCount() || nav_->selected == index)
        return;
    nav_->selected = index;
    if (notify && nav_->changed)
        nav_->changed(index);
}
int DuiNavigationView::SelectedIndex() const { return nav_->selected; }
void DuiNavigationView::SetPaneDisplayMode(DuiNavigationPaneDisplayMode mode)
{
    if (nav_->mode == mode)
        return;
    nav_->mode = mode;
    InvalidateLayout();
}
DuiNavigationPaneDisplayMode DuiNavigationView::PaneDisplayMode() const { return nav_->mode; }
void DuiNavigationView::SetContent(std::unique_ptr<core::Control> content)
{
    while (!Children().empty())
        (void)RemoveChild(Children().front().get());
    if (content)
        AddChild(std::move(content));
}
core::Control* DuiNavigationView::Content() const
{
    return Children().empty() ? nullptr : Children().front().get();
}

void DuiNavigationView::SetSelectionChangedHandler(std::function<void(int)> handler)
{
    nav_->changed = std::move(handler);
}

core::Rect DuiNavigationView::PaneRect() const
{
    const core::Rect bounds = Bounds();
    const int width = nav_->mode == DuiNavigationPaneDisplayMode::LeftCompact
        ? kCompactPaneWidth : kExpandedPaneWidth;
    return {bounds.left, bounds.top, bounds.left + std::min(width, bounds.Width()), bounds.bottom};
}
core::Rect DuiNavigationView::ContentRect() const
{
    const core::Rect bounds = Bounds();
    const core::Rect pane = PaneRect();
    return {pane.right, bounds.top, bounds.right, bounds.bottom};
}

void DuiNavigationView::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    if (core::Control* content = Content())
        content->Layout(ContentRect());
}

bool DuiNavigationView::OnEvent(const core::Event& event)
{
    if (!Enabled())
        return false;
    if (event.type == core::EventType::PointerDown && PaneRect().Contains(event.position))
    {
        const int index = (event.position.y - PaneRect().top) / kItemHeight;
        if (index >= 0 && index < ItemCount())
            SetSelectedIndex(index);
        return true;
    }
    return false;
}

void DuiNavigationView::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::Rect clipped = core::Rect::Intersect(Bounds(), dirty);
    if (clipped.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    const core::Rect pane = PaneRect();
    canvas.FillRect(core::Rect::Intersect(pane, dirty), theme.Get(core::ThemeSlot::ControlBackground));
    canvas.FillRect({pane.right - 1, pane.top, pane.right, pane.bottom},
                    theme.Get(core::ThemeSlot::ControlBorder));
    const bool compact = nav_->mode == DuiNavigationPaneDisplayMode::LeftCompact;
    for (int index = 0; index < ItemCount(); ++index)
    {
        const core::Rect row{pane.left, pane.top + index * kItemHeight, pane.right,
                             pane.top + (index + 1) * kItemHeight};
        const core::Rect rowClip = core::Rect::Intersect(row, dirty);
        if (rowClip.Empty())
            continue;
        if (index == nav_->selected)
            canvas.FillRect(rowClip, theme.Get(core::ThemeSlot::ControlHover));
        std::string label = nav_->items[index].text;
        if (compact && !label.empty())
            label = label.substr(0, 1);
        render::DuiTextStyle style{{}, {}, 9, false};
        style.color = theme.Get(core::ThemeSlot::ButtonText);
        canvas.DrawText(label, {row.left + 12, row.top, row.right - 8, row.bottom}, style,
                        render::DuiTextAlignment::Start, false);
    }
    render::PaintChildren(*this, canvas, dirty);
}

} // namespace ysDui::controls::window
