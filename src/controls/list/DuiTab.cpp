#include "ysDui/controls/list/DuiTab.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/render/DuiFocusVisual.hpp"
#include "ysDui/render/DuiPath.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::list {
namespace {
constexpr int ArrowWidth = 18;
constexpr int CloseSize = 14;
constexpr int DropdownSize = 12;
constexpr int DragThreshold = 4;
constexpr int GlyphPadding = 3;
const std::shared_ptr<const render::DuiImage> EmptyImage;
}

class DuiTab::Impl {
public:
    struct Item {
        std::string text;
        std::uintptr_t value{};
        std::shared_ptr<const render::DuiImage> icon;
        bool closeable{};
        bool dropdown{};
    };

    enum class HitZone { None, Tab, Close, Dropdown };
    std::vector<Item> items;
    render::DuiTextMeasurer* measurer{};
    std::function<void(int)> selectionChanged;
    std::function<void(int)> closed;
    std::function<void(int)> dropdownRequested;
    std::function<void(int, int)> reordered;
    render::DuiTextStyle textStyle{{50, 50, 60, 255}, {}, 9, false};
    core::Color background{238, 240, 244, 255};
    core::Color tabColor{238, 240, 244, 255};
    core::Color hover{228, 232, 240, 255};
    core::Color selected{255, 255, 255, 255};
    core::Color text{50, 50, 60, 255};
    core::Color selectedText{22, 93, 255, 255};
    core::Color border{212, 215, 222, 255};
    int selectedIndex{-1};
    int hoveredIndex{-1};
    int pressedIndex{-1};
    int pressedX{};
    int dragSource{-1};
    int dragSlot{-1};
    int tabHeight{28};
    int minWidth{60};
    int maxWidth{200};
    int padding{12};
    int gap{2};
    int iconSize{16};
    int iconGap{6};
    int scrollOffset{};
    int scrollStep{60};
    bool autoFit{};
    bool wheelSelect{true};
    bool backgroundOverride{};
    bool tabColorOverride{};
    bool hoverOverride{};
    bool selectedOverride{};
    bool textOverride{};
    bool selectedTextOverride{};
    bool borderOverride{};
    bool reorderEnabled{};
    HitZone hoveredZone{HitZone::None};
    HitZone pressedZone{HitZone::None};

    int ItemWidth(const Item& item) const {
        int textWidth = static_cast<int>(item.text.size()) * (std::max)(1, textStyle.pointSize);
        if (measurer) textWidth = measurer->MeasureText(item.text, textStyle, {}).size.width;
        int width = textWidth + padding * 2;
        if (item.icon && !item.icon->Empty()) width += iconSize + iconGap;
        if (item.closeable) width += CloseSize + 6;
        if (item.dropdown) width += DropdownSize + 6;
        return autoFit ? width : (std::clamp)(width, minWidth, maxWidth);
    }

    int TotalWidth() const {
        int width{};
        for (int index = 0; index < static_cast<int>(items.size()); ++index) {
            width += ItemWidth(items[index]);
            if (index + 1 < static_cast<int>(items.size())) width += gap;
        }
        return width;
    }
};

DuiTab::DuiTab() : tab_(std::make_unique<Impl>()) {}
DuiTab::~DuiTab() = default;
DuiTab::DuiTab(DuiTab&&) noexcept = default;
DuiTab& DuiTab::operator=(DuiTab&&) noexcept = default;

int DuiTab::AddTab(std::string text, bool closeable, bool dropdown, std::uintptr_t value,
                   std::shared_ptr<const render::DuiImage> icon) {
    InsertTab(Count(), std::move(text), closeable, dropdown, value, std::move(icon));
    return Count() - 1;
}

void DuiTab::InsertTab(int index, std::string text, bool closeable, bool dropdown,
                       std::uintptr_t value, std::shared_ptr<const render::DuiImage> icon) {
    index = (std::clamp)(index, 0, Count());
    tab_->items.insert(tab_->items.begin() + index, {std::move(text), value, std::move(icon), closeable, dropdown});
    if (tab_->selectedIndex >= index) ++tab_->selectedIndex;
    if (tab_->selectedIndex < 0) tab_->selectedIndex = 0;
    SetScrollOffset(tab_->scrollOffset);
}

