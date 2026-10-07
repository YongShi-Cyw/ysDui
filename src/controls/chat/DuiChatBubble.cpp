/**
 * 文件名：DuiChatBubble.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：对话消息气泡的布局与绘制。
 */
#include "ysDui/controls/chat/DuiChatBubble.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiPaintChildren.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::chat {
namespace {
/** 头像与气泡之间的水平间距。 */
constexpr int kAvatarGap = 8;
/** 元信息行高度。 */
constexpr int kMetaHeight = 16;
/** 气泡与元信息行之间的竖直间距。 */
constexpr int kMetaGap = 4;
/** 无内容且未布局时用于估算的默认宽度。 */
constexpr int kFallbackWidth = 320;
/** 圆角自动值的上限。 */
constexpr int kAutoCornerRadiusLimit = 12;
/** 元信息各段之间的分隔符。 */
constexpr const char* kMetaSeparator = " · ";

/** @return 该角色是否靠右显示。 */
bool AlignsRight(DuiChatRole role)
{
    return role == DuiChatRole::User;
}

/** @return 状态对应的默认文字；`Complete` 返回空串。 */
std::string DefaultStatusText(DuiChatStatus status)
{
    switch (status)
    {
    case DuiChatStatus::Sending: return "发送中…";
    case DuiChatStatus::Streaming: return "生成中…";
    case DuiChatStatus::Failed: return "发送失败";
    case DuiChatStatus::Stopped: return "已停止";
    case DuiChatStatus::Complete: return {};
    }
    return {};
}
} // namespace

class DuiChatBubble::Impl {
public:
    core::Control* content{};
    DuiChatRole role{DuiChatRole::Assistant};
    std::string name;
    std::string timestamp;
    std::string statusText;
    std::shared_ptr<const render::DuiImage> avatar;
    int avatarSize{32};
    DuiChatStatus status{DuiChatStatus::Complete};
    int maxWidth{};
    int horizontalPadding{12};
    int verticalPadding{8};
    int cornerRadius{-1};
    core::Color fill{};
    core::Color border{};
    bool fillOverride{};
    bool borderOverride{};
    // 布局结果
    core::Rect bubble;
    core::Rect avatarRect;
    core::Rect contentRect;
    core::Rect metaRect;
    int measuredContentHeight{};
};

DuiChatBubble::DuiChatBubble() : bubble_(std::make_unique<Impl>()) {}
DuiChatBubble::~DuiChatBubble() = default;
DuiChatBubble::DuiChatBubble(DuiChatBubble&&) noexcept = default;
DuiChatBubble& DuiChatBubble::operator=(DuiChatBubble&&) noexcept = default;

void DuiChatBubble::SetRole(DuiChatRole role)
{
    if (bubble_->role == role)
        return;
    bubble_->role = role;
    InvalidateLayout();
}
DuiChatRole DuiChatBubble::Role() const { return bubble_->role; }

void DuiChatBubble::SetContent(std::unique_ptr<core::Control> content)
{
    if (bubble_->content != nullptr)
        (void)RemoveChild(bubble_->content);
    bubble_->content = content.get();
    if (content != nullptr)
        AddChild(std::move(content));
    bubble_->measuredContentHeight = 0;
    InvalidateLayout();
}
core::Control* DuiChatBubble::Content() const { return bubble_->content; }

void DuiChatBubble::SetName(std::string name) { bubble_->name = std::move(name); InvalidateLayout(); }
const std::string& DuiChatBubble::Name() const { return bubble_->name; }
void DuiChatBubble::SetAvatar(std::shared_ptr<const render::DuiImage> avatar)
{
    bubble_->avatar = std::move(avatar);
    InvalidateLayout();
}
const std::shared_ptr<const render::DuiImage>& DuiChatBubble::Avatar() const { return bubble_->avatar; }
void DuiChatBubble::SetAvatarSize(int pixels)
{
    bubble_->avatarSize = (std::max)(1, pixels);
    InvalidateLayout();
}
int DuiChatBubble::AvatarSize() const { return bubble_->avatarSize; }
void DuiChatBubble::SetTimestamp(std::string text)
{
    bubble_->timestamp = std::move(text);
    InvalidateLayout();
}
const std::string& DuiChatBubble::Timestamp() const { return bubble_->timestamp; }
void DuiChatBubble::SetStatus(DuiChatStatus status)
{
    if (bubble_->status == status)
        return;
    bubble_->status = status;
    InvalidateLayout();
}
DuiChatStatus DuiChatBubble::Status() const { return bubble_->status; }
void DuiChatBubble::SetStatusText(std::string text)
{
    bubble_->statusText = std::move(text);
    InvalidateLayout();
}
std::string DuiChatBubble::StatusText() const
{
    return bubble_->statusText.empty() ? DefaultStatusText(bubble_->status) : bubble_->statusText;
}
void DuiChatBubble::SetMaxWidth(int pixels)
{
    bubble_->maxWidth = (std::max)(0, pixels);
    InvalidateLayout();
}
int DuiChatBubble::MaxWidth() const { return bubble_->maxWidth; }
void DuiChatBubble::SetContentPadding(int horizontal, int vertical)
{
    bubble_->horizontalPadding = (std::max)(0, horizontal);
    bubble_->verticalPadding = (std::max)(0, vertical);
    InvalidateLayout();
}
int DuiChatBubble::HorizontalPadding() const { return bubble_->horizontalPadding; }
int DuiChatBubble::VerticalPadding() const { return bubble_->verticalPadding; }
void DuiChatBubble::SetCornerRadius(int pixels) { bubble_->cornerRadius = pixels; }
int DuiChatBubble::CornerRadius() const { return bubble_->cornerRadius; }

