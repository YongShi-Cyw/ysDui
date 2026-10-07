#include "ysDui/controls/list/DuiMenu.hpp"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiMnemonic.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiSvg.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"

namespace ysDui::controls::list {
namespace {
constexpr int MinimumMenuWidth = 96;
constexpr int MenuHorizontalPadding = 12;
constexpr int MenuSeparatorHorizontalInset = 0; // 分割线距菜单左右边缘（逻辑像素）
constexpr int MenuVerticalPadding = 4;
constexpr int MenuRowHeight = 32;
constexpr int SeparatorHeight = 12;
constexpr int SubmenuColumnWidth = 16;
/**
 * 前导列：勾选标记与菜单项图标共用同一列，均在列内水平居中
 * 注意：行的左边界 row.left 已包含菜单水平内边距（见 ItemRect），
 *       此处不得再加内边距，否则标记/图标会被整体推右。
 */
constexpr int MenuLeadingColumnWidth = 28; // 列宽：容纳 24px 图标（两侧各余 2px）
constexpr int MenuLeadingTextGap = 6;      // 标记/图标与文字的间距
constexpr int MenuIconSize = 24;  // 菜单项图标边长（逻辑像素）
constexpr int MaximumMenuWidth = 480;
constexpr int CheckIconSize = 12; // 勾选图标绘制边长（逻辑像素）
constexpr int MenuRowHighlightInset = 4; // 悬停高亮相对菜单左右内边距

// 菜单勾选标记（对齐业务侧对勾 SVG）。
constexpr std::string_view MenuCheckSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <polygon fill="#000" points="1.8 5.52 1.8 11.04 6.04 15.02 14.2 6.31 14.2 .98 6.05 9.49 1.8 5.52"/>
</svg>)SVG";

char ToLower(char value)
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

/**
 * 将勾选 SVG 光栅化为指定颜色的预乘 BGRA 图标。
 * @param color 目标着色（按 alpha 预乘）
 */
std::shared_ptr<const render::DuiImage> CreateCheckIcon(core::Color color)
{
    auto buffer = render::RasterizeSvg(MenuCheckSvg, {CheckIconSize, CheckIconSize});
    if (!buffer)
        return {};
    std::vector<unsigned char> pixels(buffer->Bytes().begin(), buffer->Bytes().end());
    for (std::size_t offset{}; offset < pixels.size(); offset += 4)
    {
        const unsigned char alpha = pixels[offset + 3];
        pixels[offset] = static_cast<unsigned char>((static_cast<unsigned int>(color.blue) * alpha + 127U) / 255U);
        pixels[offset + 1] = static_cast<unsigned char>((static_cast<unsigned int>(color.green) * alpha + 127U) / 255U);
        pixels[offset + 2] = static_cast<unsigned char>((static_cast<unsigned int>(color.red) * alpha + 127U) / 255U);
    }
    return render::DuiImage::CreateBgra8Premultiplied(buffer->Size(), std::move(pixels));
}
}

class DuiMenu::Impl {
public:
    struct SubscriptionStore final {
        struct Subscription final {
            std::size_t id{};
            std::function<void(std::uint32_t)> invoked;
            std::function<void()> closed;
        };

        std::vector<Subscription> subscriptions;
        std::size_t nextSubscription{1};
    };

    class Content final : public core::Control, public render::DuiRenderable {
    public:
        explicit Content(DuiMenu::Impl& owner) : owner_(owner) {}

