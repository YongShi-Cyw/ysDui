/**
 * 文件名：DuiCard.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-31
 * 用途：实现卡片容器的布局和绘制。
 */
#include "ysDui/controls/basic/DuiCard.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {
namespace {
int NonNegative(int value) { return (std::max)(0, value); }
}

class DuiCard::Impl {
public:
    core::Control* content{};
    std::string title;
    std::string subtitle;
    DuiCardPadding padding;
    int cornerRadius{6};
    int headerHeight{40};
};

DuiCard::DuiCard() : card_(std::make_unique<Impl>()) {}
DuiCard::~DuiCard() = default;
DuiCard::DuiCard(DuiCard&&) noexcept = default;
DuiCard& DuiCard::operator=(DuiCard&&) noexcept = default;
void DuiCard::SetTitle(std::string title) { card_->title = std::move(title); }
const std::string& DuiCard::Title() const { return card_->title; }
void DuiCard::SetSubtitle(std::string subtitle) { card_->subtitle = std::move(subtitle); }
const std::string& DuiCard::Subtitle() const { return card_->subtitle; }
void DuiCard::SetPadding(DuiCardPadding padding)
{
    card_->padding = {NonNegative(padding.left), NonNegative(padding.top),
                      NonNegative(padding.right), NonNegative(padding.bottom)};
    Layout(Bounds());
}
DuiCardPadding DuiCard::Padding() const { return card_->padding; }
void DuiCard::SetCornerRadius(int pixels) { card_->cornerRadius = NonNegative(pixels); }
int DuiCard::CornerRadius() const { return card_->cornerRadius; }
void DuiCard::SetHeaderHeight(int pixels) { card_->headerHeight = NonNegative(pixels); Layout(Bounds()); }
int DuiCard::HeaderHeight() const { return card_->headerHeight; }
void DuiCard::SetContent(std::unique_ptr<core::Control> content)
{
    if (card_->content != nullptr)
        (void)RemoveChild(card_->content);
    card_->content = content.get();
    if (content)
        AddChild(std::move(content));
    Layout(Bounds());
}
core::Control* DuiCard::Content() const { return card_->content; }
core::Rect DuiCard::ContentRect() const
{
    const core::Rect bounds = Bounds();
    const int top = bounds.top + card_->headerHeight + card_->padding.top;
    const int left = bounds.left + card_->padding.left;
    return {left, top, (std::max)(left, bounds.right - card_->padding.right),
            (std::max)(top, bounds.bottom - card_->padding.bottom)};
}
core::Size DuiCard::DesiredSize() const { return {240, 120}; }
void DuiCard::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    if (card_->content != nullptr)
        card_->content->SetBounds(ContentRect());
}
void DuiCard::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    canvas.FillRoundedRect(bounds, card_->cornerRadius, theme.Get(core::ThemeSlot::SurfaceBackground));
    if (card_->headerHeight > 0 && (!card_->title.empty() || !card_->subtitle.empty()))
    {
        const core::Rect header{bounds.left, bounds.top,
                                bounds.right, (std::min)(bounds.bottom, bounds.top + card_->headerHeight)};
        canvas.FillRect(header, theme.Get(core::ThemeSlot::PanelHeaderBackground));
        const core::Rect textArea{header.left + card_->padding.left, header.top + 2,
                                  header.right - card_->padding.right, header.bottom - 2};
        if (!card_->title.empty() && !card_->subtitle.empty())
        {
            const int split = textArea.top + textArea.Height() / 2;
            canvas.DrawText(card_->title, {textArea.left, textArea.top, textArea.right, split},
                            {theme.Get(core::ThemeSlot::PanelText), {}, 10, true},
                            render::DuiTextAlignment::Start, false);
            canvas.DrawText(card_->subtitle, {textArea.left, split, textArea.right, textArea.bottom},
                            {theme.Get(core::ThemeSlot::TextSubtle), {}, 8, false},
                            render::DuiTextAlignment::Start, false);
        }
        else if (!card_->title.empty())
            canvas.DrawText(card_->title, textArea, {theme.Get(core::ThemeSlot::PanelText), {}, 10, true},
                            render::DuiTextAlignment::Start, false);
        else
            canvas.DrawText(card_->subtitle, textArea, {theme.Get(core::ThemeSlot::TextSubtle), {}, 8, false},
                            render::DuiTextAlignment::Start, false);
    }
    canvas.StrokeRoundedRect(bounds, card_->cornerRadius, theme.Get(core::ThemeSlot::PanelBorder), 1.0f);
    if (const auto* renderable = dynamic_cast<const render::DuiRenderable*>(card_->content))
        renderable->Paint(canvas, dirty);
}
} // namespace ysDui::controls::basic