void DuiTab::RemoveTab(int index) {
    if (index < 0 || index >= Count()) return;
    tab_->items.erase(tab_->items.begin() + index);
    if (tab_->items.empty()) tab_->selectedIndex = -1;
    else if (tab_->selectedIndex == index) tab_->selectedIndex = (std::min)(index, Count() - 1);
    else if (tab_->selectedIndex > index) --tab_->selectedIndex;
    if (tab_->hoveredIndex >= Count()) tab_->hoveredIndex = -1;
    SetScrollOffset(tab_->scrollOffset);
}

void DuiTab::ClearTabs() { tab_->items.clear(); tab_->selectedIndex = -1; tab_->hoveredIndex = -1; tab_->pressedIndex = -1; tab_->dragSource = -1; SetCaptured(false); }
int DuiTab::Count() const { return static_cast<int>(tab_->items.size()); }
std::string DuiTab::TextAt(int index) const { return index >= 0 && index < Count() ? tab_->items[index].text : std::string{}; }
void DuiTab::SetTextAt(int index, std::string text) { if (index >= 0 && index < Count()) { tab_->items[index].text = std::move(text); SetScrollOffset(tab_->scrollOffset); } }
std::uintptr_t DuiTab::ValueAt(int index) const { return index >= 0 && index < Count() ? tab_->items[index].value : 0; }
void DuiTab::SetValueAt(int index, std::uintptr_t value) { if (index >= 0 && index < Count()) tab_->items[index].value = value; }
int DuiTab::FindByValue(std::uintptr_t value) const { for (int index = 0; index < Count(); ++index) if (tab_->items[index].value == value) return index; return -1; }
void DuiTab::SetCloseable(int index, bool closeable) { if (index >= 0 && index < Count()) { tab_->items[index].closeable = closeable; SetScrollOffset(tab_->scrollOffset); } }
bool DuiTab::Closeable(int index) const { return index >= 0 && index < Count() && tab_->items[index].closeable; }
void DuiTab::SetDropdown(int index, bool dropdown) { if (index >= 0 && index < Count()) { tab_->items[index].dropdown = dropdown; SetScrollOffset(tab_->scrollOffset); } }
bool DuiTab::Dropdown(int index) const { return index >= 0 && index < Count() && tab_->items[index].dropdown; }
void DuiTab::SetIcon(int index, std::shared_ptr<const render::DuiImage> icon) { if (index >= 0 && index < Count()) { tab_->items[index].icon = std::move(icon); SetScrollOffset(tab_->scrollOffset); } }
const std::shared_ptr<const render::DuiImage>& DuiTab::IconAt(int index) const { return index >= 0 && index < Count() ? tab_->items[index].icon : EmptyImage; }

void DuiTab::SetSelectedIndex(int index, bool notify) {
    if (index < -1 || index >= Count() || index == tab_->selectedIndex) return;
    tab_->selectedIndex = index;
    if (index >= 0) EnsureVisible(index);
    if (notify && tab_->selectionChanged) tab_->selectionChanged(index);
}
int DuiTab::SelectedIndex() const { return tab_->selectedIndex; }
void DuiTab::SetSelectionChangedHandler(std::function<void(int)> handler) { tab_->selectionChanged = std::move(handler); }
void DuiTab::SetCloseHandler(std::function<void(int)> handler) { tab_->closed = std::move(handler); }
void DuiTab::SetDropdownHandler(std::function<void(int)> handler) { tab_->dropdownRequested = std::move(handler); }
void DuiTab::SetReorderedHandler(std::function<void(int, int)> handler) { tab_->reordered = std::move(handler); }
void DuiTab::SetTextMeasurer(render::DuiTextMeasurer* measurer) { tab_->measurer = measurer; SetScrollOffset(tab_->scrollOffset); }
void DuiTab::SetTextStyle(render::DuiTextStyle style) { tab_->textStyle = std::move(style); SetScrollOffset(tab_->scrollOffset); }
void DuiTab::SetTabHeight(int pixels) { tab_->tabHeight = (std::max)(1, pixels); }
int DuiTab::TabHeight() const { return tab_->tabHeight; }
void DuiTab::SetMinTabWidth(int pixels) { tab_->minWidth = (std::max)(0, pixels); SetScrollOffset(tab_->scrollOffset); }
void DuiTab::SetMaxTabWidth(int pixels) { tab_->maxWidth = (std::max)(1, pixels); SetScrollOffset(tab_->scrollOffset); }
void DuiTab::SetTabPadding(int pixels) { tab_->padding = (std::max)(0, pixels); SetScrollOffset(tab_->scrollOffset); }
void DuiTab::SetGap(int pixels) { tab_->gap = (std::max)(0, pixels); SetScrollOffset(tab_->scrollOffset); }
void DuiTab::SetIconSize(int pixels) { tab_->iconSize = (std::max)(1, pixels); SetScrollOffset(tab_->scrollOffset); }
void DuiTab::SetIconGap(int pixels) { tab_->iconGap = (std::max)(0, pixels); SetScrollOffset(tab_->scrollOffset); }
void DuiTab::SetAutoFitTabWidth(bool enabled) { tab_->autoFit = enabled; SetScrollOffset(tab_->scrollOffset); }
void DuiTab::SetWheelSelect(bool enabled) { tab_->wheelSelect = enabled; }
void DuiTab::SetReorderEnabled(bool enabled) { tab_->reorderEnabled = enabled; }
void DuiTab::SetScrollStepPixels(int pixels) { tab_->scrollStep = (std::max)(1, pixels); }