        bool OnEvent(const core::Event& event) override
        {
            if (event.type == core::EventType::PointerMove) {
                owner_.hoveredIndex = owner_.IndexAt(event.position);
                owner_.OpenSubmenu(owner_.hoveredIndex);
                return owner_.hoveredIndex >= 0;
            }
            if (event.type == core::EventType::PointerDown) {
                if (event.button != core::PointerButton::Primary)
                    return false;
                const int index = owner_.IndexAt(event.position);
                if (index < 0)
                    return false;
                owner_.Invoke(index);
                return true;
            }
            if (event.type == core::EventType::KeyDown) {
                if (event.key == core::key::Escape) {
                    owner_.Hide();
                    return true;
                }
                if (event.key == core::key::Up || event.key == core::key::Down) {
                    owner_.MoveSelection(event.key == core::key::Down ? 1 : -1);
                    return true;
                }
                if ((event.key == core::key::Enter || event.key == core::key::Space) && owner_.selectedIndex >= 0) {
                    owner_.Invoke(owner_.selectedIndex);
                    return true;
                }
                return false;
            }
            if (event.type == core::EventType::TextInput && event.text.size() == 1) {
                const char key = ToLower(event.text.front());
                for (int index = 0; index < static_cast<int>(owner_.items.size()); ++index) {
                    const auto& item = owner_.items[index];
                    if (item.enabled && item.kind != DuiMenuItemKind::Separator &&
                        core::mnemonic::FindCharacter(item.text) == key) {
                        owner_.Invoke(index);
                        return true;
                    }
                }
            }
            return false;
        }

