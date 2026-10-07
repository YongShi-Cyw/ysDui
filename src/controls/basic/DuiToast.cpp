#include "ysDui/controls/basic/DuiToast.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {
namespace {
constexpr int HorizontalPadding = 14;
constexpr int VerticalPadding = 8;
constexpr int DefaultLineHeight = 18;
constexpr int DefaultDurationMilliseconds = 3000;
constexpr int DefaultFadeMilliseconds = 200;
constexpr int CloseButtonSize = 18; // 关闭按钮命中边长
constexpr int CloseButtonRightMargin = 6;

/** 按比例把 tint 混入 base，用于从主题基色推倒浅色外观的底色与描边。 */
core::Color Blend(core::Color base, core::Color tint, double amount)
{
    const auto mix = [amount](unsigned char from, unsigned char to)
    {
        const double value = static_cast<double>(from) * (1.0 - amount)
            + static_cast<double>(to) * amount;
        const double clamped = (std::min)(255.0, (std::max)(0.0, value));
        return static_cast<unsigned char>(clamped + 0.5);
    };
    return {mix(base.red, tint.red), mix(base.green, tint.green), mix(base.blue, tint.blue), 255};
}

/** @return 该类型对应的强调色主题槽。 */
core::ThemeSlot TypeFillSlot(DuiToastType type)
{
    switch (type)
    {
    case DuiToastType::Success: return core::ThemeSlot::MessageSuccessFill;
    case DuiToastType::Warning: return core::ThemeSlot::MessageWarningFill;
    case DuiToastType::Error: return core::ThemeSlot::MessageErrorFill;
    case DuiToastType::Information:
    default: return core::ThemeSlot::MessageInformationFill;
    }
}

/**
 * 在圆形底上绘制类型符号（全部用矢量图元，不依赖字体，任意边长都清晰）。
 * @param disc 圆形底的外接矩形。
 * @param type 消息类型，决定符号形状。
 * @param color 符号颜色。
 */
void PaintTypeSymbol(render::Canvas& canvas, core::Rect disc, DuiToastType type, core::Color color)
{
    const int centerX = (disc.left + disc.right) / 2;
    const int centerY = (disc.top + disc.bottom) / 2;
    const int radius = (std::max)(3, (std::min)(disc.Width(), disc.Height()) / 2);
    const int halfStroke = (std::max)(1, radius / 5);          // 笔画半宽
    const float strokeWidth = static_cast<float>(halfStroke * 2);

    switch (type)
    {
    case DuiToastType::Warning:
    {
        // 感叹号：上方竖条 + 下方圆点
        canvas.FillRoundedRect({centerX - halfStroke, centerY - radius / 2,
                                centerX + halfStroke, centerY + radius / 6},
                               halfStroke, color);
        canvas.FillRoundedRect({centerX - halfStroke, centerY + radius / 3,
                                centerX + halfStroke, centerY + radius / 3 + halfStroke * 2},
                               halfStroke, color);
        break;
    }
    case DuiToastType::Success:
    {
        // 对勾
        render::DuiPath check;
        check.MoveTo({centerX - radius / 2, centerY});
        check.LineTo({centerX - radius / 6, centerY + radius / 3});
        check.LineTo({centerX + radius / 2, centerY - radius / 3});
        canvas.StrokePath(check, color, strokeWidth);
        break;
    }
    case DuiToastType::Error:
    {
        // 叉号
        render::DuiPath cross;
        cross.MoveTo({centerX - radius / 3, centerY - radius / 3});
        cross.LineTo({centerX + radius / 3, centerY + radius / 3});
        cross.MoveTo({centerX + radius / 3, centerY - radius / 3});
        cross.LineTo({centerX - radius / 3, centerY + radius / 3});
        canvas.StrokePath(cross, color, strokeWidth);
        break;
    }
    case DuiToastType::Information:
    default:
    {
        // 小写 i：上方圆点 + 下方竖条
        canvas.FillRoundedRect({centerX - halfStroke, centerY - radius / 2,
                                centerX + halfStroke, centerY - radius / 2 + halfStroke * 2},
                               halfStroke, color);
        canvas.FillRoundedRect({centerX - halfStroke, centerY - radius / 6,
                                centerX + halfStroke, centerY + radius / 2},
                               halfStroke, color);
        break;
    }
    }
}
}