void DuiChatBubble::SetFillColor(core::Color color) { bubble_->fill = color; bubble_->fillOverride = true; }
core::Color DuiChatBubble::FillColor() const
{
    if (bubble_->fillOverride)
        return bubble_->fill;
    if (bubble_->role == DuiChatRole::User)
        return Theme().Get(core::ThemeSlot::ChatUserBubbleFill);
    // 系统提示用浅灰底：助手气泡是白底，若沿用同一配色会在白色页面上"消失"
    if (bubble_->role == DuiChatRole::System)
        return Theme().Get(core::ThemeSlot::SurfaceAlternateBackground);
    return Theme().Get(core::ThemeSlot::ChatAssistantBubbleFill);
}
void DuiChatBubble::SetBorderColor(core::Color color) { bubble_->border = color; bubble_->borderOverride = true; }
core::Color DuiChatBubble::BorderColor() const
{
    if (bubble_->borderOverride)
        return bubble_->border;
    return Theme().Get(core::ThemeSlot::BorderLight);
}

core::Rect DuiChatBubble::BubbleRect() const { return bubble_->bubble; }
core::Rect DuiChatBubble::AvatarRect() const { return bubble_->avatarRect; }
core::Rect DuiChatBubble::ContentRect() const { return bubble_->contentRect; }
core::Rect DuiChatBubble::MetaRect() const { return bubble_->metaRect; }

bool DuiChatBubble::HasMeta() const
{
    return !bubble_->name.empty() || !bubble_->timestamp.empty() || !StatusText().empty();
}

core::Size DuiChatBubble::DesiredSize() const
{
    const int width = Bounds().Width() > 0 ? Bounds().Width() : kFallbackWidth;
    int contentHeight = bubble_->measuredContentHeight;
    if (contentHeight <= 0 && bubble_->content != nullptr)
        contentHeight = bubble_->content->DesiredSize().height;
    int height = contentHeight + bubble_->verticalPadding * 2;
    if (HasMeta())
        height += kMetaGap + kMetaHeight;
    if (contentHeight <= 0 && Bounds().Height() > 0)
    {
        // 内容不报告首选高度时以给定高度为准（与 Layout 的收缩规则一致）
        height = (std::max)(height, Bounds().Height());
    }
    return {width, height};
}