        void Paint(render::Canvas& canvas, core::Rect dirty) const override
        {
            const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
            if (bounds.Empty())
                return;
            const core::DuiTheme& theme = Theme();
            canvas.FillRect(bounds, theme.Get(core::ThemeSlot::MenuBackground));
            canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::MenuBorder), 1.0F);
            for (int index = 0; index < static_cast<int>(owner_.items.size()); ++index) {
                const DuiMenuItem& item = owner_.items[index];
                const core::Rect row = owner_.ItemRect(index);
                if (core::Rect::Intersect(row, dirty).Empty())
                    continue;
                if (item.kind == DuiMenuItemKind::Separator) {
                    const int middle = (row.top + row.bottom) / 2;
                    canvas.FillRect({row.left + MenuSeparatorHorizontalInset, middle,
                                     row.right - MenuSeparatorHorizontalInset, middle + 1},
                                    theme.Get(core::ThemeSlot::MenuSeparator));
                    continue;
                }
                if (index == owner_.hoveredIndex || index == owner_.selectedIndex)
                {
                    const core::Rect highlight{Bounds().left + MenuRowHighlightInset, row.top,
                                               Bounds().right - MenuRowHighlightInset, row.bottom};
                    canvas.FillRect(highlight, theme.Get(core::ThemeSlot::MenuHighlight));
                }
                render::DuiTextStyle style = owner_.textStyle;
                style.color = item.enabled ? theme.Get(core::ThemeSlot::MenuText)
                                           : theme.Get(core::ThemeSlot::MenuDisabledText);
                // 勾选标记与菜单项图标共用前导列，二者都在该列内水平居中，
                // 文字起点一致，保证「有勾选」与「有图标」的菜单对齐（参考 NX 原生菜单）
                const int leadingLeft = row.left;
                const int leadingWidth = owner_.LeadingColumnWidth();
                if (item.kind == DuiMenuItemKind::Checkable && item.checked)
                {
                    const auto& icon = owner_.CheckIcon(style.color);
                    if (icon)
                    {
                        const int left = leadingLeft + (leadingWidth - CheckIconSize) / 2;
                        const int top = row.top + (row.Height() - CheckIconSize) / 2;
                        canvas.DrawImage(*icon, {left, top, left + CheckIconSize, top + CheckIconSize});
                    }
                }
                if (item.image)
                {
                    const int left = leadingLeft + (leadingWidth - MenuIconSize) / 2;
                    const int top = row.top + (row.Height() - MenuIconSize) / 2;
                    canvas.DrawImage(*item.image,
                                     {left, top, left + MenuIconSize, top + MenuIconSize});
                }
                canvas.DrawText(core::mnemonic::StripPrefix(item.text),
                                {leadingLeft + leadingWidth + owner_.LeadingTextGap(),
                                 row.top,
                                 row.right - MenuHorizontalPadding - SubmenuColumnWidth, row.bottom},
                                style, render::DuiTextAlignment::Start, false);
                if (item.kind == DuiMenuItemKind::Submenu)
                    canvas.DrawText(">", {row.right - SubmenuColumnWidth - 4, row.top, row.right - 4, row.bottom},
                                    style, render::DuiTextAlignment::Center, false);
            }
        }

    private:
        DuiMenu::Impl& owner_;
    };

    /**
     * 按文字色缓存勾选图标，主题色变化时重建。
     * @param color 当前菜单项文字色
     */
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& CheckIcon(core::Color color) const
    {
        if (!checkIcon_ || checkIconColor_ != color)
        {
            checkIcon_ = CreateCheckIcon(color);
            checkIconColor_ = color;
        }
        return checkIcon_;
    }

    std::vector<DuiMenuItem> items;
    std::unique_ptr<ui::IPopupHost> popup;
    ui::IUiHostFactory* factory{};
    ui::HostRef owner;
    render::DuiTextMeasurer* measurer{};
    render::DuiTextStyle textStyle{{32, 32, 36, 255}, {}, 9, false};
    std::function<void(std::uint32_t)> invoked;
    std::shared_ptr<SubscriptionStore> subscriptionStore{std::make_shared<SubscriptionStore>()};
    core::Rect contentBounds;
    int hoveredIndex{-1};
    int selectedIndex{-1};
    DuiMenu* activeSubmenu{};
    Impl* parent{};
    mutable std::shared_ptr<const render::DuiImage> checkIcon_; // 勾选 SVG 着色缓存
    mutable core::Color checkIconColor_{};                     // 缓存对应的着色
    bool visible{};

    [[nodiscard]] int RowHeight(const DuiMenuItem& item) const
    {
        return item.kind == DuiMenuItemKind::Separator ? SeparatorHeight : MenuRowHeight;
    }

    [[nodiscard]] int EstimateTextWidth(const std::string& text) const
    {
        // 无 Measurer 时按 UTF-8 码点估算。
        // 与 DuiWin32TextRenderer 一致：fontSizeDip = pointSize * 96/72。
        const int pointSize = (std::max)(1, textStyle.pointSize);
        const int em = (std::max)(1, (pointSize * 96 + 36) / 72);
        const int asciiAdvance = (std::max)(1, (em * 6 + 5) / 10);
        const int wideAdvance = em + 2; // 中文约 1em，加兜底避免回退字体偏宽
        int width = 0;
        for (std::size_t i = 0; i < text.size();)
        {
            const unsigned char lead = static_cast<unsigned char>(text[i]);
            int step = 1;
            if ((lead & 0x80U) == 0)
            {
                width += asciiAdvance;
                step = 1;
            }
            else if ((lead & 0xE0U) == 0xC0U)
            {
                width += wideAdvance;
                step = 2;
            }
            else if ((lead & 0xF0U) == 0xE0U)
            {
                width += wideAdvance;
                step = 3;
            }
            else if ((lead & 0xF8U) == 0xF0U)
            {
                width += wideAdvance;
                step = 4;
            }
            i += (std::min)(step, static_cast<int>(text.size() - i));
        }
        return width;
    }

    /** @return 是否存在需要前导列的菜单项（勾选标记或图标） */
    [[nodiscard]] bool NeedsLeadingColumn() const
    {
        for (const auto& item : items)
        {
            if (item.kind == DuiMenuItemKind::Checkable || item.image)
                return true;
        }
        return false;
    }

    /** @return 前导列宽（无需前导列时为 0，避免留白） */
    [[nodiscard]] int LeadingColumnWidth() const
    {
        return NeedsLeadingColumn() ? MenuLeadingColumnWidth : 0;
    }

    /** @return 标记/图标与文字的间距（无需前导列时为 0） */
    [[nodiscard]] int LeadingTextGap() const
    {
        return NeedsLeadingColumn() ? MenuLeadingTextGap : 0;
    }

    [[nodiscard]] core::Size DesiredSize() const
    {
        // 与 Content::Paint 布局一致：
        // 行左内边距（ItemRect 已含）+ 前导列（勾选/图标共用）+ 标记与文字间距
        // + 文字 + 文字与子菜单间距 + 子菜单列 + 行右内边距
        const int kRowChromeWidth = LeadingColumnWidth() + LeadingTextGap()
                                    + SubmenuColumnWidth + MenuHorizontalPadding * 3;
        constexpr int kSafetyPad = 16; // 防字体回退 / DPI 舍入裁切

        int width = MinimumMenuWidth;
        int height = MenuVerticalPadding * 2;
        for (const auto& item : items)
        {
            height += RowHeight(item);
            if (item.kind == DuiMenuItemKind::Separator)
                continue;
            const std::string text = core::mnemonic::StripPrefix(item.text);
            const int textWidth = measurer ? measurer->MeasureText(text, textStyle, {}).size.width
                                           : EstimateTextWidth(text);
            width = (std::max)(width, textWidth + kRowChromeWidth + kSafetyPad);
        }
        return {std::clamp(width, MinimumMenuWidth, MaximumMenuWidth), height};
    }

    [[nodiscard]] core::Rect ItemRect(int index) const
    {
        if (index < 0 || index >= static_cast<int>(items.size()))
            return {};
        int top = contentBounds.top + MenuVerticalPadding;
        for (int itemIndex = 0; itemIndex < index; ++itemIndex)
            top += RowHeight(items[itemIndex]);
        return {contentBounds.left + MenuHorizontalPadding, top, contentBounds.right - MenuHorizontalPadding,
                top + RowHeight(items[index])};
    }

    [[nodiscard]] int IndexAt(core::Point point) const
    {
        for (int index = 0; index < static_cast<int>(items.size()); ++index) {
            if (items[index].kind != DuiMenuItemKind::Separator && ItemRect(index).Contains(point))
                return index;
        }
        return -1;
    }

    void MoveSelection(int direction)
    {
        if (items.empty())
            return;
        int index = selectedIndex;
        for (int count = 0; count < static_cast<int>(items.size()); ++count) {
            index = (index + direction + static_cast<int>(items.size())) % static_cast<int>(items.size());
            if (items[index].enabled && items[index].kind != DuiMenuItemKind::Separator) {
                selectedIndex = index;
                return;
            }
        }
    }

    void Invoke(int index)
    {
        if (index < 0 || index >= static_cast<int>(items.size()))
            return;
        DuiMenuItem& item = items[index];
        if (!item.enabled || item.kind == DuiMenuItemKind::Separator)
            return;
        if (item.kind == DuiMenuItemKind::Submenu) {
            OpenSubmenu(index);
            return;
        }
        if (item.kind == DuiMenuItemKind::Checkable)
            item.checked = !item.checked;
        const std::uint32_t id = item.id;
        Hide();
        if (parent)
            parent->Hide();
        if (invoked)
            invoked(id);
        const auto callbacks = subscriptionStore->subscriptions;
        for (const auto& subscription : callbacks)
            if (subscription.invoked) subscription.invoked(id);
    }

    void Hide()
    {
        if (!visible)
            return;
        visible = false;
        hoveredIndex = -1;
        selectedIndex = -1;
        if (activeSubmenu)
            activeSubmenu->Hide();
        activeSubmenu = nullptr;
        if (popup) {
            popup->SetDismissedHandler({});
            popup->RequestHide();
        }
        const auto callbacks = subscriptionStore->subscriptions;
        for (const auto& subscription : callbacks)
            if (subscription.closed) subscription.closed();
    }

    void Dismiss()
    {
        if (!visible)
            return;
        visible = false;
        hoveredIndex = -1;
        selectedIndex = -1;
        activeSubmenu = nullptr;
        const auto callbacks = subscriptionStore->subscriptions;
        for (const auto& subscription : callbacks)
            if (subscription.closed) subscription.closed();
        if (parent)
            parent->Hide();
    }

    void OpenSubmenu(int index)
    {
        if (index < 0 || index >= static_cast<int>(items.size()) || items[index].kind != DuiMenuItemKind::Submenu
            || !items[index].enabled || items[index].submenu == nullptr) {
            if (activeSubmenu) {
                activeSubmenu->Hide();
                activeSubmenu = nullptr;
            }
            return;
        }
        DuiMenu* submenu = items[index].submenu;
        if (activeSubmenu == submenu && submenu->Visible())
            return;
        if (activeSubmenu)
            activeSubmenu->Hide();
        if (!factory || !popup)
            return;
        const core::Rect row = ItemRect(index);
        const core::Rect anchor{row.right - 1, row.top, row.right, row.top};
        submenu->menu_->parent = this;
        const ui::HostRef popupReference = popup->Reference();
        if (submenu->Show(*factory, popupReference.Valid() ? popupReference : owner, anchor))
            activeSubmenu = submenu;
    }
};

