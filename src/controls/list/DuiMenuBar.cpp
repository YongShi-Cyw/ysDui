#include "ysDui/controls/list/DuiMenuBar.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiMnemonic.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/controls/list/DuiMenu.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"

namespace ysDui::controls::list {
namespace {
constexpr int DefaultHeight = 24;
constexpr int MinimumHeight = 12;
constexpr int HorizontalPadding = 8;
constexpr int MinimumItemWidth = 28;
}

class DuiMenuBar::Impl {
public:
    struct Item final {
        DuiMenuBarItem value;
        DuiMenu* dropdown{};
        core::Rect bounds;
        core::DuiSubscription invokedSubscription;
        core::DuiSubscription closedSubscription;
    };

    ~Impl() { Clear(); }

    void Clear()
    {
        if (activeIndex >= 0 && activeIndex < static_cast<int>(items.size()) && items[activeIndex].dropdown)
            items[activeIndex].dropdown->Hide();
        for (auto& item : items)
            Detach(item);
        items.clear();
        activeIndex = -1;
        hoveredIndex = -1;
    }

    void Detach(Item& item)
    {
        item.invokedSubscription.Reset();
        item.closedSubscription.Reset();
    }

    void Attach(int index)
    {
        Item& item = items[index];
        if (!item.dropdown)
            return;
        item.invokedSubscription = item.dropdown->SubscribeItemInvokedScoped([state = this, index](std::uint32_t id) {
            if (state->commandHandler)
                state->commandHandler(id);
            state->Close(index);
        });
        item.closedSubscription = item.dropdown->SubscribeClosedScoped([state = this, index] {
            state->Close(index);
        });
    }

    [[nodiscard]] int ItemWidth(const Item& item) const
    {
        const std::string text = core::mnemonic::StripPrefix(item.value.text);
        const int textWidth = measurer ? measurer->MeasureText(text, textStyle, {}).size.width
                                       : static_cast<int>(text.size()) * textStyle.pointSize;
        return (std::max)(MinimumItemWidth, textWidth + HorizontalPadding * 2);
    }

    void SetActive(int index, bool open)
    {
        if (activeIndex == index)
            return;
        activeIndex = index;
        if (dropdownChanged)
            dropdownChanged(index, open);
    }

    void Close(int index)
    {
        if (activeIndex != index)
            return;
        activeIndex = -1;
        if (dropdownChanged)
            dropdownChanged(index, false);
    }

    int HitTest(core::Point point) const
    {
        for (int index = 0; index < static_cast<int>(items.size()); ++index) {
            if (items[index].bounds.Contains(point))
                return index;
        }
        return -1;
    }

    std::vector<Item> items;
    ui::IUiHostFactory* factory{};
    ui::HostRef owner;
    render::DuiTextMeasurer* measurer{};
    render::DuiTextStyle textStyle{{0, 0, 0, 255}, {}, 9, false};
    std::function<void(std::uint32_t)> commandHandler;
    std::function<void(int, bool)> dropdownChanged;
    int activeIndex{-1};
    int hoveredIndex{-1};
    int itemHeight{DefaultHeight};
};

DuiMenuBar::DuiMenuBar() : menuBar_(std::make_unique<Impl>()) {}
DuiMenuBar::~DuiMenuBar() = default;
DuiMenuBar::DuiMenuBar(DuiMenuBar&&) noexcept = default;
DuiMenuBar& DuiMenuBar::operator=(DuiMenuBar&&) noexcept = default;

int DuiMenuBar::AddItem(std::uint32_t id, std::string text, DuiMenu* dropdown)
{
    menuBar_->items.push_back({{id, std::move(text), true}, dropdown, {}, {}, {}});
    const int index = static_cast<int>(menuBar_->items.size()) - 1;
    menuBar_->Attach(index);
    Layout(Bounds());
    return index;
}

void DuiMenuBar::RemoveItem(int index)
{
    if (index < 0 || index >= ItemCount())
        return;
    if (menuBar_->activeIndex == index && menuBar_->items[index].dropdown)
        menuBar_->items[index].dropdown->Hide();
    menuBar_->Detach(menuBar_->items[index]);
    menuBar_->items.erase(menuBar_->items.begin() + index);
    if (menuBar_->activeIndex == index)
        menuBar_->activeIndex = -1;
    if (menuBar_->hoveredIndex == index)
        menuBar_->hoveredIndex = -1;
    Layout(Bounds());
}

void DuiMenuBar::ClearItems() { menuBar_->Clear(); }
int DuiMenuBar::ItemCount() const { return static_cast<int>(menuBar_->items.size()); }
DuiMenuBarItem DuiMenuBar::ItemAt(int index) const
{
    return index >= 0 && index < ItemCount() ? menuBar_->items[index].value : DuiMenuBarItem{};
}

void DuiMenuBar::SetItemEnabled(std::uint32_t id, bool enabled)
{
    for (auto& item : menuBar_->items)
        if (item.value.id == id) item.value.enabled = enabled;
}

bool DuiMenuBar::ItemEnabled(std::uint32_t id) const
{
    for (const auto& item : menuBar_->items)
        if (item.value.id == id) return item.value.enabled;
    return false;
}

void DuiMenuBar::SetDropdown(int index, DuiMenu* dropdown)
{
    if (index < 0 || index >= ItemCount())
        return;
    if (menuBar_->activeIndex == index && menuBar_->items[index].dropdown)
        menuBar_->items[index].dropdown->Hide();
    menuBar_->Detach(menuBar_->items[index]);
    menuBar_->items[index].dropdown = dropdown;
    menuBar_->Attach(index);
}

DuiMenu* DuiMenuBar::Dropdown(int index) const
{
    return index >= 0 && index < ItemCount() ? menuBar_->items[index].dropdown : nullptr;
}

void DuiMenuBar::SetItemHeight(int pixels)
{
    menuBar_->itemHeight = (std::max)(MinimumHeight, pixels);
    Layout(Bounds());
}

int DuiMenuBar::ItemHeight() const { return menuBar_->itemHeight; }

void DuiMenuBar::SetPopupContext(ui::IUiHostFactory& factory, ui::HostRef owner)
{
    menuBar_->factory = &factory;
    menuBar_->owner = owner;
}

void DuiMenuBar::SetTextMeasurer(render::DuiTextMeasurer* measurer) { menuBar_->measurer = measurer; Layout(Bounds()); }
void DuiMenuBar::SetTextStyle(render::DuiTextStyle style) { menuBar_->textStyle = std::move(style); Layout(Bounds()); }
void DuiMenuBar::SetCommandHandler(std::function<void(std::uint32_t)> handler) { menuBar_->commandHandler = std::move(handler); }
void DuiMenuBar::SetDropdownChangedHandler(std::function<void(int, bool)> handler) { menuBar_->dropdownChanged = std::move(handler); }
int DuiMenuBar::ActiveIndex() const { return menuBar_->activeIndex; }
int DuiMenuBar::HoveredIndex() const { return menuBar_->hoveredIndex; }
core::Rect DuiMenuBar::ItemRect(int index) const { return index >= 0 && index < ItemCount() ? menuBar_->items[index].bounds : core::Rect{}; }
core::Size DuiMenuBar::DesiredSize() const
{
    int width{};
    for (const auto& item : menuBar_->items)
        width += menuBar_->ItemWidth(item);
    return {width, menuBar_->itemHeight};
}

void DuiMenuBar::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    int left = bounds.left;
    for (auto& item : menuBar_->items) {
        const int width = menuBar_->ItemWidth(item);
        item.bounds = {left, bounds.top, left + width, bounds.bottom};
        left += width;
    }
}