void DuiChatBubble::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    Impl* state = bubble_.get();
    const bool hasAvatar = state->avatar != nullptr && !state->avatar->Empty();
    const int avatarSide = hasAvatar ? state->avatarSize : 0;

    // 头像列：用户靠右、其余靠左；气泡占剩余宽度
    int bubbleAreaLeft = bounds.left;
    int bubbleAreaRight = bounds.right;
    state->avatarRect = {};
    if (hasAvatar)
    {
        const int top = bounds.top;
        if (AlignsRight(state->role))
        {
            state->avatarRect = {bounds.right - avatarSide, top, bounds.right, top + avatarSide};
            bubbleAreaRight = state->avatarRect.left - kAvatarGap;
        }
        else
        {
            state->avatarRect = {bounds.left, top, bounds.left + avatarSide, top + avatarSide};
            bubbleAreaLeft = state->avatarRect.right + kAvatarGap;
        }
    }
    const int bubbleAreaWidth = (std::max)(0, bubbleAreaRight - bubbleAreaLeft);
    const int bubbleWidth = state->maxWidth > 0 ? (std::min)(bubbleAreaWidth, state->maxWidth) : bubbleAreaWidth;
    const int contentWidth = (std::max)(1, bubbleWidth - state->horizontalPadding * 2);

    // 先按最终内容宽度测量一次：正文高度依赖换行结果，必须先量后排
    int contentHeight{};
    if (state->content != nullptr)
    {
        state->content->Layout({0, 0, contentWidth, 0});
        contentHeight = state->content->DesiredSize().height;
    }
    state->measuredContentHeight = contentHeight;

    const int bubbleHeight = contentHeight > 0
        ? contentHeight + state->verticalPadding * 2
        // 内容不报告首选高度（如 DuiLabel）：沿用调用方给定的高度，避免收缩成空气泡
        : (std::max)(0, bounds.Height() - (HasMeta() ? kMetaGap + kMetaHeight : 0));
    int totalHeight = bubbleHeight;
    if (HasMeta())
        totalHeight += kMetaGap + kMetaHeight;
    // 高度随内容收缩（与 DuiExpander 同一做法）
    SetBounds({bounds.left, bounds.top, bounds.right, bounds.top + totalHeight});

    const int bubbleLeft = AlignsRight(state->role) ? bubbleAreaRight - bubbleWidth : bubbleAreaLeft;
    state->bubble = {bubbleLeft, bounds.top, bubbleLeft + bubbleWidth, bounds.top + bubbleHeight};
    state->contentRect = {state->bubble.left + state->horizontalPadding,
                          state->bubble.top + state->verticalPadding,
                          state->bubble.right - state->horizontalPadding,
                          state->bubble.bottom - state->verticalPadding};
    if (state->content != nullptr)
        state->content->Layout(state->contentRect);

    state->metaRect = {};
    if (HasMeta())
    {
        const int metaTop = state->bubble.bottom + kMetaGap;
        if (AlignsRight(state->role))
        {
            // 元信息与气泡右边界对齐
            state->metaRect = {bounds.left, metaTop, state->bubble.right, metaTop + kMetaHeight};
        }
        else
        {
            state->metaRect = {state->bubble.left, metaTop, bounds.right, metaTop + kMetaHeight};
        }
    }
}

void DuiChatBubble::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const Impl* state = bubble_.get();
    const core::DuiTheme& theme = Theme();
    if (!state->avatarRect.Empty())
    {
        const core::Size source = state->avatar->Size();
        canvas.PushClip(core::Rect::Intersect(state->avatarRect, dirty));
        canvas.DrawImage(*state->avatar, {0, 0, source.width, source.height}, state->avatarRect);
        canvas.PopClip();
    }
    if (!state->bubble.Empty() && !core::Rect::Intersect(state->bubble, dirty).Empty())
    {
        const int height = state->bubble.Height();
        const int radius = state->cornerRadius < 0
            ? (std::min)(kAutoCornerRadiusLimit, height / 2)
            : (std::min)(state->cornerRadius, height / 2);
        const core::Color fill = FillColor();
        if (fill.alpha != 0)
            canvas.FillRoundedRect(state->bubble, radius, fill);
        const core::Color border = BorderColor();
        if (border.alpha != 0)
            canvas.StrokeRoundedRect(state->bubble, radius, border, 1.0f);
    }
    // 正文裁剪在气泡内：内容（如宽表横向滚动）不应溢出到气泡外
    if (!state->contentRect.Empty())
    {
        canvas.PushClip(state->contentRect);
        render::PaintChildren(*this, canvas, dirty);
        canvas.PopClip();
    }
    if (!state->metaRect.Empty())
    {
        std::string meta = state->name;
        const auto append = [&meta](std::string_view part)
        {
            if (part.empty())
                return;
            if (!meta.empty())
                meta += kMetaSeparator;
            meta += part;
        };
        append(state->timestamp);
        append(StatusText());
        if (!meta.empty())
        {
            render::DuiTextStyle style;
            style.color = theme.Get(core::ThemeSlot::ChatMetaText);
            style.pointSize = 8;
            canvas.DrawText(meta, state->metaRect, style,
                            AlignsRight(state->role) ? render::DuiTextAlignment::End
                                                     : render::DuiTextAlignment::Start,
                            false);
        }
    }
}

core::DuiAccessibilityData DuiChatBubble::CreateAccessibilityData() const
{
    // 消息按分组暴露，名称取发送者名，值取状态文字便于屏幕阅读器播报
    return {core::DuiAccessibilityRole::Group, bubble_->name, StatusText(), {}, false, {}};
}

} // namespace ysDui::controls::chat
