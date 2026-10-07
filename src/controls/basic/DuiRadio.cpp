/**
 * 文件名：DuiRadio.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：独立单选的互斥、交互与绘制。
 */
#include "ysDui/controls/basic/DuiRadio.hpp"

#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {

class DuiRadio::Impl {
public:
    std::string text;
    std::function<void()> clickHandler;
    DuiButtonVariant variant{DuiButtonVariant::Default};
    int radioGroup{};
    bool checked{};
    render::DuiTextStyle textStyle{{255, 255, 255, 255}, {}, 9, true};
};

DuiRadio::DuiRadio() : radio_(std::make_unique<Impl>()) {}
DuiRadio::~DuiRadio() = default;
DuiRadio::DuiRadio(DuiRadio&&) noexcept = default;
DuiRadio& DuiRadio::operator=(DuiRadio&&) noexcept = default;

void DuiRadio::SetText(std::string text) { radio_->text = std::move(text); }
const std::string& DuiRadio::Text() const { return radio_->text; }
void DuiRadio::SetChecked(bool checked)
{
    radio_->checked = checked;
    if (!checked || Parent() == nullptr)
        return;
    for (const auto& child : Parent()->Children())
    {
        auto* sibling = dynamic_cast<DuiRadio*>(child.get());
        if (sibling != nullptr && sibling != this && sibling->RadioGroup() == radio_->radioGroup)
            sibling->SetChecked(false);
    }
}
bool DuiRadio::Checked() const { return radio_->checked; }
void DuiRadio::SetRadioGroup(int group) { radio_->radioGroup = group < 0 ? 0 : group; }
int DuiRadio::RadioGroup() const { return radio_->radioGroup; }
void DuiRadio::SetVariant(DuiButtonVariant variant) { radio_->variant = variant; }
DuiButtonVariant DuiRadio::Variant() const { return radio_->variant; }
void DuiRadio::SetTextStyle(render::DuiTextStyle style) { radio_->textStyle = std::move(style); }
const render::DuiTextStyle& DuiRadio::TextStyle() const { return radio_->textStyle; }
void DuiRadio::SetClickHandler(std::function<void()> handler) { radio_->clickHandler = std::move(handler); }

core::DuiAccessibilityData DuiRadio::CreateAccessibilityData() const
{
    return {core::DuiAccessibilityRole::RadioButton, radio_->text,
            radio_->checked ? "true" : "false", {}, true, {}};
}

void DuiRadio::Activate()
{
    SetChecked(true);
    if (radio_->clickHandler)
        radio_->clickHandler();
}

bool DuiRadio::OnEvent(const core::Event& event)
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

void DuiRadio::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::Rect clipped = core::Rect::Intersect(Bounds(), dirty);
    if (clipped.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    const core::Color primary = theme.Get(core::ThemeSlot::BrandPrimary);
    const core::Color control = theme.Get(core::ThemeSlot::ControlBackground);
    const core::Color controlBorder = theme.Get(core::ThemeSlot::ControlBorder);
    const core::Color defaultText = theme.Get(core::ThemeSlot::ButtonText);
    const bool disabled = !Enabled();
    const bool transparentChoice = radio_->variant == DuiButtonVariant::Ghost ||
        radio_->variant == DuiButtonVariant::Outlined || radio_->variant == DuiButtonVariant::Text;
    core::Color fill = control;
    core::Color border = controlBorder;
    core::Color textColor = disabled ? theme.Get(core::ThemeSlot::ButtonChoiceDisabledText) : defaultText;
    bool outlined = true;
    if (transparentChoice)
    {
        fill = {255, 255, 255, 0};
        border = radio_->variant == DuiButtonVariant::Outlined
            ? primary : core::Color{0, 0, 0, 0};
        textColor = disabled ? theme.Get(core::ThemeSlot::ButtonChoiceDisabledText)
            : radio_->variant == DuiButtonVariant::Ghost
                ? theme.Get(core::ThemeSlot::ButtonChoiceGhostText)
            : primary;
        outlined = radio_->variant == DuiButtonVariant::Outlined;
    }
    constexpr int kChromeRadius = 6;
    if (fill.alpha != 0)
        canvas.FillRoundedRect(clipped, kChromeRadius, fill);
    if (outlined)
        canvas.StrokeRoundedRect(clipped, kChromeRadius, border, 1.0f);

    constexpr int kGlyphSide = 16;
    const int left = clipped.left + 8;
    const int top = clipped.top + (clipped.Height() - kGlyphSide) / 2;
    const core::Rect glyph{left, top, left + kGlyphSide, top + kGlyphSide};
    if (!transparentChoice)
        canvas.FillEllipse(glyph, theme.Get(core::ThemeSlot::SurfaceBackground));
    canvas.StrokeRoundedRect(glyph, kGlyphSide / 2, theme.Get(core::ThemeSlot::ButtonChoiceBorder), 1.0f);
    if (Checked())
        canvas.FillEllipse({left + 5, top + 5, left + 11, top + 11}, primary);

    if (radio_->text.empty())
        return;
    core::Rect textBounds = clipped;
    textBounds.left = glyph.right + 8;
    render::DuiTextStyle style = radio_->textStyle;
    style.color = textColor;
    canvas.DrawText(radio_->text, textBounds, style, render::DuiTextAlignment::Start, false);
}

} // namespace ysDui::controls::basic