class DuiToast::Impl {
public:
    core::AnimationClock* clock{};
    core::AnimationClock::TaskId task{};
    render::DuiTextMeasurer* textMeasurer{};
    std::shared_ptr<const render::DuiImage> icon;
    render::DuiTextStyle textStyle{{255, 255, 255, 255}, {}, 9, false};
    std::string text;
    core::Color background{50, 50, 50, 255};
    DuiToastPlacement placement{DuiToastPlacement::Top};
    DuiToastType type{DuiToastType::Information};
    DuiToastAppearance appearance{DuiToastAppearance::Dark};
    std::function<void()> closed;
    bool closeHovered{};
    bool closePressed{};
    bool showClose{};
    bool textStyleOverride{};
    bool backgroundOverride{};
    core::Rect layoutBounds;
    core::Rect contentBounds;
    int durationMilliseconds{DefaultDurationMilliseconds};
    int fadeMilliseconds{DefaultFadeMilliseconds};
    int cornerRadius{}; // 默认直角；需要胶囊外观时可显式设置
    int iconSize{16};
    int iconGap{8};
    int edgeOffset{40};
    int maxWidth{};
    int generation{};
};

DuiToast::DuiToast() : toast_(std::make_unique<Impl>())
{
    SetVisible(false);
}

DuiToast::~DuiToast()
{
    CancelAnimation();
}

void DuiToast::SetAnimationClock(core::AnimationClock* clock)
{
    if (toast_->clock == clock)
        return;
    CancelAnimation();
    toast_->clock = clock;
    // 若在尚无时钟时已显示（例如构建期预置的消息），补排停留任务，
    // 使 duration 在时钟接入后依然生效，否则该条会永久停留。
    if (toast_->clock != nullptr && Active() && OverlayAlpha() >= 1.0
        && toast_->durationMilliseconds > 0)
    {
        StartHold(toast_->generation);
    }
}

void DuiToast::Show(std::string text)
{
    if (text.empty()) {
        HideNow();
        return;
    }
    CancelAnimation();
    toast_->text = std::move(text);
    const int generation = ++toast_->generation;
    SetVisible(true);
    const core::Rect available = Parent() ? Parent()->Bounds() : toast_->layoutBounds;
    Layout(available);
    if (toast_->fadeMilliseconds == 0 || toast_->clock == nullptr) {
        SetOverlayAlpha(1.0);
        if (toast_->clock != nullptr)
            StartHold(generation);
    } else {
        StartFadeIn(generation);
    }
}

void DuiToast::HideNow()
{
    CancelAnimation();
    ++toast_->generation;
    SetOverlayAlpha(0.0);
    SetVisible(false);
}

