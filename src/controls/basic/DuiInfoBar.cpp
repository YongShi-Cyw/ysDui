#include "ysDui/controls/basic/DuiInfoBar.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {
namespace {
constexpr int AccentStripeWidth = 4;   // 左侧强调条宽度
constexpr int HorizontalPadding = 12;  // 强调条右侧的左右内边距
constexpr int VerticalPadding = 8;     // 上下内边距
constexpr int IconSize = 16;           // 级别图标边长
constexpr int IconGap = 8;             // 图标与文字的间距
constexpr int CloseButtonSize = 20;    // 关闭按钮命中边长
constexpr int LineHeight = 16;         // 单行文本高度（用于不依赖测量器的高度计算）
constexpr int TitleMessageGap = 2;     // 标题行与正文的间距

/** 统计按显式 '\n' 分行的行数；空串按 1 行计（占位高度）。 */
int CountLines(std::string_view text)
{
    if (text.empty())
        return 0;
    int lines = 1;
    for (const char character : text)
    {
        if (character == '\n')
            ++lines;
    }
    return lines;
}
} // namespace

class DuiInfoBar::Impl {
public:
    DuiInfoBarSeverity severity{DuiInfoBarSeverity::Information};
    std::string title;
    std::string message;
    bool closable{};
    bool closeHovered{};
    bool closePressed{};
    std::function<void()> closed;
    render::DuiTextStyle textStyle{{34, 34, 40, 255}, {}, 9, false};
};

DuiInfoBar::DuiInfoBar() : bar_(std::make_unique<Impl>())
{
    SetPointerCursor(core::DuiPointerCursor::Arrow);
}
DuiInfoBar::~DuiInfoBar() = default;
DuiInfoBar::DuiInfoBar(DuiInfoBar&&) noexcept = default;
DuiInfoBar& DuiInfoBar::operator=(DuiInfoBar&&) noexcept = default;

void DuiInfoBar::SetSeverity(DuiInfoBarSeverity severity) { bar_->severity = severity; }
DuiInfoBarSeverity DuiInfoBar::Severity() const { return bar_->severity; }
void DuiInfoBar::SetTitle(std::string title) { bar_->title = std::move(title); }
const std::string& DuiInfoBar::Title() const { return bar_->title; }
void DuiInfoBar::SetMessage(std::string message) { bar_->message = std::move(message); }
const std::string& DuiInfoBar::Message() const { return bar_->message; }
void DuiInfoBar::SetClosable(bool closable) { bar_->closable = closable; }
bool DuiInfoBar::Closable() const { return bar_->closable; }
void DuiInfoBar::SetClosedHandler(std::function<void()> handler) { bar_->closed = std::move(handler); }
void DuiInfoBar::SetTextStyle(render::DuiTextStyle style) { bar_->textStyle = std::move(style); }
const render::DuiTextStyle& DuiInfoBar::TextStyle() const { return bar_->textStyle; }

core::Rect DuiInfoBar::ContentRect() const
{
    const core::Rect bounds = Bounds();
    const int left = bounds.left + AccentStripeWidth + HorizontalPadding;
    int right = bounds.right - HorizontalPadding;
    if (bar_->closable)
        right -= CloseButtonSize + IconGap;
    // 文本区从图标右侧开始；右边界受关闭按钮让位约束，小于左边界时收敛到左边界
    const int textLeft = left + IconSize + IconGap;
    const int contentRight = (std::max)(left, right);
    return {(std::min)(textLeft, contentRight), bounds.top + VerticalPadding,
            contentRight, bounds.bottom - VerticalPadding};
}

core::Rect DuiInfoBar::CloseRect() const
{
    if (!bar_->closable)
        return {};
    const core::Rect bounds = Bounds();
    const int top = bounds.top + (bounds.Height() - CloseButtonSize) / 2;
    return {bounds.right - HorizontalPadding - CloseButtonSize, top,
            bounds.right - HorizontalPadding, top + CloseButtonSize};
}

int DuiInfoBar::TextLineCount() const
{
    const int titleLines = CountLines(bar_->title);
    const int messageLines = CountLines(bar_->message);
    const int total = titleLines + messageLines;
    return total > 0 ? total : 1;
}

int DuiInfoBar::LineCount() const { return TextLineCount(); }

core::Size DuiInfoBar::DesiredSize() const
{
    const int height = VerticalPadding * 2 + TextLineCount() * LineHeight
        + (CountLines(bar_->title) > 0 && CountLines(bar_->message) > 0 ? TitleMessageGap : 0);
    // 宽度返回 0：横幅应占满容器宽度，由父容器决定；高度才是有意义的内容约束
    return {0, height};
}