DuiMenu::DuiMenu() : menu_(std::make_unique<Impl>()) {}
DuiMenu::~DuiMenu() { Hide(); }
DuiMenu::DuiMenu(DuiMenu&&) noexcept = default;
DuiMenu& DuiMenu::operator=(DuiMenu&&) noexcept = default;

void DuiMenu::AddItem(std::uint32_t id, std::string text)
{
    // 默认构造再赋值：避免 GCC/Clang -Wmissing-field-initializers（指定初始化漏字段仍会告警）。
    DuiMenuItem item{};
    item.id = id;
    item.kind = DuiMenuItemKind::Command;
    item.text = std::move(text);
    menu_->items.push_back(std::move(item));
}

void DuiMenu::AddItem(std::uint32_t id, std::string text,
                      std::shared_ptr<const render::DuiImage> image)
{
    DuiMenuItem item{};
    item.id = id;
    item.kind = DuiMenuItemKind::Command;
    item.text = std::move(text);
    item.image = std::move(image);
    menu_->items.push_back(std::move(item));
}

void DuiMenu::AddCheckItem(std::uint32_t id, std::string text, bool checked)
{
    DuiMenuItem item{};
    item.id = id;
    item.kind = DuiMenuItemKind::Checkable;
    item.text = std::move(text);
    item.enabled = true;
    item.checked = checked;
    menu_->items.push_back(std::move(item));
}

