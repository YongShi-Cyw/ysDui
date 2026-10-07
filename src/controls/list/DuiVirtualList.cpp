#include "ysDui/controls/list/DuiVirtualList.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::list {
namespace {
constexpr int DefaultRowHeight = 28;
constexpr int ScrollBarWidth = 17;
}

class DuiVirtualList::Impl {
public:
    input::DuiScrollBar* scrollBar{};
    RowRenderer renderRow;
    RowClickHandler rowClicked;
    std::function<void(int)> selectionChanged;
    core::Color background{255, 255, 255, 255};
    core::Color selected{180, 210, 245, 255};
    core::Color hovered{232, 240, 252, 255};
    int rowCount{};
    int rowHeight{DefaultRowHeight};
    int selectedIndex{-1};
    int hoveredIndex{-1};
    bool backgroundOverride{};
    bool selectedOverride{};
    bool hoverOverride{};
};

DuiVirtualList::DuiVirtualList() : virtualList_(std::make_unique<Impl>()) {
    auto scrollBar = std::make_unique<input::DuiScrollBar>();
    virtualList_->scrollBar = scrollBar.get();
    scrollBar->SetLineSize(DefaultRowHeight);
    scrollBar->SetValueChangedHandler([](int) {});
    AddChild(std::move(scrollBar));
}
DuiVirtualList::~DuiVirtualList() = default;

void DuiVirtualList::SetRowCount(int count) { virtualList_->rowCount = (std::max)(0, count); if (virtualList_->selectedIndex >= count) virtualList_->selectedIndex = -1; UpdateScrollRange(); }
int DuiVirtualList::RowCount() const { return virtualList_->rowCount; }
void DuiVirtualList::SetRowHeight(int pixels) { virtualList_->rowHeight = (std::max)(1, pixels); virtualList_->scrollBar->SetLineSize(virtualList_->rowHeight); UpdateScrollRange(); }
int DuiVirtualList::RowHeight() const { return virtualList_->rowHeight; }
void DuiVirtualList::SetRowRenderer(RowRenderer renderer) { virtualList_->renderRow = std::move(renderer); }
void DuiVirtualList::SetRowClickHandler(RowClickHandler handler) { virtualList_->rowClicked = std::move(handler); }
void DuiVirtualList::SetSelectionChangedHandler(std::function<void(int)> handler) { virtualList_->selectionChanged = std::move(handler); }
void DuiVirtualList::SetSelectedIndex(int index, bool notify) { if (index < -1 || index >= RowCount() || index == virtualList_->selectedIndex) return; virtualList_->selectedIndex = index; if (index >= 0) EnsureVisible(index); if (notify && virtualList_->selectionChanged) virtualList_->selectionChanged(index); }
int DuiVirtualList::SelectedIndex() const { return virtualList_->selectedIndex; }
void DuiVirtualList::SetScrollPosition(int pixels) { virtualList_->scrollBar->SetPosition(pixels, false); }
int DuiVirtualList::ScrollPosition() const { return virtualList_->scrollBar->Position(); }
int DuiVirtualList::BodyWidth() const { return Bounds().Width() - (virtualList_->scrollBar->Visible() ? ScrollBarWidth : 0); }
int DuiVirtualList::VisibleRows() const { return Bounds().Height() / RowHeight(); }
core::Rect DuiVirtualList::RowRect(int index) const { const int top = Bounds().top + index * RowHeight() - ScrollPosition(); return {Bounds().left, top, Bounds().left + BodyWidth(), top + RowHeight()}; }
int DuiVirtualList::IndexFromPoint(core::Point point) const { if (!Bounds().Contains(point) || point.x >= Bounds().left + BodyWidth()) return -1; const int index = (point.y - Bounds().top + ScrollPosition()) / RowHeight(); return index >= 0 && index < RowCount() ? index : -1; }
void DuiVirtualList::EnsureVisible(int index)
{
    if (index < 0 || index >= RowCount() || Bounds().Height() <= 0)
        return;
    const int top = index * RowHeight();
    const int bottom = top + RowHeight();
    int position = ScrollPosition();
    if (top < position)
        position = top;
    else if (bottom > position + Bounds().Height())
        position = bottom - Bounds().Height();
    SetScrollPosition(position);
}
void DuiVirtualList::SetBackgroundColor(core::Color color) { virtualList_->background = color; virtualList_->backgroundOverride = true; }
void DuiVirtualList::SetSelectedColor(core::Color color) { virtualList_->selected = color; virtualList_->selectedOverride = true; }
void DuiVirtualList::SetHoverColor(core::Color color) { virtualList_->hovered = color; virtualList_->hoverOverride = true; }
void DuiVirtualList::UpdateScrollRange() { const int overflow = (std::max)(0, RowCount() * RowHeight() - Bounds().Height()); virtualList_->scrollBar->SetRange(0, overflow); virtualList_->scrollBar->SetPageSize((std::max)(1, Bounds().Height())); virtualList_->scrollBar->SetVisible(overflow > 0); }
void DuiVirtualList::Layout(core::Rect bounds) { SetBounds(bounds); UpdateScrollRange(); if (virtualList_->scrollBar->Visible()) virtualList_->scrollBar->SetBounds({bounds.right - ScrollBarWidth, bounds.top, bounds.right, bounds.bottom}); }