bool DuiInfoBar::OnEvent(const core::Event& event)
{
    if (!Enabled() || !bar_->closable)
        return false;
    const core::Rect close = CloseRect();
    const bool contains = close.Contains(event.position);
    if (event.type == core::EventType::PointerMove)
    {
        bar_->closeHovered = contains;
        SetPointerCursor(contains ? core::DuiPointerCursor::Hand : core::DuiPointerCursor::Arrow);
        return Captured();
    }
    if (event.type == core::EventType::PointerLeave)
    {
        bar_->closeHovered = false;
        bar_->closePressed = false;
        SetPointerCursor(core::DuiPointerCursor::Arrow);
        return false;
    }
    if (event.type == core::EventType::PointerDown && contains)
    {
        bar_->closePressed = true;
        SetCaptured(true);
        return true;
    }
    if (event.type == core::EventType::PointerCancel && Captured())
    {
        bar_->closePressed = false;
        SetCaptured(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && Captured())
    {
        const bool activate = bar_->closePressed && contains;
        bar_->closePressed = false;
        SetCaptured(false);
        if (activate)
        {
            SetVisible(false);
            if (bar_->closed)
                bar_->closed();
        }
        return true;
    }
    return false;
}

namespace {
/** @return 级别对应的强调色主题槽。 */
core::ThemeSlot AccentSlot(DuiInfoBarSeverity severity)
{
    switch (severity)
    {
    case DuiInfoBarSeverity::Success: return core::ThemeSlot::MessageSuccessFill;
    case DuiInfoBarSeverity::Warning: return core::ThemeSlot::MessageWarningFill;
    case DuiInfoBarSeverity::Error: return core::ThemeSlot::MessageErrorFill;
    case DuiInfoBarSeverity::Information:
    default: return core::ThemeSlot::MessageInformationFill;
    }
}
} // namespace

void DuiInfoBar::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    const core::Color accent = theme.Get(AccentSlot(bar_->severity));

    canvas.FillRect(bounds, theme.Get(core::ThemeSlot::MessageBackground));
    // 左侧强调条：不随脏区裁切而变窄，始终贴控件左边缘
    const core::Rect stripe{Bounds().left, Bounds().top,
                            (std::min)(Bounds().left + AccentStripeWidth, Bounds().right), Bounds().bottom};
    if (!stripe.Empty())
        canvas.FillRect(core::Rect::Intersect(stripe, dirty), accent);

    // 级别图标：实心圆 + 白色符号，与 DuiMessageBox 的图标语义保持一致
    const int iconLeft = Bounds().left + AccentStripeWidth + HorizontalPadding;
    const int iconTop = Bounds().top + (Bounds().Height() - IconSize) / 2;
    const core::Rect icon{iconLeft, iconTop, iconLeft + IconSize, iconTop + IconSize};
    canvas.FillEllipse(icon, accent);
    const render::DuiTextStyle iconStyle{theme.Get(core::ThemeSlot::DialogTitleText), {}, 9, true};
    switch (bar_->severity)
    {
    case DuiInfoBarSeverity::Success: canvas.DrawText("v", icon, iconStyle, render::DuiTextAlignment::Center, false); break;
    case DuiInfoBarSeverity::Warning: canvas.DrawText("!", icon, iconStyle, render::DuiTextAlignment::Center, false); break;
    case DuiInfoBarSeverity::Error: canvas.DrawText("x", icon, iconStyle, render::DuiTextAlignment::Center, false); break;
    case DuiInfoBarSeverity::Information:
    default: canvas.DrawText("i", icon, iconStyle, render::DuiTextAlignment::Center, false); break;
    }

    const core::Rect content = ContentRect();
    if (!content.Empty())
    {
        render::DuiTextStyle titleStyle = bar_->textStyle;
        titleStyle.bold = true;
        render::DuiTextStyle messageStyle = bar_->textStyle;
        messageStyle.color = theme.Get(core::ThemeSlot::TextSubtle);

        if (!bar_->title.empty() && !bar_->message.empty())
        {
            const int titleHeight = CountLines(bar_->title) * LineHeight;
            canvas.DrawText(bar_->title,
                            {content.left, content.top, content.right, content.top + titleHeight},
                            titleStyle, render::DuiTextAlignment::Start, false);
            canvas.DrawText(bar_->message,
                            {content.left, content.top + titleHeight + TitleMessageGap, content.right, content.bottom},
                            messageStyle, render::DuiTextAlignment::Start, true);
        }
        else if (!bar_->title.empty())
        {
            canvas.DrawText(bar_->title, content, titleStyle, render::DuiTextAlignment::Start, true);
        }
        else if (!bar_->message.empty())
        {
            canvas.DrawText(bar_->message, content, messageStyle, render::DuiTextAlignment::Start, true);
        }
    }

    if (bar_->closable)
    {
        const core::Rect close = CloseRect();
        if (!close.Empty())
        {
            if (bar_->closePressed)
                canvas.FillRoundedRect(close, 3, theme.Get(core::ThemeSlot::DialogCaptionPressed));
            else if (bar_->closeHovered)
                canvas.FillRoundedRect(close, 3, theme.Get(core::ThemeSlot::DialogCaptionHover));
            const int centerX = (close.left + close.right) / 2;
            const int centerY = (close.top + close.bottom) / 2;
            render::DuiPath cross;
            cross.MoveTo({centerX - 4, centerY - 4});
            cross.LineTo({centerX + 4, centerY + 4});
            cross.MoveTo({centerX + 4, centerY - 4});
            cross.LineTo({centerX - 4, centerY + 4});
            canvas.StrokePath(cross, theme.Get(core::ThemeSlot::TextSubtle), 1.4F);
        }
    }
}

core::DuiAccessibilityData DuiInfoBar::CreateAccessibilityData() const
{
    const std::string name = bar_->title.empty() ? bar_->message : bar_->title;
    return {core::DuiAccessibilityRole::Text, name, bar_->message, {}, true, {}};
}

} // namespace ysDui::controls::basic