bool DuiToast::Active() const { return Visible(); }
double DuiToast::Opacity() const { return OverlayAlpha(); }
const std::string& DuiToast::Text() const { return toast_->text; }
void DuiToast::SetDurationMilliseconds(int milliseconds) { toast_->durationMilliseconds = (std::max)(0, milliseconds); }
int DuiToast::DurationMilliseconds() const { return toast_->durationMilliseconds; }
void DuiToast::SetFadeMilliseconds(int milliseconds) { toast_->fadeMilliseconds = (std::max)(0, milliseconds); }
int DuiToast::FadeMilliseconds() const { return toast_->fadeMilliseconds; }
void DuiToast::SetTextMeasurer(render::DuiTextMeasurer* measurer) { toast_->textMeasurer = measurer; }
void DuiToast::SetTextStyle(render::DuiTextStyle style) { toast_->textStyle = std::move(style); toast_->textStyleOverride = true; }
void DuiToast::SetBackgroundColor(core::Color color) { toast_->background = color; toast_->backgroundOverride = true; }
void DuiToast::SetCornerRadius(int pixels) { toast_->cornerRadius = (std::max)(0, pixels); }
void DuiToast::SetIcon(std::shared_ptr<const render::DuiImage> image) { toast_->icon = std::move(image); }
void DuiToast::SetIconSize(int pixels) { toast_->iconSize = (std::max)(1, pixels); }
void DuiToast::SetIconGap(int pixels) { toast_->iconGap = (std::max)(0, pixels); }
void DuiToast::SetEdgeOffset(int pixels) { toast_->edgeOffset = pixels; }
void DuiToast::SetTopOffset(int pixels) { SetEdgeOffset(pixels); }
int DuiToast::EdgeOffset() const { return toast_->edgeOffset; }
void DuiToast::SetPlacement(DuiToastPlacement placement) { toast_->placement = placement; }
DuiToastPlacement DuiToast::Placement() const { return toast_->placement; }
void DuiToast::SetType(DuiToastType type) { toast_->type = type; }
DuiToastType DuiToast::Type() const { return toast_->type; }
void DuiToast::SetAppearance(DuiToastAppearance appearance) { toast_->appearance = appearance; }
DuiToastAppearance DuiToast::Appearance() const { return toast_->appearance; }
void DuiToast::SetShowClose(bool showClose) { toast_->showClose = showClose; }
bool DuiToast::ShowClose() const { return toast_->showClose; }
void DuiToast::SetClosedHandler(std::function<void()> handler) { toast_->closed = std::move(handler); }
core::Rect DuiToast::CloseRect() const
{
    if (!toast_->showClose || !Active())
        return {};
    const core::Rect bounds = Bounds();
    const int top = bounds.top + (bounds.Height() - CloseButtonSize) / 2;
    return {bounds.right - CloseButtonRightMargin - CloseButtonSize, top,
            bounds.right - CloseButtonRightMargin, top + CloseButtonSize};
}
void DuiToast::SetMaxWidth(int pixels) { toast_->maxWidth = (std::max)(0, pixels); }
core::Rect DuiToast::ContentRect() const { return toast_->contentBounds; }

int DuiToast::MeasureWidth(int textPixels, bool hasIcon, int iconPixels, int iconGapPixels)
{
    textPixels = (std::max)(0, textPixels);
    iconPixels = (std::max)(0, iconPixels);
    iconGapPixels = (std::max)(0, iconGapPixels);
    return HorizontalPadding * 2 + textPixels + (hasIcon ? iconPixels + iconGapPixels : 0);
}

std::string DuiToast::ApplyEllipsis(std::string_view text, int maximumCharacters)
{
    if (maximumCharacters <= 0)
        return std::string(text);
    int characters{};
    std::size_t end{};
    while (end < text.size() && characters < maximumCharacters)
    {
        const unsigned char first = static_cast<unsigned char>(text[end]);
        const std::size_t width = first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
        end += (std::min)(width, text.size() - end);
        ++characters;
    }
    if (end == text.size())
        return std::string(text);
    if (maximumCharacters < 3)
        return std::string(text.substr(0, end));

    characters = 0;
    end = 0;
    while (end < text.size() && characters < maximumCharacters - 3)
    {
        const unsigned char first = static_cast<unsigned char>(text[end]);
        const std::size_t width = first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
        end += (std::min)(width, text.size() - end);
        ++characters;
    }
    return std::string(text.substr(0, end)) + "...";
}

void DuiToast::Layout(core::Rect availableBounds)
{
    if (availableBounds.Empty())
        return;
    toast_->layoutBounds = availableBounds;
    const render::DuiTextMetrics metrics = toast_->textMeasurer
        ? toast_->textMeasurer->MeasureText(toast_->text, toast_->textStyle, {})
        : render::DuiTextMetrics{{static_cast<int>(toast_->text.size()) * toast_->textStyle.pointSize,
                                  DefaultLineHeight}, 1, DefaultLineHeight};
    int width = MeasureWidth(metrics.size.width, static_cast<bool>(toast_->icon), toast_->iconSize, toast_->iconGap);
    if (toast_->maxWidth > 0)
        width = (std::min)(width, toast_->maxWidth);
    const int height = (std::max)(metrics.size.height, toast_->icon ? toast_->iconSize : 0) + VerticalPadding * 2;
    const int left = availableBounds.left + (availableBounds.Width() - width) / 2;
    // 停靠边决定从可用区域的顶边向下、还是底边向上量取偏移
    const int top = toast_->placement == DuiToastPlacement::Bottom
        ? availableBounds.bottom - toast_->edgeOffset - height
        : availableBounds.top + toast_->edgeOffset;
    toast_->contentBounds = {left, top, left + width, top + height};
    SetBounds(toast_->contentBounds);
}