bool DuiVirtualList::OnEvent(const core::Event& event) {
    if (!Enabled()) return false;
    if (event.type == core::EventType::PointerMove) { virtualList_->hoveredIndex = IndexFromPoint(event.position); return virtualList_->hoveredIndex >= 0; }
    if (event.type == core::EventType::PointerWheel && Bounds().Contains(event.position)) { virtualList_->scrollBar->SetPosition(ScrollPosition() + (event.wheelDelta > 0 ? -3 : 3) * RowHeight()); return event.wheelDelta != 0; }
    if (event.type == core::EventType::PointerDown || event.type == core::EventType::PointerDoubleClick) { const int index = IndexFromPoint(event.position); if (index < 0) return false; SetSelectedIndex(index); if (virtualList_->rowClicked) virtualList_->rowClicked(index, event.type == core::EventType::PointerDoubleClick); return true; }
    if (event.type == core::EventType::KeyDown && RowCount() > 0) { int next = SelectedIndex(); if (event.key == core::key::Up) next = (std::max)(0, next - 1); else if (event.key == core::key::Down) next = (std::min)(RowCount() - 1, next + 1); else if (event.key == core::key::Home) next = 0; else if (event.key == core::key::End) next = RowCount() - 1; else if (event.key == core::key::PageUp) next = (std::max)(0, next - VisibleRows()); else if (event.key == core::key::PageDown) next = (std::min)(RowCount() - 1, next + VisibleRows()); else return false; SetSelectedIndex(next); return true; }
    return false;
}

void DuiVirtualList::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible()) return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty()) return;
    const core::Color background = virtualList_->backgroundOverride
        ? virtualList_->background : Theme().Get(core::ThemeSlot::ListBackground);
    const core::Color selected = virtualList_->selectedOverride
        ? virtualList_->selected : Theme().Get(core::ThemeSlot::ListSelection);
    const core::Color hovered = virtualList_->hoverOverride
        ? virtualList_->hovered : Theme().Get(core::ThemeSlot::ListHover);
    canvas.FillRect(bounds, background);
    canvas.PushClip({Bounds().left, Bounds().top, Bounds().left + BodyWidth(), Bounds().bottom});
    const int first = ScrollPosition() / RowHeight();
    const int last = (std::min)(RowCount(), first + VisibleRows() + 2);
    for (int index = first; index < last; ++index) {
        const core::Rect row = RowRect(index);
        if (core::Rect::Intersect(row, dirty).Empty()) continue;
        if (index == SelectedIndex()) canvas.FillRect(row, selected);
        else if (index == virtualList_->hoveredIndex) canvas.FillRect(row, hovered);
        if (virtualList_->renderRow) virtualList_->renderRow(canvas, index, row, index == SelectedIndex(), index == virtualList_->hoveredIndex);
    }
    canvas.PopClip();
    if (virtualList_->scrollBar->Visible()) virtualList_->scrollBar->Paint(canvas, dirty);
    // 列表底色与页面底色同为浅色：靠外框区分控件范围
    canvas.StrokeRoundedRect(Bounds(), 0, Theme().Get(core::ThemeSlot::GridBorder), 1.0F);
}

} // namespace ysDui::controls::list