bool DuiMenuBar::ProcessMnemonic(char character)
{
    const char lower = character >= 'A' && character <= 'Z'
                               ? static_cast<char>(character - 'A' + 'a')
                               : character;
    for (int index = 0; index < ItemCount(); ++index) {
        const auto& item = menuBar_->items[index];
        if (item.value.enabled && core::mnemonic::FindCharacter(item.value.text) == lower) {
            if (menuBar_->activeIndex >= 0 && menuBar_->activeIndex != index) {
                DuiMenu* active = menuBar_->items[menuBar_->activeIndex].dropdown;
                if (active)
                    active->Hide();
            }
            if (menuBar_->factory && item.dropdown && item.dropdown->Show(*menuBar_->factory, menuBar_->owner, item.bounds)) {
                menuBar_->SetActive(index, true);
                return true;
            }
        }
    }
    return false;
}

bool DuiMenuBar::OnEvent(const core::Event& event)
{
    if (!Enabled())
        return false;
    if (event.type == core::EventType::PointerMove) {
        const int index = menuBar_->HitTest(event.position);
        menuBar_->hoveredIndex = index;
        if (index >= 0 && index != menuBar_->activeIndex && menuBar_->activeIndex >= 0)
            ProcessMnemonic(core::mnemonic::FindCharacter(menuBar_->items[index].value.text));
        return index >= 0;
    }
    if (event.type == core::EventType::PointerDown) {
        const int index = menuBar_->HitTest(event.position);
        return index >= 0 && ProcessMnemonic(core::mnemonic::FindCharacter(menuBar_->items[index].value.text));
    }
    if (event.type == core::EventType::KeyDown && (event.modifiers & core::modifier::Alt) != 0)
        return ProcessMnemonic(static_cast<char>(event.key));
    return false;
}

void DuiMenuBar::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect visible = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || visible.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    for (int index = 0; index < ItemCount(); ++index) {
        const auto& item = menuBar_->items[index];
        if (core::Rect::Intersect(item.bounds, dirty).Empty())
            continue;
        if (index == menuBar_->activeIndex)
            canvas.FillRect(item.bounds, theme.Get(core::ThemeSlot::MenuBarActive));
        else if (index == menuBar_->hoveredIndex && item.value.enabled)
            canvas.FillRect(item.bounds, theme.Get(core::ThemeSlot::MenuBarHover));
        render::DuiTextStyle style = menuBar_->textStyle;
        style.color = item.value.enabled ? theme.Get(core::ThemeSlot::MenuBarText)
                                         : theme.Get(core::ThemeSlot::MenuBarDisabledText);
        canvas.DrawText(core::mnemonic::StripPrefix(item.value.text), item.bounds, style,
                        render::DuiTextAlignment::Center, false);
    }
}

} // namespace ysDui::controls::list
