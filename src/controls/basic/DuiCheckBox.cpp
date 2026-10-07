/**
 * 文件名：DuiCheckBox.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：独立复选框的交互与绘制，视觉与原 DuiButton CheckBox kind 对齐。
 */
#include "ysDui/controls/basic/DuiCheckBox.hpp"

#include <utility>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiPath.hpp"

namespace ysDui::controls::basic {

class DuiCheckBox::Impl {
public:
    std::string text;
    std::function<void()> clickHandler;
    DuiButtonVariant variant{DuiButtonVariant::Default};
    bool checked{};
    render::DuiTextStyle textStyle{{255, 255, 255, 255}, {}, 9, true};
};

DuiCheckBox::DuiCheckBox() : checkBox_(std::make_unique<Impl>()) {}
DuiCheckBox::~DuiCheckBox() = default;
DuiCheckBox::DuiCheckBox(DuiCheckBox&&) noexcept = default;
DuiCheckBox& DuiCheckBox::operator=(DuiCheckBox&&) noexcept = default;

void DuiCheckBox::SetText(std::string text) { checkBox_->text = std::move(text); }
const std::string& DuiCheckBox::Text() const { return checkBox_->text; }
void DuiCheckBox::SetChecked(bool checked) { checkBox_->checked = checked; }
bool DuiCheckBox::Checked() const { return checkBox_->checked; }
void DuiCheckBox::SetVariant(DuiButtonVariant variant) { checkBox_->variant = variant; }
DuiButtonVariant DuiCheckBox::Variant() const { return checkBox_->variant; }
void DuiCheckBox::SetTextStyle(render::DuiTextStyle style) { checkBox_->textStyle = std::move(style); }
const render::DuiTextStyle& DuiCheckBox::TextStyle() const { return checkBox_->textStyle; }
void DuiCheckBox::SetClickHandler(std::function<void()> handler)
{
    checkBox_->clickHandler = std::move(handler);
}

core::DuiAccessibilityData DuiCheckBox::CreateAccessibilityData() const
{
    return {core::DuiAccessibilityRole::CheckBox, checkBox_->text,
            checkBox_->checked ? "true" : "false", {}, true, {}};
}

void DuiCheckBox::Activate()
{
    SetChecked(!Checked());
    if (checkBox_->clickHandler)
        checkBox_->clickHandler();
}

bool DuiCheckBox::OnEvent(const core::Event& event)
{
    if (!Enabled())
        return false;
    if (event.type == core::EventType::KeyDown &&
        (event.key == core::key::Enter || event.key == core::key::Space))
    {
        Activate();
        return true;
    }
    const bool contains = Bounds().Contains(event.position);
    if (event.type == core::EventType::PointerMove)
    {
        SetHovered(contains);
        return Captured();
    }
    if (event.type == core::EventType::PointerDown && contains)
    {
        SetHovered(true);
        SetCaptured(true);
        return true;
    }
    if (event.type == core::EventType::PointerCancel && Captured())
    {
        SetCaptured(false);
        SetHovered(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && Captured())
    {
        SetCaptured(false);
        SetHovered(contains);
        if (contains)
            Activate();
        return true;
    }
    return false;
}

void DuiCheckBox::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::Rect clipped = core::Rect::Intersect(Bounds(), dirty);
    if (clipped.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    const core::Color primary = theme.Get(core::ThemeSlot::BrandPrimary);
    const core::Color primaryText = theme.Get(core::ThemeSlot::TextOnPrimary);
    const core::Color control = theme.Get(core::ThemeSlot::ControlBackground);
    const core::Color controlBorder = theme.Get(core::ThemeSlot::ControlBorder);
    const core::Color defaultText = theme.Get(core::ThemeSlot::ButtonText);
    const bool disabled = !Enabled();
    const bool transparentChoice = checkBox_->variant == DuiButtonVariant::Ghost ||
        checkBox_->variant == DuiButtonVariant::Outlined || checkBox_->variant == DuiButtonVariant::Text;
    core::Color fill = control;
    core::Color border = controlBorder;
    core::Color textColor = disabled ? theme.Get(core::ThemeSlot::ButtonChoiceDisabledText) : defaultText;
    bool outlined = true;
    if (transparentChoice)
    {
        fill = {255, 255, 255, 0};
        border = checkBox_->variant == DuiButtonVariant::Outlined
            ? primary : core::Color{0, 0, 0, 0};
        textColor = disabled ? theme.Get(core::ThemeSlot::ButtonChoiceDisabledText)
            : checkBox_->variant == DuiButtonVariant::Ghost
                ? theme.Get(core::ThemeSlot::ButtonChoiceGhostText)
            : primary;
        outlined = checkBox_->variant == DuiButtonVariant::Outlined;
    }
    constexpr int kChromeRadius = 6; // 与原 Button-CheckBox 圆角一致
    if (fill.alpha != 0)
        canvas.FillRoundedRect(clipped, kChromeRadius, fill);
    if (outlined)
        canvas.StrokeRoundedRect(clipped, kChromeRadius, border, 1.0f);

    constexpr int kGlyphSide = 16; // 勾选方框边长
    const int left = clipped.left + 8;
    const int top = clipped.top + (clipped.Height() - kGlyphSide) / 2;
    const core::Rect glyph{left, top, left + kGlyphSide, top + kGlyphSide};
    const core::Color glyphFill = Checked() ? primary
        : transparentChoice ? core::Color{0, 0, 0, 0}
                            : theme.Get(core::ThemeSlot::SurfaceBackground);
    if (glyphFill.alpha != 0)
        canvas.FillRoundedRect(glyph, 0, glyphFill);
    canvas.StrokeRoundedRect(glyph, 0, theme.Get(core::ThemeSlot::ButtonChoiceBorder), 1.0f);
    if (Checked())
    {
        render::DuiPath check;
        check.MoveTo({left + 4, top + 8});
        check.LineTo({left + 7, top + 11});
        check.LineTo({left + 12, top + 5});
        canvas.StrokePath(check, primaryText, 2.0F);
    }

    if (checkBox_->text.empty())
        return;
    core::Rect textBounds = clipped;
    textBounds.left = glyph.right + 8;
    render::DuiTextStyle style = checkBox_->textStyle;
    style.color = textColor;
    canvas.DrawText(checkBox_->text, textBounds, style, render::DuiTextAlignment::Start, false);
}

} // namespace ysDui::controls::basic