bool DuiTab::NeedsScroll() const { return tab_->TotalWidth() > Bounds().Width(); }
int DuiTab::MaxScrollOffset() const { const int viewport = Bounds().Width() - (NeedsScroll() ? ArrowWidth * 2 : 0); return (std::max)(0, tab_->TotalWidth() - viewport); }
bool DuiTab::CanScrollLeft() const { return NeedsScroll() && tab_->scrollOffset > 0; }
bool DuiTab::CanScrollRight() const { return NeedsScroll() && tab_->scrollOffset < MaxScrollOffset(); }
void DuiTab::SetScrollOffset(int pixels) { tab_->scrollOffset = (std::clamp)(pixels, 0, MaxScrollOffset()); }
int DuiTab::ScrollOffset() const { return tab_->scrollOffset; }
core::Rect DuiTab::LeftArrowRect() const { return NeedsScroll() ? core::Rect{Bounds().left, Bounds().bottom - TabHeight(), Bounds().left + ArrowWidth, Bounds().bottom} : core::Rect{}; }
core::Rect DuiTab::RightArrowRect() const { return NeedsScroll() ? core::Rect{Bounds().right - ArrowWidth, Bounds().bottom - TabHeight(), Bounds().right, Bounds().bottom} : core::Rect{}; }
core::Rect DuiTab::TabRect(int index) const {
    if (index < 0 || index >= Count()) return {};
    const int contentLeft = Bounds().left + (NeedsScroll() ? ArrowWidth : 0);
    int left = contentLeft - tab_->scrollOffset;
    for (int current = 0; current < index; ++current) left += tab_->ItemWidth(tab_->items[current]) + tab_->gap;
    return {left, Bounds().bottom - TabHeight(), left + tab_->ItemWidth(tab_->items[index]), Bounds().bottom};
}
core::Rect DuiTab::CloseRect(int index) const { if (!Closeable(index)) return {}; const core::Rect tabRect = TabRect(index); const int right = tabRect.right - tab_->padding; return {right - CloseSize, tabRect.top + (tabRect.Height() - CloseSize) / 2, right, tabRect.top + (tabRect.Height() + CloseSize) / 2}; }
core::Rect DuiTab::DropdownRect(int index) const { if (!Dropdown(index)) return {}; const core::Rect close = CloseRect(index); const core::Rect tabRect = TabRect(index); const int right = close.Empty() ? tabRect.right - tab_->padding : close.left - 6; return {right - DropdownSize, tabRect.top + (tabRect.Height() - DropdownSize) / 2, right, tabRect.top + (tabRect.Height() + DropdownSize) / 2}; }
int DuiTab::HitTestIndex(core::Point point) const { for (int index = 0; index < Count(); ++index) if (TabRect(index).Contains(point)) return index; return -1; }
void DuiTab::EnsureVisible(int index) { if (index < 0 || index >= Count() || !NeedsScroll()) return; const core::Rect rect = TabRect(index); const int left = Bounds().left + ArrowWidth; const int right = Bounds().right - ArrowWidth; if (rect.left < left) SetScrollOffset(tab_->scrollOffset - (left - rect.left)); else if (rect.right > right) SetScrollOffset(tab_->scrollOffset + (rect.right - right)); }
void DuiTab::MoveTab(int from, int insertionIndex) {
    if (from < 0 || from >= Count()) return;
    insertionIndex = (std::clamp)(insertionIndex, 0, Count());
    if (insertionIndex > from) --insertionIndex;
    if (from == insertionIndex) return;
    Impl::Item item = std::move(tab_->items[from]);
    tab_->items.erase(tab_->items.begin() + from);
    tab_->items.insert(tab_->items.begin() + insertionIndex, std::move(item));
    if (tab_->selectedIndex == from) tab_->selectedIndex = insertionIndex;
    else if (from < tab_->selectedIndex && tab_->selectedIndex <= insertionIndex) --tab_->selectedIndex;
    else if (insertionIndex <= tab_->selectedIndex && tab_->selectedIndex < from) ++tab_->selectedIndex;
    if (tab_->reordered) tab_->reordered(from, insertionIndex);
    EnsureVisible(tab_->selectedIndex);
}
core::Size DuiTab::DesiredSize() const { return {(std::max)(120, tab_->TotalWidth()), TabHeight()}; }
core::DuiAccessibilityData DuiTab::CreateAccessibilityData() const {
    const std::string value = tab_->selectedIndex >= 0 && tab_->selectedIndex < Count()
        ? tab_->items[tab_->selectedIndex].text : std::string{};
    return {core::DuiAccessibilityRole::Tab, {}, value, {}, true, {}};
}
void DuiTab::Layout(core::Rect bounds) { SetBounds(bounds); SetScrollOffset(tab_->scrollOffset); }

