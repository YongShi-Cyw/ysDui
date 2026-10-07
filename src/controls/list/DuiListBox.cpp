#include "ysDui/controls/list/DuiListBox.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiImage.hpp"

namespace ysDui::controls::list {
namespace {
constexpr int DefaultRowHeight = 22;
constexpr int ScrollBarWidth = 17;
constexpr int CheckboxColumnWidth = 22;
constexpr int TextPadding = 6;
}

class DuiListBox::Impl {
public:
    struct Item final {
        std::string text;
        std::uintptr_t value{};
        std::shared_ptr<const render::DuiImage> icon;
        bool selected{};
        bool checked{};
    };

    input::DuiScrollBar* scrollBar{};
    std::vector<Item> items;
    std::function<void(int)> selectionChanged;
    std::function<void(int, bool)> checkChanged;
    std::function<void(int)> reordered;
    std::function<void(int, bool)> itemClicked;
    render::DuiTextStyle textStyle;
    core::Color background{255, 255, 255, 255};
    core::Color selectedColor{180, 210, 245, 255};
    core::Color selectedTextColor{10, 30, 90, 255};
    core::Color hoverColor{232, 240, 252, 255};
    int selectedIndex{-1};
    int hoveredIndex{-1};
    int rowHeight{DefaultRowHeight};
    int dragSource{-1};
    int dragInsertion{-1};
    bool multiSelect{};
    bool checkboxesVisible{};
    bool reorderEnabled{};
    std::shared_ptr<const render::DuiImage> leadingIcon;
    core::Size leadingIconSize{16, 16};
    int leadingIconGap{8};
    bool textStyleOverride{};
    bool backgroundOverride{};
    bool selectedColorOverride{};
    bool selectedTextColorOverride{};
    bool hoverColorOverride{};
};

DuiListBox::DuiListBox() : listBox_(std::make_unique<Impl>())
{
    auto scrollBar = std::make_unique<input::DuiScrollBar>();
    listBox_->scrollBar = scrollBar.get();
    scrollBar->SetLineSize(DefaultRowHeight);
    AddChild(std::move(scrollBar));
}

DuiListBox::~DuiListBox() = default;

int DuiListBox::AddItem(std::string text, std::uintptr_t value)
{
    listBox_->items.push_back({std::move(text), value, {}});
    UpdateScrollRange();
    return Count() - 1;
}

void DuiListBox::InsertItem(int index, std::string text, std::uintptr_t value)
{
    index = std::clamp(index, 0, Count());
    listBox_->items.insert(listBox_->items.begin() + index, {std::move(text), value, {}});
    if (listBox_->selectedIndex >= index)
        ++listBox_->selectedIndex;
    UpdateScrollRange();
}

void DuiListBox::RemoveItem(int index)
{
    if (index < 0 || index >= Count())
        return;
    listBox_->items.erase(listBox_->items.begin() + index);
    if (listBox_->selectedIndex == index)
        listBox_->selectedIndex = -1;
    else if (listBox_->selectedIndex > index)
        --listBox_->selectedIndex;
    if (listBox_->hoveredIndex == index)
        listBox_->hoveredIndex = -1;
    UpdateScrollRange();
}

void DuiListBox::ClearItems()
{
    listBox_->items.clear();
    listBox_->selectedIndex = -1;
    listBox_->hoveredIndex = -1;
    UpdateScrollRange();
}

int DuiListBox::Count() const { return static_cast<int>(listBox_->items.size()); }
std::string DuiListBox::TextAt(int index) const { return index >= 0 && index < Count() ? listBox_->items[index].text : std::string{}; }
std::uintptr_t DuiListBox::ValueAt(int index) const { return index >= 0 && index < Count() ? listBox_->items[index].value : 0; }
void DuiListBox::SetTextAt(int index, std::string text) { if (index >= 0 && index < Count()) listBox_->items[index].text = std::move(text); }
void DuiListBox::SetValueAt(int index, std::uintptr_t value) { if (index >= 0 && index < Count()) listBox_->items[index].value = value; }
void DuiListBox::SetIconAt(int index, std::shared_ptr<const render::DuiImage> icon)
{
    if (index >= 0 && index < Count())
        listBox_->items[index].icon = std::move(icon);
}

const std::shared_ptr<const render::DuiImage>& DuiListBox::IconAt(int index) const
{
    static const std::shared_ptr<const render::DuiImage> empty;
    return index >= 0 && index < Count() ? listBox_->items[index].icon : empty;
}

void DuiListBox::SetSelectedIndex(int index, bool notify)
{
    if (index < -1 || index >= Count() || index == listBox_->selectedIndex)
        return;
    listBox_->selectedIndex = index;
    if (!listBox_->multiSelect) {
        for (int itemIndex = 0; itemIndex < Count(); ++itemIndex)
            listBox_->items[itemIndex].selected = itemIndex == index;
    }
    if (index >= 0)
        EnsureVisible(index);
    if (notify && listBox_->selectionChanged)
        listBox_->selectionChanged(index);
}

int DuiListBox::SelectedIndex() const { return listBox_->selectedIndex; }

void DuiListBox::SetMultiSelect(bool enabled)
{
    if (listBox_->multiSelect == enabled)
        return;
    listBox_->multiSelect = enabled;
    if (!enabled) {
        for (int index = 0; index < Count(); ++index)
            listBox_->items[index].selected = index == listBox_->selectedIndex;
    }
}

bool DuiListBox::MultiSelect() const { return listBox_->multiSelect; }
bool DuiListBox::IsSelected(int index) const { return index >= 0 && index < Count() && listBox_->items[index].selected; }

void DuiListBox::SetSelected(int index, bool selected, bool notify)
{
    if (index < 0 || index >= Count() || listBox_->items[index].selected == selected)
        return;
    if (selected && !listBox_->multiSelect) {
        for (int itemIndex = 0; itemIndex < Count(); ++itemIndex)
            listBox_->items[itemIndex].selected = itemIndex == index;
    } else {
        listBox_->items[index].selected = selected;
    }
    if (selected)
        listBox_->selectedIndex = index;
    if (notify && listBox_->selectionChanged)
        listBox_->selectionChanged(index);
}

int DuiListBox::SelectionCount() const
{
    return static_cast<int>(std::count_if(listBox_->items.begin(), listBox_->items.end(),
        [](const Impl::Item& item) { return item.selected; }));
}

std::vector<int> DuiListBox::SelectedIndices() const
{
    std::vector<int> result;
    for (int index = 0; index < Count(); ++index) {
        if (listBox_->items[index].selected)
            result.push_back(index);
    }
    return result;
}

void DuiListBox::ClearSelection(bool notify)
{
    const bool hadSelection = SelectionCount() != 0 || listBox_->selectedIndex >= 0;
    for (Impl::Item& item : listBox_->items)
        item.selected = false;
    listBox_->selectedIndex = -1;
    if (notify && hadSelection && listBox_->selectionChanged)
        listBox_->selectionChanged(-1);
}

void DuiListBox::SetCheckboxesVisible(bool visible) { listBox_->checkboxesVisible = visible; }
bool DuiListBox::CheckboxesVisible() const { return listBox_->checkboxesVisible; }
bool DuiListBox::IsChecked(int index) const { return index >= 0 && index < Count() && listBox_->items[index].checked; }
void DuiListBox::SetChecked(int index, bool checked, bool notify)
{
    if (index < 0 || index >= Count() || listBox_->items[index].checked == checked)
        return;
    listBox_->items[index].checked = checked;
    if (notify && listBox_->checkChanged)
        listBox_->checkChanged(index, checked);
}

void DuiListBox::SetReorderEnabled(bool enabled) { listBox_->reorderEnabled = enabled; }
bool DuiListBox::ReorderEnabled() const { return listBox_->reorderEnabled; }

void DuiListBox::MoveItem(int from, int insertionIndex, bool notify)
{
    if (from < 0 || from >= Count())
        return;
    insertionIndex = std::clamp(insertionIndex, 0, Count());
    if (insertionIndex > from)
        --insertionIndex;
    if (from == insertionIndex)
        return;
    Impl::Item item = std::move(listBox_->items[from]);
    listBox_->items.erase(listBox_->items.begin() + from);
    listBox_->items.insert(listBox_->items.begin() + insertionIndex, std::move(item));
    if (listBox_->selectedIndex == from)
        listBox_->selectedIndex = insertionIndex;
    else if (from < listBox_->selectedIndex && listBox_->selectedIndex <= insertionIndex)
        --listBox_->selectedIndex;
    else if (insertionIndex <= listBox_->selectedIndex && listBox_->selectedIndex < from)
        ++listBox_->selectedIndex;
    if (notify && listBox_->reordered)
        listBox_->reordered(insertionIndex);
}

void DuiListBox::SetRowHeight(int pixels)
{
    listBox_->rowHeight = (std::max)(1, pixels);
    listBox_->scrollBar->SetLineSize(listBox_->rowHeight);
    UpdateScrollRange();
}

int DuiListBox::RowHeight() const { return listBox_->rowHeight; }
void DuiListBox::SetScrollPosition(int pixels) { listBox_->scrollBar->SetPosition(pixels, false); }
int DuiListBox::ScrollPosition() const { return listBox_->scrollBar->Position(); }

void DuiListBox::EnsureVisible(int index)
{
    if (index < 0 || index >= Count())
        return;
    if (Bounds().Height() <= 0)
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

int DuiListBox::BodyWidth() const { return Bounds().Width() - (listBox_->scrollBar->Visible() ? ScrollBarWidth : 0); }
int DuiListBox::VisibleRows() const { return (std::max)(1, Bounds().Height() / RowHeight()); }
core::Rect DuiListBox::RowRect(int index) const
{
    const int top = Bounds().top + index * RowHeight() - ScrollPosition();
    return {Bounds().left, top, Bounds().left + BodyWidth(), top + RowHeight()};
}

int DuiListBox::IndexFromPoint(core::Point point) const
{
    if (!Bounds().Contains(point) || point.x >= Bounds().left + BodyWidth())
        return -1;
    const int index = (point.y - Bounds().top + ScrollPosition()) / RowHeight();
    return index >= 0 && index < Count() ? index : -1;
}

void DuiListBox::SetLeadingIcon(std::shared_ptr<const render::DuiImage> icon)
{
    listBox_->leadingIcon = std::move(icon);
}

const std::shared_ptr<const render::DuiImage>& DuiListBox::LeadingIcon() const
{
    return listBox_->leadingIcon;
}

void DuiListBox::SetLeadingIconSize(core::Size size)
{
    listBox_->leadingIconSize = {(std::max)(0, size.width), (std::max)(0, size.height)};
}

core::Size DuiListBox::LeadingIconSize() const { return listBox_->leadingIconSize; }

void DuiListBox::SetLeadingIconGap(int pixels) { listBox_->leadingIconGap = (std::max)(0, pixels); }

int DuiListBox::LeadingIconGap() const { return listBox_->leadingIconGap; }

void DuiListBox::SetTextStyle(render::DuiTextStyle style) { listBox_->textStyle = std::move(style); listBox_->textStyleOverride = true; }
void DuiListBox::SetBackgroundColor(core::Color color) { listBox_->background = color; listBox_->backgroundOverride = true; }
void DuiListBox::SetTextColor(core::Color color) { listBox_->textStyle.color = color; listBox_->textStyleOverride = true; }
void DuiListBox::SetSelectedColor(core::Color color) { listBox_->selectedColor = color; listBox_->selectedColorOverride = true; }
void DuiListBox::SetSelectedTextColor(core::Color color) { listBox_->selectedTextColor = color; listBox_->selectedTextColorOverride = true; }
void DuiListBox::SetHoverColor(core::Color color) { listBox_->hoverColor = color; listBox_->hoverColorOverride = true; }
void DuiListBox::SetSelectionChangedHandler(std::function<void(int)> handler) { listBox_->selectionChanged = std::move(handler); }
void DuiListBox::SetCheckChangedHandler(std::function<void(int, bool)> handler) { listBox_->checkChanged = std::move(handler); }
void DuiListBox::SetReorderedHandler(std::function<void(int)> handler) { listBox_->reordered = std::move(handler); }
void DuiListBox::SetItemClickedHandler(std::function<void(int, bool)> handler) { listBox_->itemClicked = std::move(handler); }

void DuiListBox::UpdateScrollRange()
{
    const int overflow = (std::max)(0, Count() * RowHeight() - Bounds().Height());
    listBox_->scrollBar->SetRange(0, overflow);
    listBox_->scrollBar->SetPageSize((std::max)(1, Bounds().Height()));
    listBox_->scrollBar->SetVisible(overflow > 0);
}

void DuiListBox::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    UpdateScrollRange();
    if (listBox_->scrollBar->Visible())
        listBox_->scrollBar->SetBounds({bounds.right - ScrollBarWidth, bounds.top, bounds.right, bounds.bottom});
}

bool DuiListBox::OnEvent(const core::Event& event)
{
    if (!Enabled())
        return false;
    if (event.type == core::EventType::PointerMove) {
        if (listBox_->dragSource >= 0) {
            const int relativeY = event.position.y - Bounds().top + ScrollPosition();
            int insertion = std::clamp(relativeY / RowHeight(), 0, Count());
            if (insertion < Count() && relativeY % RowHeight() > RowHeight() / 2)
                ++insertion;
            listBox_->dragInsertion = insertion;
            return true;
        }
        listBox_->hoveredIndex = IndexFromPoint(event.position);
        return listBox_->hoveredIndex >= 0;
    }
    if (event.type == core::EventType::PointerWheel && Bounds().Contains(event.position)) {
        if (event.wheelDelta == 0)
            return false;
        listBox_->scrollBar->SetPosition(ScrollPosition() + (event.wheelDelta > 0 ? -3 : 3) * RowHeight());
        return true;
    }
    if (event.type == core::EventType::PointerDown || event.type == core::EventType::PointerDoubleClick) {
        const int index = IndexFromPoint(event.position);
        if (index < 0)
            return false;
        if (listBox_->checkboxesVisible && event.position.x < Bounds().left + CheckboxColumnWidth) {
            SetChecked(index, !IsChecked(index));
            return true;
        }
        if (listBox_->multiSelect && (event.modifiers & core::modifier::Shift) != 0 && SelectedIndex() >= 0) {
            const int first = (std::min)(SelectedIndex(), index);
            const int last = (std::max)(SelectedIndex(), index);
            for (int itemIndex = 0; itemIndex < Count(); ++itemIndex)
                listBox_->items[itemIndex].selected = itemIndex >= first && itemIndex <= last;
            SetSelectedIndex(index);
        } else if (listBox_->multiSelect && (event.modifiers & core::modifier::Control) != 0) {
            SetSelected(index, !IsSelected(index));
        } else {
            SetSelectedIndex(index);
        }
        if (event.type == core::EventType::PointerDown && listBox_->reorderEnabled) {
            listBox_->dragSource = index;
            listBox_->dragInsertion = -1;
            SetCaptured(true);
        }
        if (listBox_->itemClicked)
            listBox_->itemClicked(index, event.type == core::EventType::PointerDoubleClick);
        return true;
    }
    if (event.type == core::EventType::PointerCancel && listBox_->dragSource >= 0) {
        listBox_->dragSource = -1;
        listBox_->dragInsertion = -1;
        SetCaptured(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && listBox_->dragSource >= 0) {
        const int source = listBox_->dragSource;
        const int insertion = listBox_->dragInsertion;
        listBox_->dragSource = -1;
        listBox_->dragInsertion = -1;
        SetCaptured(false);
        if (insertion >= 0 && insertion != source && insertion != source + 1)
            MoveItem(source, insertion);
        return true;
    }
    if (event.type == core::EventType::KeyDown && Count() > 0) {
        int next = SelectedIndex();
        if (event.key == core::key::Up)
            next = (std::max)(0, next - 1);
        else if (event.key == core::key::Down)
            next = (std::min)(Count() - 1, next + 1);
        else if (event.key == core::key::Home)
            next = 0;
        else if (event.key == core::key::End)
            next = Count() - 1;
        else if (event.key == core::key::PageUp)
            next = (std::max)(0, next - VisibleRows());
        else if (event.key == core::key::PageDown)
            next = (std::min)(Count() - 1, next + VisibleRows());
        else if ((event.key == core::key::Enter || event.key == core::key::Space) && next >= 0) {
            if (event.key == core::key::Space && listBox_->checkboxesVisible)
                SetChecked(next, !IsChecked(next));
            else if (listBox_->itemClicked)
                listBox_->itemClicked(next, false);
            return true;
        } else {
            return false;
        }
        SetSelectedIndex(next);
        return true;
    }
    return false;
}

void DuiListBox::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    const core::Color background = listBox_->backgroundOverride
        ? listBox_->background : theme.Get(core::ThemeSlot::ListBackground);
    const core::Color selectedColor = listBox_->selectedColorOverride
        ? listBox_->selectedColor : theme.Get(core::ThemeSlot::ListSelection);
    const core::Color selectedTextColor = listBox_->selectedTextColorOverride
        ? listBox_->selectedTextColor : theme.Get(core::ThemeSlot::ListSelectionText);
    const core::Color hoverColor = listBox_->hoverColorOverride
        ? listBox_->hoverColor : theme.Get(core::ThemeSlot::ListHover);
    render::DuiTextStyle textStyle = listBox_->textStyle;
    if (!listBox_->textStyleOverride)
        textStyle.color = theme.Get(core::ThemeSlot::ListText);
    canvas.FillRect(bounds, background);
    canvas.PushClip({Bounds().left, Bounds().top, Bounds().left + BodyWidth(), Bounds().bottom});
    const int first = ScrollPosition() / RowHeight();
    const int last = (std::min)(Count(), first + VisibleRows() + 1);
    for (int index = first; index < last; ++index) {
        const core::Rect row = RowRect(index);
        if (core::Rect::Intersect(row, dirty).Empty())
            continue;
        const Impl::Item& item = listBox_->items[index];
        if (item.selected)
            canvas.FillRect(row, selectedColor);
        else if (index == listBox_->hoveredIndex)
            canvas.FillRect(row, hoverColor);
        int textLeft = row.left + TextPadding;
        if (listBox_->checkboxesVisible) {
            const core::Rect checkbox{row.left + 4, row.top + 4, row.left + CheckboxColumnWidth - 4, row.bottom - 4};
            canvas.StrokeRoundedRect(checkbox, 2, theme.Get(core::ThemeSlot::ListCheckboxBorder), 1.0F);
            if (item.checked)
                canvas.FillRoundedRect(checkbox, 2, theme.Get(core::ThemeSlot::ListCheckboxFill));
            textLeft = row.left + CheckboxColumnWidth + TextPadding;
        }
        const auto& icon = item.icon ? item.icon : listBox_->leadingIcon;
        if (icon && !icon->Empty()
            && listBox_->leadingIconSize.width > 0 && listBox_->leadingIconSize.height > 0)
        {
            const core::Size iconSize = listBox_->leadingIconSize;
            const int iconTop = row.top + (row.Height() - iconSize.height) / 2;
            const core::Rect iconBounds{textLeft, iconTop,
                                        textLeft + iconSize.width, iconTop + iconSize.height};
            const core::Size sourceSize = icon->Size();
            canvas.DrawImage(*icon, {0, 0, sourceSize.width, sourceSize.height}, iconBounds);
            textLeft = iconBounds.right + listBox_->leadingIconGap;
        }
        render::DuiTextStyle style = textStyle;
        if (item.selected)
            style.color = selectedTextColor;
        canvas.DrawText(item.text, {textLeft, row.top, row.right - TextPadding, row.bottom}, style,
                        render::DuiTextAlignment::Start, false);
    }
    if (listBox_->dragSource >= 0 && listBox_->dragInsertion >= 0) {
        const int y = Bounds().top + listBox_->dragInsertion * RowHeight() - ScrollPosition();
        canvas.FillRect({Bounds().left, y, Bounds().left + BodyWidth(), y + 2},
                        theme.Get(core::ThemeSlot::ListInsertion));
    }
    canvas.PopClip();
    if (listBox_->scrollBar->Visible())
        listBox_->scrollBar->Paint(canvas, dirty);
    // 列表底色与页面底色同为浅色：靠外框区分控件范围
    canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::GridBorder), 1.0F);
}

} // namespace ysDui::controls::list
