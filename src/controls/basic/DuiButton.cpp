#include "ysDui/controls/basic/DuiButton.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiFocusVisual.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiPath.hpp"

namespace ysDui::controls::basic {
namespace {
/** 下拉窄条宽度：容纳箭头与分隔线。 */
constexpr int DropDownStripWidth = 22;
/** 箭头半宽与半高（逻辑像素）。 */
constexpr int ChevronHalfWidth = 4;
constexpr int ChevronHalfHeight = 3;
} // namespace

class DuiButton::Impl {
public:
    std::string text;
    std::function<void()> clickHandler;
    std::function<void()> dropDownHandler;
    DuiButtonKind kind{DuiButtonKind::Push};
    DuiButtonVariant variant{DuiButtonVariant::Primary};
    bool pressedDropDown{}; // 本次按下是否落在下拉窄条内
    DuiButtonSkin skin;
    std::shared_ptr<const render::DuiImage> leadingIcon;
    core::Size leadingIconSize{16, 16};
    int leadingIconGap{8};
    render::DuiTextStyle textStyle{{255, 255, 255, 255}, {}, 9, true};
};

DuiButton::DuiButton() : button_(std::make_unique<Impl>()) {}
DuiButton::~DuiButton() = default;
DuiButton::DuiButton(DuiButton&&) noexcept = default;
DuiButton& DuiButton::operator=(DuiButton&&) noexcept = default;
void DuiButton::SetText(std::string text) { button_->text = std::move(text); }
const std::string& DuiButton::Text() const { return button_->text; }
void DuiButton::SetKind(DuiButtonKind kind) { button_->kind = kind; }
DuiButtonKind DuiButton::Kind() const { return button_->kind; }
void DuiButton::SetVariant(DuiButtonVariant variant) { button_->variant = variant; }
DuiButtonVariant DuiButton::Variant() const { return button_->variant; }
void DuiButton::SetSkin(DuiButtonSkin skin) { button_->skin = std::move(skin); }
const DuiButtonSkin& DuiButton::Skin() const { return button_->skin; }
void DuiButton::SetLeadingIcon(std::shared_ptr<const render::DuiImage> icon) { button_->leadingIcon = std::move(icon); }
const std::shared_ptr<const render::DuiImage>& DuiButton::LeadingIcon() const { return button_->leadingIcon; }
void DuiButton::SetLeadingIconSize(core::Size size)
{
    button_->leadingIconSize = {(std::max)(0, size.width), (std::max)(0, size.height)};
}
core::Size DuiButton::LeadingIconSize() const { return button_->leadingIconSize; }
void DuiButton::SetLeadingIconGap(int pixels) { button_->leadingIconGap = (std::max)(0, pixels); }
int DuiButton::LeadingIconGap() const { return button_->leadingIconGap; }
void DuiButton::SetTextStyle(render::DuiTextStyle style) { button_->textStyle = std::move(style); }
const render::DuiTextStyle& DuiButton::TextStyle() const { return button_->textStyle; }
void DuiButton::SetClickHandler(std::function<void()> handler) { button_->clickHandler = std::move(handler); }
void DuiButton::SetDropDownHandler(std::function<void()> handler) { button_->dropDownHandler = std::move(handler); }
core::Rect DuiButton::DropDownRect() const
{
    if (button_->kind != DuiButtonKind::DropDown && button_->kind != DuiButtonKind::Split)
        return {};
    const core::Rect bounds = Bounds();
    const int width = (std::min)(DropDownStripWidth, bounds.Width());
    return {bounds.right - width, bounds.top, bounds.right, bounds.bottom};
}
core::Rect DuiButton::MainRect() const
{
    const core::Rect bounds = Bounds();
    const core::Rect strip = DropDownRect();
    return strip.Empty() ? bounds : core::Rect{bounds.left, bounds.top, strip.left, bounds.bottom};
}
core::DuiAccessibilityData DuiButton::CreateAccessibilityData() const
{
    core::DuiAccessibilityData data{core::DuiAccessibilityRole::Button, button_->text,
                                    {}, {}, true, {}};
    data.patterns = core::DuiAccessibilityPattern::Invoke;
    return data;
}

void DuiButton::Activate()
{
    if (button_->kind == DuiButtonKind::DropDown)
    {
        // 整颗按钮即下拉触发器；未单独设置下拉回调时回退到点击回调
        const auto& handler = button_->dropDownHandler ? button_->dropDownHandler : button_->clickHandler;
        if (handler)
            handler();
        return;
    }
    if (button_->kind == DuiButtonKind::Split)
    {
        // 分裂按钮：按下区域决定触发哪一个回调
        const auto& handler = button_->pressedDropDown ? button_->dropDownHandler : button_->clickHandler;
        if (handler)
            handler();
        return;
    }
    if (button_->clickHandler)
        button_->clickHandler();
}

bool DuiButton::PerformAccessibilityAction(core::DuiAccessibilityAction action, std::string_view)
{
    // 分裂按钮经无障碍触发时按主操作处理（窄条是纯鼠标细分区域）
    if (!Enabled() || action != core::DuiAccessibilityAction::Invoke)
    {
        return false;
    }
    button_->pressedDropDown = false;
    Activate();
    return true;
}

bool DuiButton::OnEvent(const core::Event& event) {
    if (!Enabled()) return false;
    if (event.type == core::EventType::KeyDown &&
        (event.key == core::key::Enter || event.key == core::key::Space)) {
        Activate();
        return true;
    }
    const bool contains = Bounds().Contains(event.position);
    if (event.type == core::EventType::PointerMove) {
        SetHovered(contains);
        return Captured();
    }
    if (event.type == core::EventType::PointerDown && contains) {
        SetHovered(true);
        SetCaptured(true);
        // 记录按下落在哪个区域，供分裂按钮在抬起时选择回调
        button_->pressedDropDown = !DropDownRect().Empty() && DropDownRect().Contains(event.position);
        return true;
    }
    if (event.type == core::EventType::PointerCancel && Captured()) {
        SetCaptured(false);
        SetHovered(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && Captured()) {
        SetCaptured(false);
        SetHovered(contains);
        if (contains) Activate();
        return true;
    }
    return false;
}

void DuiButton::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible()) return;
    const core::Rect clipped = core::Rect::Intersect(Bounds(), dirty);
    if (clipped.Empty()) return;
    const core::DuiTheme& theme = Theme();
    const core::Color primary = theme.Get(core::ThemeSlot::BrandPrimary);
    const core::Color primaryText = theme.Get(core::ThemeSlot::TextOnPrimary);
    const core::Color control = theme.Get(core::ThemeSlot::ControlBackground);
    const core::Color controlBorder = theme.Get(core::ThemeSlot::ControlBorder);
    const core::Color defaultText = theme.Get(core::ThemeSlot::ButtonText);
    const bool disabled = !Enabled();
    core::Color fill = primary;
    core::Color border = primary;
    core::Color textColor = primaryText;
    bool outlined{};
    switch (button_->variant) {
    case DuiButtonVariant::Primary: break;
    case DuiButtonVariant::Default:
        fill = control; border = controlBorder; textColor = defaultText; outlined = true; break;
    case DuiButtonVariant::Danger:
        fill = theme.Get(core::ThemeSlot::Danger); border = fill; break;
    case DuiButtonVariant::Ghost:
    case DuiButtonVariant::Text:
        fill = {0, 0, 0, 0}; border = {0, 0, 0, 0}; textColor = primary; break;
    case DuiButtonVariant::Outlined:
        fill = theme.Get(core::ThemeSlot::SurfaceBackground); border = primary; textColor = primary; outlined = true; break;
    }
    if (button_->kind == DuiButtonKind::Icon && button_->variant == DuiButtonVariant::Primary) {
        fill = control; border = controlBorder; textColor = defaultText; outlined = true;
    }
    if (Hovered() && !disabled) {
        switch (button_->variant) {
        case DuiButtonVariant::Primary: fill = theme.Get(core::ThemeSlot::ButtonPrimaryHover); break;
        case DuiButtonVariant::Default: fill = theme.Get(core::ThemeSlot::ControlHover); break;
        case DuiButtonVariant::Danger: fill = theme.Get(core::ThemeSlot::DangerHover); border = fill; break;
        case DuiButtonVariant::Ghost:
        case DuiButtonVariant::Text: fill = theme.Get(core::ThemeSlot::ButtonGhostHover); break;
        case DuiButtonVariant::Outlined: fill = theme.Get(core::ThemeSlot::ButtonOutlinedHover); break;
        }
    }
    if (Captured() && !disabled) {
        switch (button_->variant) {
        case DuiButtonVariant::Primary: fill = theme.Get(core::ThemeSlot::ButtonPrimaryPressed); break;
        case DuiButtonVariant::Default: fill = theme.Get(core::ThemeSlot::ControlPressed); break;
        case DuiButtonVariant::Danger: fill = theme.Get(core::ThemeSlot::DangerPressed); border = fill; break;
        case DuiButtonVariant::Ghost:
        case DuiButtonVariant::Text: fill = theme.Get(core::ThemeSlot::ButtonGhostPressed); break;
        case DuiButtonVariant::Outlined: fill = theme.Get(core::ThemeSlot::ButtonOutlinedPressed); break;
        }
    }
    if (disabled) {
        fill = theme.Get(core::ThemeSlot::ControlDisabled); border = fill;
        textColor = theme.Get(core::ThemeSlot::ButtonDisabledText); outlined = false;
    }
    const int radius = button_->kind == DuiButtonKind::Push ? 8 : 6;
    const auto validImage = [](const std::shared_ptr<const render::DuiImage>& image)
    {
        return image != nullptr && !image->Empty();
    };
    const render::DuiImage* skinImage{};
    if (button_->kind == DuiButtonKind::Push && validImage(button_->skin.normal))
    {
        const auto& candidate = disabled ? button_->skin.disabled
            : Captured() ? button_->skin.pressed
            : Hovered() ? button_->skin.hovered
            : button_->skin.normal;
        skinImage = validImage(candidate) ? candidate.get() : button_->skin.normal.get();
    }
    if (skinImage != nullptr)
        render::DrawNinePatch(canvas, *skinImage, clipped, button_->skin.insets);
    else
    {
        if (fill.alpha != 0)
            canvas.FillRoundedRect(clipped, radius, fill);
        if (outlined)
            canvas.StrokeRoundedRect(clipped, radius, border, 1.0f);
    }
    core::Rect textBounds = clipped;
    if (button_->leadingIcon && !button_->leadingIcon->Empty()) {
        const core::Size iconSize = button_->leadingIconSize;
        const int iconLeft = clipped.left + 10;
        const int iconTop = clipped.top + (clipped.Height() - iconSize.height) / 2;
        const core::Rect iconBounds{iconLeft, iconTop, iconLeft + iconSize.width, iconTop + iconSize.height};
        const core::Size sourceSize = button_->leadingIcon->Size();
        canvas.DrawImage(*button_->leadingIcon, {0, 0, sourceSize.width, sourceSize.height}, iconBounds);
        textBounds.left = iconBounds.right + button_->leadingIconGap;
    }
    // 下拉 / 分裂：为右侧窄条让出空间，并在窄条内绘制箭头
    const core::Rect strip = DropDownRect();
    const bool hasStrip = !strip.Empty();
    if (hasStrip && button_->kind == DuiButtonKind::Split)
    {
        // 分隔线标示两个可点区域；绘制在窄条左边界
        const core::Color divider = border.alpha != 0 ? border : theme.Get(core::ThemeSlot::ControlBorder);
        canvas.FillRect({strip.left, clipped.top + 5, strip.left + 1, clipped.bottom - 5}, divider);
    }
    if (hasStrip)
        textBounds.right = (std::max)(textBounds.left, strip.left - 4);
    if (!button_->text.empty()) {
        render::DuiTextStyle style = button_->textStyle;
        style.color = textColor;
        canvas.DrawText(button_->text, textBounds, style, render::DuiTextAlignment::Center, false);
    }
    if (hasStrip)
    {
        const int centerX = (strip.left + strip.right) / 2;
        const int centerY = (strip.top + strip.bottom) / 2;
        render::DuiPath chevron;
        chevron.MoveTo({centerX - ChevronHalfWidth, centerY - ChevronHalfHeight});
        chevron.LineTo({centerX + ChevronHalfWidth, centerY - ChevronHalfHeight});
        chevron.LineTo({centerX, centerY + ChevronHalfHeight});
        chevron.Close();
        canvas.FillPath(chevron, textColor);
    }
    if (Focused())
        render::DrawThemedFocusRing(canvas, clipped, theme);
}

} // namespace ysDui::controls::basic