void DuiToast::CancelAnimation()
{
    if (toast_->clock != nullptr && toast_->task != 0)
        toast_->clock->Cancel(toast_->task);
    toast_->task = 0;
}

void DuiToast::StartFadeIn(int generation)
{
    const double start = OverlayAlpha();
    toast_->task = toast_->clock->Schedule(toast_->fadeMilliseconds, [this, generation, start](double progress) {
        if (generation != toast_->generation)
            return;
        SetOverlayAlpha(start + (1.0 - start) * progress);
        if (progress >= 1.0)
            StartHold(generation);
    });
}

void DuiToast::StartHold(int generation)
{
    // duration 为 0 表示不自动关闭（对应 Message 的 duration: 0）
    if (toast_->durationMilliseconds <= 0)
    {
        toast_->task = 0;
        return;
    }
    toast_->task = toast_->clock->Schedule(toast_->durationMilliseconds, [this, generation](double progress) {
        if (generation == toast_->generation && progress >= 1.0) {
            if (toast_->fadeMilliseconds == 0)
                FinishHide();
            else
                StartFadeOut(generation);
        }
    });
}

void DuiToast::StartFadeOut(int generation)
{
    const double start = OverlayAlpha();
    toast_->task = toast_->clock->Schedule(toast_->fadeMilliseconds, [this, generation, start](double progress) {
        if (generation != toast_->generation)
            return;
        SetOverlayAlpha(start * (1.0 - progress));
        if (progress >= 1.0)
            FinishHide();
    });
}

void DuiToast::FinishHide()
{
    SetOverlayAlpha(0.0);
    SetVisible(false);
    toast_->task = 0;
    toast_->closeHovered = false;
    toast_->closePressed = false;
    if (toast_->closed)
        toast_->closed();
}

core::Control* DuiToast::HitTest(core::Point point)
{
    // 覆盖层默认穿透；仅关闭按钮占用命中，避免遮挡下层控件
    const core::Rect close = CloseRect();
    return (!close.Empty() && close.Contains(point)) ? this : nullptr;
}

bool DuiToast::OnEvent(const core::Event& event)
{
    if (!toast_->showClose)
        return false;
    const core::Rect close = CloseRect();
    if (close.Empty())
        return false;
    const bool contains = close.Contains(event.position);
    if (event.type == core::EventType::PointerMove)
    {
        toast_->closeHovered = contains;
        return true;
    }
    if (event.type == core::EventType::PointerLeave)
    {
        toast_->closeHovered = false;
        toast_->closePressed = false;
        return false;
    }
    if (event.type == core::EventType::PointerDown && contains)
    {
        toast_->closePressed = true;
        return true;
    }
    if (event.type == core::EventType::PointerCancel)
    {
        toast_->closePressed = false;
        return false;
    }
    if (event.type == core::EventType::PointerUp && toast_->closePressed)
    {
        toast_->closePressed = false;
        // 立即结束并通知关闭：与自动消失共用同一条生命周期出口
        CancelAnimation();
        ++toast_->generation;
        FinishHide();
        return true;
    }
    return false;
}