void DuiMenu::AddSubmenu(std::uint32_t id, std::string text, DuiMenu* submenu)
{
    DuiMenuItem item{};
    item.id = id;
    item.kind = DuiMenuItemKind::Submenu;
    item.text = std::move(text);
    item.enabled = submenu != nullptr;
    item.checked = false;
    item.submenu = submenu;
    menu_->items.push_back(std::move(item));
}

void DuiMenu::AddSeparator()
{
    DuiMenuItem item{};
    item.id = 0;
    item.kind = DuiMenuItemKind::Separator;
    item.enabled = false;
    menu_->items.push_back(std::move(item));
}
void DuiMenu::ClearItems() { Hide(); menu_->items.clear(); }
int DuiMenu::ItemCount() const { return static_cast<int>(menu_->items.size()); }
DuiMenuItem DuiMenu::ItemAt(int index) const { return index >= 0 && index < ItemCount() ? menu_->items[index] : DuiMenuItem{}; }

void DuiMenu::SetItemEnabled(std::uint32_t id, bool enabled)
{
    for (auto& item : menu_->items)
        if (item.id == id) item.enabled = enabled;
}

void DuiMenu::SetItemChecked(std::uint32_t id, bool checked)
{
    for (auto& item : menu_->items)
        if (item.id == id && item.kind == DuiMenuItemKind::Checkable) item.checked = checked;
}

bool DuiMenu::ItemChecked(std::uint32_t id) const
{
    for (const auto& item : menu_->items)
        if (item.id == id) return item.checked;
    return false;
}