bool DuiTab::OnEvent(const core::Event& event) {
    if (!Enabled()) return false;
    if (event.type == core::EventType::KeyDown) {
        if (Count() == 0) return false;
        int next = tab_->selectedIndex < 0 ? 0 : tab_->selectedIndex;
        if (event.key == core::key::Left || event.key == core::key::Up)
            next = (next - 1 + Count()) % Count();
        else if (event.key == core::key::Right || event.key == core::key::Down)
            next = (next + 1) % Count();
        else if (event.key == core::key::Home) next = 0;
        else if (event.key == core::key::End) next = Count() - 1;
        else return false;
        if (next != tab_->selectedIndex) SetSelectedIndex(next);
        return true;
    }
    if (event.type == core::EventType::PointerWheel) {
        if (!tab_->wheelSelect || Count() == 0) return false;
        const int next = event.wheelDelta > 0 ? tab_->selectedIndex - 1 : tab_->selectedIndex + 1;
        if (next < 0 || next >= Count()) return false;
        SetSelectedIndex(next); return true;
    }
    if (event.type == core::EventType::PointerMove) {
        if (tab_->dragSource >= 0 && tab_->reorderEnabled) {
            if (std::abs(event.position.x - tab_->pressedX) >= DragThreshold) {
                int slot = Count();
                for (int index = 0; index < Count(); ++index) if (event.position.x < (TabRect(index).left + TabRect(index).right) / 2) { slot = index; break; }
                tab_->dragSlot = slot;
            }
            return true;
        }
        tab_->hoveredIndex = HitTestIndex(event.position);
        tab_->hoveredZone = tab_->hoveredIndex < 0 ? Impl::HitZone::None : CloseRect(tab_->hoveredIndex).Contains(event.position) ? Impl::HitZone::Close : DropdownRect(tab_->hoveredIndex).Contains(event.position) ? Impl::HitZone::Dropdown : Impl::HitZone::Tab;
        return tab_->hoveredIndex >= 0;
    }
    if (event.type == core::EventType::PointerDown) {
        if (LeftArrowRect().Contains(event.position)) { SetScrollOffset(tab_->scrollOffset - tab_->scrollStep); return true; }
        if (RightArrowRect().Contains(event.position)) { SetScrollOffset(tab_->scrollOffset + tab_->scrollStep); return true; }
        tab_->pressedIndex = HitTestIndex(event.position);
        if (tab_->pressedIndex < 0) return false;
        tab_->pressedX = event.position.x;
        tab_->pressedZone = CloseRect(tab_->pressedIndex).Contains(event.position) ? Impl::HitZone::Close : DropdownRect(tab_->pressedIndex).Contains(event.position) ? Impl::HitZone::Dropdown : Impl::HitZone::Tab;
        if (tab_->pressedZone == Impl::HitZone::Tab && tab_->reorderEnabled) tab_->dragSource = tab_->pressedIndex;
        SetCaptured(true); return true;
    }
    if (event.type == core::EventType::PointerCancel && Captured()) {
        tab_->pressedIndex = -1;
        tab_->dragSource = -1;
        tab_->dragSlot = -1;
        tab_->pressedZone = Impl::HitZone::None;
        SetCaptured(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp) {
        const int pressed = tab_->pressedIndex;
        const int current = HitTestIndex(event.position);
        const bool reordered = tab_->dragSource >= 0 && tab_->dragSlot >= 0 && std::abs(event.position.x - tab_->pressedX) >= DragThreshold;
        const int slot = tab_->dragSlot;
        tab_->pressedIndex = -1; tab_->dragSource = -1; tab_->dragSlot = -1; SetCaptured(false);
        if (reordered) { MoveTab(pressed, slot); return true; }
        if (pressed < 0 || pressed != current) return false;
        const Impl::HitZone zone = CloseRect(current).Contains(event.position) ? Impl::HitZone::Close : DropdownRect(current).Contains(event.position) ? Impl::HitZone::Dropdown : Impl::HitZone::Tab;
        if (zone != tab_->pressedZone) return false;
        if (zone == Impl::HitZone::Close && tab_->closed) tab_->closed(current);
        else if (zone == Impl::HitZone::Dropdown && tab_->dropdownRequested) tab_->dropdownRequested(current);
        else if (zone == Impl::HitZone::Tab) SetSelectedIndex(current);
        return true;
    }
    return false;
}

void DuiTab::SetBackgroundColor(core::Color color) { tab_->background = color; tab_->backgroundOverride = true; }
void DuiTab::SetTabColor(core::Color color) { tab_->tabColor = color; tab_->tabColorOverride = true; }
void DuiTab::SetHoverColor(core::Color color) { tab_->hover = color; tab_->hoverOverride = true; }
void DuiTab::SetSelectedColor(core::Color color) { tab_->selected = color; tab_->selectedOverride = true; }
void DuiTab::SetTextColor(core::Color color) { tab_->text = color; tab_->textOverride = true; }
void DuiTab::SetSelectedTextColor(core::Color color) { tab_->selectedText = color; tab_->selectedTextOverride = true; }
void DuiTab::SetBorderColor(core::Color color) { tab_->border = color; tab_->borderOverride = true; }

void DuiTab::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible()) return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty()) return;
    const core::DuiTheme& theme = Theme();
    const core::Color background = tab_->backgroundOverride
        ? tab_->background : theme.Get(core::ThemeSlot::TabBackground);
    const core::Color tabColor = tab_->tabColorOverride
        ? tab_->tabColor : theme.Get(core::ThemeSlot::TabIdle);
    const core::Color hover = tab_->hoverOverride
        ? tab_->hover : theme.Get(core::ThemeSlot::TabHover);
    const core::Color selectedColor = tab_->selectedOverride
        ? tab_->selected : theme.Get(core::ThemeSlot::TabSelected);
    const core::Color textColor = tab_->textOverride
        ? tab_->text : theme.Get(core::ThemeSlot::TabText);
    const core::Color selectedTextColor = tab_->selectedTextOverride
        ? tab_->selectedText : theme.Get(core::ThemeSlot::TabSelectedText);
    const core::Color border = tab_->borderOverride
        ? tab_->border : theme.Get(core::ThemeSlot::TabBorder);
    canvas.FillRect(bounds, background);
    const core::Rect strip{Bounds().left, Bounds().bottom - TabHeight(), Bounds().right, Bounds().bottom};
    const bool needsScroll = NeedsScroll();
    const int contentLeft = Bounds().left + (needsScroll ? ArrowWidth : 0);
    const int contentRight = Bounds().right - (needsScroll ? ArrowWidth : 0);
    canvas.PushClip(bounds);
    canvas.PushClip(core::Rect::Intersect({contentLeft, strip.top, contentRight, strip.bottom}, dirty));
    for (int index = 0; index < Count(); ++index) {
        const core::Rect rect = TabRect(index);
        if (core::Rect::Intersect(rect, dirty).Empty()) continue;
        const bool selected = index == tab_->selectedIndex;
        canvas.FillRect(rect, selected ? selectedColor : index == tab_->hoveredIndex ? hover : tabColor);
        canvas.StrokeRoundedRect(rect, 0, border, 1.0F);
        const auto& item = tab_->items[index];
        int left = rect.left + tab_->padding;
        if (item.icon && !item.icon->Empty()) { const int top = rect.top + (rect.Height() - tab_->iconSize) / 2; canvas.DrawImage(*item.icon, {left, top, left + tab_->iconSize, top + tab_->iconSize}); left += tab_->iconSize + tab_->iconGap; }
        const core::Rect close = CloseRect(index); const core::Rect dropdown = DropdownRect(index);
        const int right = !dropdown.Empty() ? dropdown.left - 3 : !close.Empty() ? close.left - 3 : rect.right - tab_->padding;
        auto style = tab_->textStyle;
        style.color = !Enabled() ? theme.Get(core::ThemeSlot::TextDisabled)
                                 : selected ? selectedTextColor : textColor;
        canvas.DrawText(item.text, {left, rect.top, right, rect.bottom}, style, render::DuiTextAlignment::Start, false);
        if (!dropdown.Empty()) {
            const bool hovered = index == tab_->hoveredIndex && tab_->hoveredZone == Impl::HitZone::Dropdown;
            render::DuiPath arrow;
            arrow.MoveTo({dropdown.left, dropdown.top + GlyphPadding});
            arrow.LineTo({dropdown.right, dropdown.top + GlyphPadding});
            arrow.LineTo({(dropdown.left + dropdown.right) / 2, dropdown.bottom - GlyphPadding});
            arrow.Close();
            canvas.FillPath(arrow, hovered ? theme.Get(core::ThemeSlot::TabDropdownHover)
                                           : theme.Get(core::ThemeSlot::TabGlyph));
        }
        if (!close.Empty()) {
            const bool hovered = index == tab_->hoveredIndex && tab_->hoveredZone == Impl::HitZone::Close;
            if (hovered) canvas.FillRect(close, theme.Get(core::ThemeSlot::TabCloseHoverBackground));
            render::DuiPath glyph;
            glyph.MoveTo({close.left + GlyphPadding, close.top + GlyphPadding});
            glyph.LineTo({close.right - GlyphPadding, close.bottom - GlyphPadding});
            glyph.MoveTo({close.right - GlyphPadding, close.top + GlyphPadding});
            glyph.LineTo({close.left + GlyphPadding, close.bottom - GlyphPadding});
            canvas.StrokePath(glyph, hovered ? theme.Get(core::ThemeSlot::TabCloseHoverStroke)
                                             : theme.Get(core::ThemeSlot::TabGlyph), 2.0F);
        }
    }
    if (tab_->dragSource >= 0 && tab_->dragSlot >= 0) {
        int x = contentLeft - tab_->scrollOffset;
        for (int index = 0; index < tab_->dragSlot; ++index)
            x += tab_->ItemWidth(tab_->items[index]) + tab_->gap;
        canvas.FillRect({x, strip.top + 2, x + 2, strip.bottom - 2},
                        theme.Get(core::ThemeSlot::BrandPrimary));
    }
    canvas.PopClip();
    if (needsScroll) {
        canvas.FillRect(LeftArrowRect(), background); canvas.FillRect(RightArrowRect(), background);
        const auto drawArrow = [&canvas](core::Rect rect, bool right, core::Color color) {
            render::DuiPath path;
            const int x = (rect.left + rect.right) / 2;
            const int y = (rect.top + rect.bottom) / 2;
            if (right) { path.MoveTo({x - 2, y - 4}); path.LineTo({x + 4, y}); path.LineTo({x - 2, y + 4}); }
            else { path.MoveTo({x + 2, y - 4}); path.LineTo({x - 4, y}); path.LineTo({x + 2, y + 4}); }
            path.Close();
            canvas.FillPath(path, color);
        };
        drawArrow(LeftArrowRect(), false, CanScrollLeft()
            ? theme.Get(core::ThemeSlot::TabScrollArrow)
            : theme.Get(core::ThemeSlot::TabScrollArrowDisabled));
        drawArrow(RightArrowRect(), true, CanScrollRight()
            ? theme.Get(core::ThemeSlot::TabScrollArrow)
            : theme.Get(core::ThemeSlot::TabScrollArrowDisabled));
    }
    render::DuiPath separator;
    separator.MoveTo({strip.left, strip.bottom - 1});
    separator.LineTo({strip.right, strip.bottom - 1});
    canvas.StrokePath(separator, border, 1.0F);
    if (tab_->selectedIndex >= 0) {
        const core::Rect selected = TabRect(tab_->selectedIndex);
        const int left = (std::max)(contentLeft, selected.left);
        const int right = (std::min)(contentRight, selected.right);
        if (left < right) canvas.FillRect({left, strip.bottom - 1, right, strip.bottom}, selectedColor);
    }
    if (Focused() && Enabled()) render::DrawThemedFocusRing(canvas, Bounds(), theme);
    canvas.PopClip();
}

} // namespace ysDui::controls::list
