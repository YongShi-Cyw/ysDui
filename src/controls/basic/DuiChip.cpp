/**
 * 文件名：DuiChip.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：标签控件的交互与绘制。
 */
#include "ysDui/controls/basic/DuiChip.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiPath.hpp"

namespace ysDui::controls::basic {
namespace {
/** 图标与文字、文字与关闭按钮之间的间距。 */
constexpr int kInnerGap = 6;
/** 关闭按钮的点击区边长。 */
constexpr int kCloseHitSize = 14;
/** 关闭叉的半径。 */
constexpr int kCloseGlyphRadius = 3;

/**
 * 估算单行文本尺寸。
 * 说明：不依赖平台文本测量器；ASCII 按字号的 55% 估宽，非 ASCII 按一个字号计宽，
 * 与 Markdown 回退测量保持一致，用于首选尺寸这类"够用即可"的场合。
 */
core::Size EstimateTextSize(std::string_view text, const render::DuiTextStyle& style)
{
    const int em = (std::max)(8, style.pointSize);
    int width{};
    for (std::size_t index{}; index < text.size();)
    {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        const std::size_t units = first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
        index += (std::min)(units, text.size() - index);
        width += units == 1 ? (std::max)(4, em * 55 / 100) : em;
    }
    return {width, (std::max)(10, style.pointSize + 6)};
}
} // namespace

class DuiChip::Impl {
public:
    std::string text;
    DuiChipVariant variant{DuiChipVariant::Filled};
    core::Color fill{};
    core::Color textColor{};
    core::Color border{};
    bool fillOverride{};
    bool textColorOverride{};
    bool borderOverride{};
    render::DuiTextStyle textStyle{{20, 20, 20, 255}, {}, 9, false};
    int cornerRadius{-1};
    int horizontalPadding{10};
    int verticalPadding{4};
    std::shared_ptr<const render::DuiImage> leadingIcon;
    int iconSize{14};
    bool closable{};
    bool closeHovered{};
    bool bodyPressed{};
    std::function<void()> clicked;
    std::function<void()> closed;
};

DuiChip::DuiChip() : chip_(std::make_unique<Impl>()) {}
DuiChip::~DuiChip() = default;
DuiChip::DuiChip(DuiChip&&) noexcept = default;
DuiChip& DuiChip::operator=(DuiChip&&) noexcept = default;

void DuiChip::SetText(std::string text) { chip_->text = std::move(text); }
const std::string& DuiChip::Text() const { return chip_->text; }
void DuiChip::SetVariant(DuiChipVariant variant) { chip_->variant = variant; }
DuiChipVariant DuiChip::Variant() const { return chip_->variant; }

void DuiChip::SetFillColor(core::Color color) { chip_->fill = color; chip_->fillOverride = true; }
core::Color DuiChip::FillColor() const
{
    if (chip_->fillOverride)
        return chip_->fill;
    if (chip_->variant == DuiChipVariant::Outlined)
        return {255, 255, 255, 0};
    return Theme().Get(core::ThemeSlot::ChipFill);
}
void DuiChip::SetTextColor(core::Color color) { chip_->textColor = color; chip_->textColorOverride = true; }
core::Color DuiChip::TextColor() const
{
    if (chip_->textColorOverride)
        return chip_->textColor;
    return chip_->variant == DuiChipVariant::Outlined ? Theme().Get(core::ThemeSlot::BrandPrimary)
                                                     : Theme().Get(core::ThemeSlot::ChipText);
}
void DuiChip::SetBorderColor(core::Color color) { chip_->border = color; chip_->borderOverride = true; }
core::Color DuiChip::BorderColor() const
{
    if (chip_->borderOverride)
        return chip_->border;
    return chip_->variant == DuiChipVariant::Outlined ? Theme().Get(core::ThemeSlot::BrandPrimary)
                                                     : Theme().Get(core::ThemeSlot::ChipBorder);
}
void DuiChip::SetTextStyle(render::DuiTextStyle style) { chip_->textStyle = std::move(style); }
const render::DuiTextStyle& DuiChip::TextStyle() const { return chip_->textStyle; }
void DuiChip::SetCornerRadius(int pixels) { chip_->cornerRadius = pixels; }
int DuiChip::CornerRadius() const { return chip_->cornerRadius; }
void DuiChip::SetPadding(int horizontal, int vertical)
{
    chip_->horizontalPadding = (std::max)(0, horizontal);
    chip_->verticalPadding = (std::max)(0, vertical);
}
int DuiChip::HorizontalPadding() const { return chip_->horizontalPadding; }
int DuiChip::VerticalPadding() const { return chip_->verticalPadding; }
void DuiChip::SetLeadingIcon(std::shared_ptr<const render::DuiImage> icon) { chip_->leadingIcon = std::move(icon); }
const std::shared_ptr<const render::DuiImage>& DuiChip::LeadingIcon() const { return chip_->leadingIcon; }
void DuiChip::SetIconSize(int pixels) { chip_->iconSize = (std::max)(0, pixels); }
int DuiChip::IconSize() const { return chip_->iconSize; }
void DuiChip::SetClosable(bool closable) { chip_->closable = closable; }
bool DuiChip::Closable() const { return chip_->closable; }
void DuiChip::SetClickHandler(std::function<void()> handler) { chip_->clicked = std::move(handler); }
void DuiChip::SetCloseHandler(std::function<void()> handler) { chip_->closed = std::move(handler); }

core::Rect DuiChip::CloseRect() const
{
    if (!chip_->closable)
        return {};
    const core::Rect bounds = Bounds();
    if (bounds.Empty())
        return {};
    const int right = bounds.right - chip_->horizontalPadding;
    const int centerY = (bounds.top + bounds.bottom) / 2;
    return {right - kCloseHitSize, centerY - kCloseHitSize / 2, right, centerY + kCloseHitSize / 2};
}

core::Rect DuiChip::ContentRect() const
{
    const core::Rect bounds = Bounds();
    if (bounds.Empty())
        return {};
    int left = bounds.left + chip_->horizontalPadding;
    const bool hasIcon = chip_->leadingIcon != nullptr && !chip_->leadingIcon->Empty() && chip_->iconSize > 0;
    if (hasIcon)
        left += chip_->iconSize + kInnerGap;
    const core::Rect close = CloseRect();
    const int right = close.Empty() ? bounds.right - chip_->horizontalPadding : close.left - kInnerGap;
    return {left, bounds.top, (std::max)(left, right), bounds.bottom};
}

core::Size DuiChip::DesiredSize() const
{
    const core::Size text = EstimateTextSize(chip_->text, chip_->textStyle);
    int width = chip_->horizontalPadding * 2 + text.width;
    if (chip_->leadingIcon != nullptr && !chip_->leadingIcon->Empty() && chip_->iconSize > 0)
        width += chip_->iconSize + kInnerGap;
    if (chip_->closable)
        width += kCloseHitSize + kInnerGap;
    return {width, text.height + chip_->verticalPadding * 2};
}

bool DuiChip::OnEvent(const core::Event& event)
{
    if (!Enabled())
        return false;
    const core::Rect bounds = Bounds();
    const bool contains = bounds.Contains(event.position);
    const core::Rect close = CloseRect();
    const bool overClose = !close.Empty() && close.Contains(event.position);
    switch (event.type)
    {
    case core::EventType::PointerMove:
        // 只有落在关闭按钮上才需要重绘；标签体本身没有悬停反馈
        chip_->closeHovered = overClose;
        SetHovered(contains);
        return Captured();
    case core::EventType::PointerLeave:
        chip_->closeHovered = false;
        SetHovered(false);
        return false;
    case core::EventType::PointerDown:
        if (!contains)
            return false;
        chip_->bodyPressed = true;
        SetCaptured(true);
        return true;
    case core::EventType::PointerUp:
        if (!chip_->bodyPressed)
            return false;
        chip_->bodyPressed = false;
        SetCaptured(false);
        if (!contains)
            return true;
        // 关闭按钮优先：命中时不再触发标签体点击
        if (overClose)
        {
            if (chip_->closed)
                chip_->closed();
        }
        else if (chip_->clicked)
        {
            chip_->clicked();
        }
        return true;
    case core::EventType::PointerCancel:
        if (!chip_->bodyPressed)
            return false;
        chip_->bodyPressed = false;
        SetCaptured(false);
        return true;
    default:
        return false;
    }
}

core::DuiAccessibilityData DuiChip::CreateAccessibilityData() const
{
    // 可关闭的标签按按钮暴露，便于无障碍用户执行关闭
    core::DuiAccessibilityData data{core::DuiAccessibilityRole::Button, chip_->text, {}, {}, true, {}};
    if (!chip_->closable)
        data.patterns = core::DuiAccessibilityPattern::None;
    return data;
}

bool DuiChip::PerformAccessibilityAction(core::DuiAccessibilityAction action, std::string_view)
{
    if (!Enabled() || action != core::DuiAccessibilityAction::Invoke)
        return false;
    if (chip_->closable)
    {
        if (chip_->closed)
            chip_->closed();
        return true;
    }
    if (chip_->clicked)
    {
        chip_->clicked();
        return true;
    }
    return false;
}

void DuiChip::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty())
        return;
    const core::Rect pill = Bounds();
    const int height = pill.Height();
    const int radius = chip_->cornerRadius < 0 ? height / 2
                                              : (std::min)(chip_->cornerRadius, height / 2);
    const core::Color fill = FillColor();
    if (fill.alpha != 0)
        canvas.FillRoundedRect(pill, radius, fill);
    canvas.StrokeRoundedRect(pill, radius, BorderColor(), 1.0f);
    // 超出标签宽度的文本与图标被裁掉，避免溢出到相邻控件上
    canvas.PushClip(pill);

    const core::Color textColor = Enabled() ? TextColor()
                                            : Theme().Get(core::ThemeSlot::ButtonChoiceDisabledText);
    if (chip_->leadingIcon != nullptr && !chip_->leadingIcon->Empty() && chip_->iconSize > 0)
    {
        const int left = pill.left + chip_->horizontalPadding;
        const int top = (pill.top + pill.bottom) / 2 - chip_->iconSize / 2;
        const core::Size source = chip_->leadingIcon->Size();
        canvas.DrawImage(*chip_->leadingIcon, {0, 0, source.width, source.height},
                         {left, top, left + chip_->iconSize, top + chip_->iconSize});
    }
    if (!chip_->text.empty())
    {
        render::DuiTextStyle style = chip_->textStyle;
        style.color = textColor;
        canvas.DrawText(chip_->text, ContentRect(), style, render::DuiTextAlignment::Start, false);
    }
    // 关闭按钮：悬停时给一层反馈底色，叉号用两条描边线
    const core::Rect close = CloseRect();
    if (!close.Empty())
    {
        const int centerX = (close.left + close.right) / 2;
        const int centerY = (close.top + close.bottom) / 2;
        if (chip_->closeHovered && Enabled())
        {
            const int side = kCloseHitSize;
            canvas.FillRoundedRect({centerX - side / 2, centerY - side / 2,
                                    centerX - side / 2 + side, centerY - side / 2 + side},
                                   2, Theme().Get(core::ThemeSlot::ControlHover));
        }
        render::DuiPath glyph;
        glyph.MoveTo({centerX - kCloseGlyphRadius, centerY - kCloseGlyphRadius});
        glyph.LineTo({centerX + kCloseGlyphRadius, centerY + kCloseGlyphRadius});
        glyph.MoveTo({centerX + kCloseGlyphRadius, centerY - kCloseGlyphRadius});
        glyph.LineTo({centerX - kCloseGlyphRadius, centerY + kCloseGlyphRadius});
        canvas.StrokePath(glyph, textColor, 1.4F);
    }
    canvas.PopClip();
}

} // namespace ysDui::controls::basic