void DuiToast::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!Active() || OverlayAlpha() <= 0.0)
        return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    const core::Color accent = theme.Get(TypeFillSlot(toast_->type));
    const bool light = toast_->appearance == DuiToastAppearance::Light;

    // 底色：显式覆盖 > 浅色外观按类型混出 > 深色默认
    core::Color background = toast_->backgroundOverride ? toast_->background
        : light ? Blend(theme.Get(core::ThemeSlot::MessageBackground), accent, 0.10)
                : theme.Get(core::ThemeSlot::ToastBackground);
    background.alpha = static_cast<unsigned char>(static_cast<double>(background.alpha) * OverlayAlpha());
    canvas.FillRoundedRect(bounds, toast_->cornerRadius, background);
    if (light)
    {
        core::Color border = Blend(theme.Get(core::ThemeSlot::MessageBackground), accent, 0.32);
        border.alpha = static_cast<unsigned char>(static_cast<double>(border.alpha) * OverlayAlpha());
        canvas.StrokeRoundedRect(bounds, toast_->cornerRadius, border, 1.0F);
    }

    int left = bounds.left + HorizontalPadding;
    if (toast_->icon)
    {
        const int top = bounds.top + (bounds.Height() - toast_->iconSize) / 2;
        canvas.DrawImage(*toast_->icon, {left, top, left + toast_->iconSize, top + toast_->iconSize});
        left += toast_->iconSize + toast_->iconGap;
    }
    else
    {
        // 未提供自定义图像时，按类型绘制矢量图标：底色圆 + 符号。
        // 深色外观用白底 + 类型色符号（在饱和底色上清晰）；浅色外观用类型色底 + 白符号。
        const int top = bounds.top + (bounds.Height() - toast_->iconSize) / 2;
        const core::Rect disc{left, top, left + toast_->iconSize, top + toast_->iconSize};
        core::Color discColor = light ? accent : core::Color{255, 255, 255, 255};
        core::Color symbolColor = light ? theme.Get(core::ThemeSlot::TextOnPrimary) : accent;
        const double alpha = OverlayAlpha();
        discColor.alpha = static_cast<unsigned char>(static_cast<double>(discColor.alpha) * alpha);
        symbolColor.alpha = static_cast<unsigned char>(static_cast<double>(symbolColor.alpha) * alpha);
        canvas.FillEllipse(disc, discColor);
        PaintTypeSymbol(canvas, disc, toast_->type, symbolColor);
        left += toast_->iconSize + toast_->iconGap;
    }

    render::DuiTextStyle style = toast_->textStyle;
    if (!toast_->textStyleOverride)
        style.color = light ? accent : theme.Get(core::ThemeSlot::TextOnPrimary);
    style.color.alpha = static_cast<unsigned char>(static_cast<double>(style.color.alpha) * OverlayAlpha());

    // 关闭按钮占位：文本区右边界相应左移，避免文字压在按钮下
    const core::Rect close = CloseRect();
    const int right = close.Empty() ? bounds.right - HorizontalPadding : close.left - 4;
    const int availableWidth = (std::max)(0, right - left);
    std::string text = toast_->text;
    if (toast_->maxWidth > 0 && availableWidth > 0 && toast_->textMeasurer != nullptr &&
        toast_->textMeasurer->MeasureText(text, style, {}).size.width > availableWidth) {
        const int maximumCharacters = (std::max)(1, static_cast<int>(text.size()) * availableWidth /
            (std::max)(1, toast_->textMeasurer->MeasureText(text, style, {}).size.width));
        text = ApplyEllipsis(text, maximumCharacters);
    }
    canvas.DrawText(text, {left, bounds.top + VerticalPadding, right, bounds.bottom - VerticalPadding},
                    style, render::DuiTextAlignment::Start, false);

    if (!close.Empty())
    {
        if (toast_->closePressed)
        {
            core::Color pressed = theme.Get(core::ThemeSlot::ControlPressed);
            pressed.alpha = static_cast<unsigned char>(static_cast<double>(pressed.alpha) * OverlayAlpha());
            canvas.FillRoundedRect(close, 3, pressed);
        }
        else if (toast_->closeHovered)
        {
            core::Color hovered = theme.Get(core::ThemeSlot::ControlHover);
            hovered.alpha = static_cast<unsigned char>(static_cast<double>(hovered.alpha) * OverlayAlpha());
            canvas.FillRoundedRect(close, 3, hovered);
        }
        core::Color glyph = light ? accent : theme.Get(core::ThemeSlot::TextOnPrimary);
        glyph.alpha = static_cast<unsigned char>(static_cast<double>(glyph.alpha) * OverlayAlpha());
        const int centerX = (close.left + close.right) / 2;
        const int centerY = (close.top + close.bottom) / 2;
        render::DuiPath cross;
        cross.MoveTo({centerX - 3, centerY - 3});
        cross.LineTo({centerX + 3, centerY + 3});
        cross.MoveTo({centerX + 3, centerY - 3});
        cross.LineTo({centerX - 3, centerY + 3});
        canvas.StrokePath(cross, glyph, 1.4F);
    }
}

} // namespace ysDui::controls::basic