void DuiMenu::SetTextMeasurer(render::DuiTextMeasurer* measurer) { menu_->measurer = measurer; }
void DuiMenu::SetTextStyle(render::DuiTextStyle style) { menu_->textStyle = std::move(style); }
void DuiMenu::SetItemInvokedHandler(std::function<void(std::uint32_t)> handler) { menu_->invoked = std::move(handler); }
core::DuiSubscription DuiMenu::SubscribeItemInvokedScoped(std::function<void(std::uint32_t)> handler)
{
    if (!handler)
        return {};
    const auto store = menu_->subscriptionStore;
    const std::size_t id = store->nextSubscription++;
    core::DuiSubscription subscription([weakStore = std::weak_ptr<Impl::SubscriptionStore>{store}, id]
    {
        if (const auto lockedStore = weakStore.lock())
        {
            std::erase_if(lockedStore->subscriptions,
                          [id](const Impl::SubscriptionStore::Subscription& value) { return value.id == id; });
        }
    });
    store->subscriptions.push_back({id, std::move(handler), {}});
    return subscription;
}

core::DuiSubscription DuiMenu::SubscribeClosedScoped(std::function<void()> handler)
{
    if (!handler)
        return {};
    const auto store = menu_->subscriptionStore;
    const std::size_t id = store->nextSubscription++;
    core::DuiSubscription subscription([weakStore = std::weak_ptr<Impl::SubscriptionStore>{store}, id]
    {
        if (const auto lockedStore = weakStore.lock())
        {
            std::erase_if(lockedStore->subscriptions,
                          [id](const Impl::SubscriptionStore::Subscription& value) { return value.id == id; });
        }
    });
    store->subscriptions.push_back({id, {}, std::move(handler)});
    return subscription;
}

std::size_t DuiMenu::SubscribeItemInvoked(std::function<void(std::uint32_t)> handler)
{
    const std::size_t id = menu_->subscriptionStore->nextSubscription++;
    menu_->subscriptionStore->subscriptions.push_back({id, std::move(handler), {}});
    return id;
}

std::size_t DuiMenu::SubscribeClosed(std::function<void()> handler)
{
    const std::size_t id = menu_->subscriptionStore->nextSubscription++;
    menu_->subscriptionStore->subscriptions.push_back({id, {}, std::move(handler)});
    return id;
}

void DuiMenu::Unsubscribe(std::size_t subscription)
{
    std::erase_if(menu_->subscriptionStore->subscriptions,
                  [subscription](const Impl::SubscriptionStore::Subscription& value)
                  {
                      return value.id == subscription;
                  });
}

bool DuiMenu::Show(ui::IUiHostFactory& factory, ui::HostRef owner, core::Rect anchor,
                   ui::DuiPopupPlacement placement)
{
    if (menu_->items.empty())
        return false;
    Hide();
    menu_->factory = &factory;
    menu_->owner = owner;
    menu_->popup = factory.CreatePopupHost(owner);
    if (!menu_->popup)
        return false;
    const core::Size size = menu_->DesiredSize();
    auto content = std::make_unique<Impl::Content>(*menu_);
    Impl::Content* rawContent = content.get();
    menu_->popup->SetDismissedHandler([state = menu_.get()] { state->Dismiss(); });
    if (!menu_->popup->Show({anchor, size, placement, true,
                             ui::DuiPopupModality::Modeless, ui::DuiPopupSizeMode::Specified,
                             false, false, std::nullopt}, std::move(content),
                            [state = menu_.get(), rawContent](core::Rect bounds) {
                                state->contentBounds = bounds;
                                rawContent->SetBounds(bounds);
                            },
                            [rawContent](render::Canvas& canvas, core::Rect dirty) { rawContent->Paint(canvas, dirty); })) {
        menu_->popup.reset();
        return false;
    }
    menu_->visible = true;
    return true;
}

void DuiMenu::Hide() { if (menu_) menu_->Hide(); }
bool DuiMenu::Visible() const { return menu_ && menu_->visible; }

} // namespace ysDui::controls::list
