#include "ysDui/controls/input/DuiComboBox.hpp"

#include <algorithm>
#include <cwctype>
#include <utility>

#include "DuiTextInputPaint.hpp"
#include "ysDui/controls/list/DuiListBox.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiPopupSurface.hpp"

namespace ysDui::controls::input {
class DuiComboBox::Impl {
public:
    struct Item final {
        std::string text;
        std::shared_ptr<const render::DuiImage> icon;
    };

    std::vector<Item> items;
    ui::DuiTextInput* input{};
    ui::IPopupHost* popup{};
    std::shared_ptr<int> inputLifetime;
    std::shared_ptr<int> popupLifetime;
    std::shared_ptr<const render::DuiImage> leadingIcon;
    std::function<void(int)> changed;
    std::string text;
    render::DuiTextStyle style;
    core::Color arrowColor{80, 100, 140, 255};
    core::Size leadingIconSize{16, 16};
    int leadingIconGap{8};
    int selected{-1};
    int maxVisible{8};
    int itemHeight{22};
    bool editable{};
    bool incremental{};
    bool substring{};
    bool suppress{};
    bool popupOpen{};
    bool arrowColorOverride{};
    bool backgroundVisible{true};
};
namespace { bool Contains(std::string_view value, std::string_view query, bool substring) { if (query.empty()) return true; std::string lowerValue(value), lowerQuery(query); for (auto& c : lowerValue) c = static_cast<char>(std::towlower(c)); for (auto& c : lowerQuery) c = static_cast<char>(std::towlower(c)); if (substring) return lowerValue.find(lowerQuery) != std::string::npos; return lowerValue.size() >= lowerQuery.size() && lowerValue.compare(0, lowerQuery.size(), lowerQuery) == 0; } }
DuiComboBox::DuiComboBox() : comboBox_(std::make_unique<Impl>())
{
    SetPointerCursor(core::DuiPointerCursor::Hand);
}
DuiComboBox::~DuiComboBox()
{
    comboBox_->inputLifetime.reset();
    comboBox_->input = nullptr;
    comboBox_->popupLifetime.reset();
    comboBox_->popup = nullptr;
}
int DuiComboBox::AddItem(std::string text) { comboBox_->items.push_back({std::move(text), {}}); return Count() - 1; }
void DuiComboBox::RemoveItem(int index) { if (index < 0 || index >= Count()) return; comboBox_->items.erase(comboBox_->items.begin() + index); if (comboBox_->selected == index) comboBox_->selected = -1; else if (comboBox_->selected > index) --comboBox_->selected; }
void DuiComboBox::ClearItems() { ClosePopup(); comboBox_->items.clear(); comboBox_->selected = -1; comboBox_->text.clear(); }
int DuiComboBox::Count() const { return static_cast<int>(comboBox_->items.size()); }
std::string DuiComboBox::TextAt(int index) const { return index >= 0 && index < Count() ? comboBox_->items[index].text : std::string{}; }
void DuiComboBox::SetItemIcon(int index, std::shared_ptr<const render::DuiImage> icon)
{
    if (index >= 0 && index < Count())
        comboBox_->items[index].icon = std::move(icon);
}

const std::shared_ptr<const render::DuiImage>& DuiComboBox::ItemIconAt(int index) const
{
    static const std::shared_ptr<const render::DuiImage> empty;
    return index >= 0 && index < Count() ? comboBox_->items[index].icon : empty;
}
void DuiComboBox::SetSelectedIndex(int index, bool notify) { if (index < -1 || index >= Count() || comboBox_->selected == index) return; comboBox_->selected = index; if (index >= 0) SetText(comboBox_->items[index].text, false); if (notify && comboBox_->changed) comboBox_->changed(index); }
int DuiComboBox::SelectedIndex() const { return comboBox_->selected; }
void DuiComboBox::SetEditable(bool editable) { comboBox_->editable = editable; Layout(Bounds()); }
bool DuiComboBox::Editable() const { return comboBox_->editable; }
void DuiComboBox::SetTextInput(ui::DuiTextInput* input) { if (comboBox_->input == input) return; comboBox_->inputLifetime.reset(); if (comboBox_->input) comboBox_->input->SetChangedHandler({}); comboBox_->input = input; if (!input) return; input->SetBorderVisible(false); comboBox_->inputLifetime = std::make_shared<int>(); const std::weak_ptr<int> lifetime = comboBox_->inputLifetime; input->SetChangedHandler([lifetime, state = comboBox_.get()] { if (!lifetime.lock() || state->suppress) return; state->text = state->input->Text(); state->selected = -1; }); input->SetText(comboBox_->text); Layout(Bounds()); }
void DuiComboBox::SetPopupHost(ui::IPopupHost* popup) { if (comboBox_->popup == popup) return; comboBox_->popupLifetime.reset(); if (comboBox_->popup) { ClosePopup(); comboBox_->popup->SetDismissedHandler({}); } comboBox_->popup = popup; if (popup) { comboBox_->popupLifetime = std::make_shared<int>(); const std::weak_ptr<int> lifetime = comboBox_->popupLifetime; popup->SetDismissedHandler([lifetime, state = comboBox_.get()] { if (lifetime.lock()) state->popupOpen = false; }); } }
void DuiComboBox::SetText(std::string text, bool notify)
{
    comboBox_->text = std::move(text);
    if (comboBox_->input && comboBox_->input->Text() != comboBox_->text)
    {
        comboBox_->suppress = true;
        comboBox_->input->SetText(comboBox_->text);
        comboBox_->suppress = false;
    }
    const auto found = std::find_if(comboBox_->items.begin(), comboBox_->items.end(), [this](const Impl::Item& item)
    {
        return item.text == comboBox_->text;
    });
    comboBox_->selected = found == comboBox_->items.end() ? -1 : static_cast<int>(found - comboBox_->items.begin());
    if (notify && comboBox_->changed)
        comboBox_->changed(comboBox_->selected);
}
std::string DuiComboBox::Text() const { return comboBox_->input && Editable() ? comboBox_->input->Text() : comboBox_->text; }
void DuiComboBox::SetIncrementalSearch(bool enabled) { comboBox_->incremental = enabled; }
void DuiComboBox::SetSubstringSearch(bool enabled) { comboBox_->substring = enabled; }
std::vector<int> DuiComboBox::FilteredIndices(std::string_view query) const
{
    std::vector<int> result;
    for (int index = 0; index < Count(); ++index)
    {
        if (Contains(comboBox_->items[index].text, query, comboBox_->substring))
            result.push_back(index);
    }
    return result;
}
void DuiComboBox::SetMaxVisibleItems(int count) { comboBox_->maxVisible = (std::max)(1, count); }
void DuiComboBox::SetItemHeight(int pixels) { comboBox_->itemHeight = (std::max)(8, pixels); }
void DuiComboBox::SetArrowColor(core::Color color) { comboBox_->arrowColor = color; comboBox_->arrowColorOverride = true; }
core::Color DuiComboBox::ArrowColor() const { return comboBox_->arrowColorOverride ? comboBox_->arrowColor : Theme().Get(core::ThemeSlot::ComboArrow); }
void DuiComboBox::SetBackgroundVisible(bool visible) { comboBox_->backgroundVisible = visible; }
bool DuiComboBox::BackgroundVisible() const { return comboBox_->backgroundVisible; }
void DuiComboBox::SetLeadingIcon(std::shared_ptr<const render::DuiImage> icon) { comboBox_->leadingIcon = std::move(icon); }
const std::shared_ptr<const render::DuiImage>& DuiComboBox::LeadingIcon() const { return comboBox_->leadingIcon; }
void DuiComboBox::SetLeadingIconSize(core::Size size) { comboBox_->leadingIconSize = {(std::max)(0, size.width), (std::max)(0, size.height)}; }
core::Size DuiComboBox::LeadingIconSize() const { return comboBox_->leadingIconSize; }
void DuiComboBox::SetLeadingIconGap(int pixels) { comboBox_->leadingIconGap = (std::max)(0, pixels); }
int DuiComboBox::LeadingIconGap() const { return comboBox_->leadingIconGap; }
void DuiComboBox::SetSelectionChangedHandler(std::function<void(int)> handler) { comboBox_->changed = std::move(handler); }
void DuiComboBox::OpenPopup()
{
    if (!comboBox_->popup || comboBox_->popupOpen || Count() == 0)
        return;
    const auto indices = comboBox_->incremental ? FilteredIndices(Text()) : FilteredIndices({});
    if (indices.empty())
        return;
    auto list = std::make_unique<list::DuiListBox>();
    auto* raw = list.get();
    raw->SetTheme(&Theme());
    raw->SetRowHeight(comboBox_->itemHeight);
    raw->SetLeadingIcon(comboBox_->leadingIcon);
    raw->SetLeadingIconSize(comboBox_->leadingIconSize);
    raw->SetLeadingIconGap(comboBox_->leadingIconGap);
    for (int index : indices)
    {
        const int row = raw->AddItem(TextAt(index), static_cast<std::uintptr_t>(index));
        raw->SetIconAt(row, ItemIconAt(index));
    }
    const auto selected = std::find(indices.begin(), indices.end(), comboBox_->selected);
    raw->SetSelectedIndex(selected == indices.end() ? -1 : static_cast<int>(selected - indices.begin()), false);
    Impl* state = comboBox_.get();
    const std::weak_ptr<int> lifetime = comboBox_->popupLifetime;
    raw->SetSelectionChangedHandler([lifetime, state, indices](int row)
    {
        if (!lifetime.lock())
            return;
        if (row >= 0 && row < static_cast<int>(indices.size()))
        {
            const int index = indices[row];
            state->selected = index;
            state->text = state->items[index].text;
            if (state->input)
            {
                state->suppress = true;
                state->input->SetText(state->text);
                state->suppress = false;
            }
            if (state->changed)
                state->changed(index);
        }
        if (state->popup)
            state->popup->RequestHide();
        state->popupOpen = false;
    });
    ui::DuiPopupOptions options;
    options.anchor = Bounds();
    options.size = {Bounds().Width(),
                    (std::min)(static_cast<int>(indices.size()), comboBox_->maxVisible) * comboBox_->itemHeight};
    comboBox_->popupOpen = comboBox_->popup->Show(options, std::move(list), [raw](core::Rect bounds)
    {
        raw->Layout(bounds);
    }, [raw](render::Canvas& canvas, core::Rect dirty)
    {
        raw->Paint(canvas, dirty);
        render::PaintPopupBorder(canvas, raw->Bounds(), raw->Theme());
    });
}
void DuiComboBox::ClosePopup() { if (comboBox_->popup && comboBox_->popupOpen) comboBox_->popup->RequestHide(); comboBox_->popupOpen = false; }
bool DuiComboBox::PopupOpen() const { return comboBox_->popupOpen; }
core::Rect DuiComboBox::ArrowRect() const { const auto bounds = Bounds(); return {bounds.right - 20, bounds.top, bounds.right, bounds.bottom}; }
void DuiComboBox::Layout(core::Rect bounds) { SetBounds(bounds); if (comboBox_->input) { comboBox_->input->SetBounds({bounds.left + 1, bounds.top + 1, ArrowRect().left - 1, bounds.bottom - 1}); comboBox_->input->SetVisible(Editable() && EffectivelyVisible()); comboBox_->input->SetEnabled(Enabled()); } }
bool DuiComboBox::OnEvent(const core::Event& event) { if (!Enabled()) return false; if (event.type == core::EventType::PointerDown && Bounds().Contains(event.position)) { if (!Editable() || ArrowRect().Contains(event.position)) { if (PopupOpen()) ClosePopup(); else OpenPopup(); return true; } if (comboBox_->input) comboBox_->input->FocusAt(event.position); return true; } if (event.type == core::EventType::PointerWheel && !PopupOpen() && Count() > 0) { const int next = std::clamp(SelectedIndex() + (event.wheelDelta > 0 ? -1 : 1), 0, Count() - 1); SetSelectedIndex(next); return true; } if (event.type == core::EventType::KeyDown && (event.key == core::key::Up || event.key == core::key::Down) && Count() > 0) { SetSelectedIndex(std::clamp(SelectedIndex() + (event.key == core::key::Up ? -1 : 1), 0, Count() - 1)); return true; } return false; }
void DuiComboBox::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = Bounds();
    const core::Rect clipped = core::Rect::Intersect(bounds, dirty);
    if (!EffectivelyVisible() || clipped.Empty()) return;
    if (comboBox_->backgroundVisible)
        canvas.FillRect(clipped, Theme().Get(core::ThemeSlot::SurfaceBackground));
    canvas.StrokeRoundedRect(bounds, 0, Theme().Get(core::ThemeSlot::FieldBorder), 1.0F);
    const auto arrow = ArrowRect();
    render::DuiPath path;
    const int x = (arrow.left + arrow.right) / 2;
    const int y = (arrow.top + arrow.bottom) / 2;
    path.MoveTo({x - 4, y - 2});
    path.LineTo({x + 4, y - 2});
    path.LineTo({x, y + 3});
    path.Close();
    canvas.FillPath(path, ArrowColor());
    int textLeft = bounds.left + 8;
    const auto& selectedIcon = comboBox_->selected >= 0 && comboBox_->selected < Count()
        && comboBox_->items[comboBox_->selected].icon ? comboBox_->items[comboBox_->selected].icon : comboBox_->leadingIcon;
    if (!Editable() && selectedIcon && !selectedIcon->Empty()
        && comboBox_->leadingIconSize.width > 0 && comboBox_->leadingIconSize.height > 0)
    {
        const core::Size iconSize = comboBox_->leadingIconSize;
        const int iconTop = bounds.top + (bounds.Height() - iconSize.height) / 2;
        const core::Rect iconBounds{textLeft, iconTop, textLeft + iconSize.width, iconTop + iconSize.height};
        const core::Size sourceSize = selectedIcon->Size();
        canvas.DrawImage(*selectedIcon, {0, 0, sourceSize.width, sourceSize.height}, iconBounds);
        textLeft = iconBounds.right + comboBox_->leadingIconGap;
    }
    auto style = comboBox_->style;
    style.color = Enabled() ? Theme().Get(core::ThemeSlot::ControlText)
                            : Theme().Get(core::ThemeSlot::FieldDisabledText);
    ysDui::controls::detail::PaintTextInput(
        canvas, Editable() ? comboBox_->input : nullptr, Text(), {},
        {textLeft, bounds.top, arrow.left - 4, bounds.bottom}, style, style.color,
        render::DuiTextAlignment::Start, false, Editable() && Focused() && Enabled(),
        Theme().Get(core::ThemeSlot::TextSelectionBackground),
        Theme().Get(core::ThemeSlot::TextSelectionText));
}
} // namespace ysDui::controls::input
