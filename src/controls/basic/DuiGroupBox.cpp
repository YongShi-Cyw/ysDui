#include "ysDui/controls/basic/DuiGroupBox.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiPath.hpp"

namespace ysDui::controls::basic {
namespace {
int NonNegative(int value) { return (std::max)(0, value); }

/** 标题文字左侧留白（不可勾选时）。 */
constexpr int kTitleInset = 12;
/** 复选框边长。 */
constexpr int kCheckBoxSide = 14;
/** 复选框左边缘相对控件左边界的内缩。 */
constexpr int kCheckBoxInset = 10;
/** 复选框与标题文字之间的间距。 */
constexpr int kCheckBoxTextGap = 6;
/** 标题栏镂空区相对内容左右各留的余量。 */
constexpr int kCutoutMargin = 6;
} // namespace

class DuiGroupBox::Impl {
public:
    core::Control* content{};
    std::string title;
    core::Color border{200, 200, 208, 255};
    core::Color titleBackground{255, 255, 255, 255};
    render::DuiTextStyle titleStyle{{60, 60, 80, 255}, {}, 9, false};
    bool borderOverride{};
    bool titleBackgroundOverride{};
    bool titleStyleOverride{};
    int cornerRadius{};
    int titleStripHeight{24};
    DuiGroupBoxPadding padding;

    bool checkable{};
    bool checked{true};           // 未勾选即停用内容
    bool pressed{};               // 复选框按下中
    bool hovered{};               // 指针悬停在复选框上
    std::function<void(bool)> checkChanged;
};

DuiGroupBox::DuiGroupBox() : groupBox_(std::make_unique<Impl>()) {}
DuiGroupBox::~DuiGroupBox() = default;
DuiGroupBox::DuiGroupBox(DuiGroupBox&&) noexcept = default;
DuiGroupBox& DuiGroupBox::operator=(DuiGroupBox&&) noexcept = default;
void DuiGroupBox::SetTitle(std::string title) { groupBox_->title = std::move(title); }
const std::string& DuiGroupBox::Title() const { return groupBox_->title; }
void DuiGroupBox::SetBorderColor(core::Color color) { groupBox_->border = color; groupBox_->borderOverride = true; }
core::Color DuiGroupBox::BorderColor() const { return groupBox_->borderOverride ? groupBox_->border : Theme().Get(core::ThemeSlot::GroupBorder); }
void DuiGroupBox::SetCornerRadius(int pixels) { groupBox_->cornerRadius = NonNegative(pixels); }
int DuiGroupBox::CornerRadius() const { return groupBox_->cornerRadius; }
void DuiGroupBox::SetTitleStyle(render::DuiTextStyle style) { groupBox_->titleStyle = std::move(style); groupBox_->titleStyleOverride = true; }
const render::DuiTextStyle& DuiGroupBox::TitleStyle() const { return groupBox_->titleStyle; }
void DuiGroupBox::SetTitleBackgroundColor(core::Color color) { groupBox_->titleBackground = color; groupBox_->titleBackgroundOverride = true; }
core::Color DuiGroupBox::TitleBackgroundColor() const { return groupBox_->titleBackgroundOverride ? groupBox_->titleBackground : Theme().Get(core::ThemeSlot::SurfaceBackground); }
void DuiGroupBox::SetPadding(DuiGroupBoxPadding padding) { groupBox_->padding = {NonNegative(padding.left), NonNegative(padding.top), NonNegative(padding.right), NonNegative(padding.bottom)}; }
DuiGroupBoxPadding DuiGroupBox::Padding() const { return groupBox_->padding; }
void DuiGroupBox::SetTitleStripHeight(int pixels) { groupBox_->titleStripHeight = NonNegative(pixels); }
int DuiGroupBox::TitleStripHeight() const { return groupBox_->titleStripHeight; }
void DuiGroupBox::SetContent(std::unique_ptr<core::Control> content) {
    if (groupBox_->content != nullptr) {
        auto previous = RemoveChild(groupBox_->content);
        (void)previous;
    }
    groupBox_->content = content.get();
    if (content) AddChild(std::move(content));
    // 内容根是整棵子树的启用开关，换内容后需要重新套用勾选状态
    ApplyContentEnabled();
    Layout(Bounds());
}
core::Control* DuiGroupBox::Content() const { return groupBox_->content; }

void DuiGroupBox::ApplyContentEnabled()
{
    if (groupBox_->content == nullptr)
        return;
    groupBox_->content->SetEnabled(!groupBox_->checkable || groupBox_->checked);
}

void DuiGroupBox::SetCheckable(bool checkable)
{
    if (groupBox_->checkable == checkable)
        return;
    groupBox_->checkable = checkable;
    if (!checkable)
        groupBox_->pressed = false;
    ApplyContentEnabled();
}

bool DuiGroupBox::Checkable() const { return groupBox_->checkable; }

void DuiGroupBox::SetChecked(bool checked)
{
    if (groupBox_->checked == checked)
        return;
    groupBox_->checked = checked;
    ApplyContentEnabled();
    if (groupBox_->checkChanged)
        groupBox_->checkChanged(checked);
}

bool DuiGroupBox::Checked() const { return groupBox_->checked; }

void DuiGroupBox::SetCheckChangedHandler(std::function<void(bool)> handler)
{
    groupBox_->checkChanged = std::move(handler);
}

core::Rect DuiGroupBox::CheckBoxRect() const
{
    if (!groupBox_->checkable)
        return {};
    const core::Rect bounds = Bounds();
    if (bounds.Empty())
        return {};
    const int centerY = bounds.top + groupBox_->titleStripHeight / 2;
    const int left = bounds.left + kCheckBoxInset;
    return {left, centerY - kCheckBoxSide / 2, left + kCheckBoxSide, centerY - kCheckBoxSide / 2 + kCheckBoxSide};
}

void DuiGroupBox::Toggle()
{
    if (!groupBox_->checkable || !Enabled())
        return;
    SetChecked(!Checked());
}

bool DuiGroupBox::OnEvent(const core::Event& event)
{
    if (!groupBox_->checkable || !Enabled())
        return false;
    const core::Rect check = CheckBoxRect();
    switch (event.type)
    {
    case core::EventType::PointerMove:
        groupBox_->hovered = check.Contains(event.position);
        // 与 DuiCheckBox 一致：悬停移动只在自身被捕获时才算已处理
        return Captured();
    case core::EventType::PointerDown:
        // 只有落在复选框上才接管：点击内容区不应切换勾选
        if (!check.Contains(event.position))
            return false;
        groupBox_->pressed = true;
        SetCaptured(true);
        return true;
    case core::EventType::PointerUp:
        if (!groupBox_->pressed)
            return false;
        groupBox_->pressed = false;
        SetCaptured(false);
        if (check.Contains(event.position))
            Toggle();
        return true;
    case core::EventType::PointerCancel:
        if (!groupBox_->pressed)
            return false;
        groupBox_->pressed = false;
        SetCaptured(false);
        return true;
    case core::EventType::KeyDown:
        // 事件会从子控件冒泡上来，因此必须要求焦点在本控件自身
        if (Focused() && (event.key == core::key::Space || event.key == core::key::Enter))
        {
            Toggle();
            return true;
        }
        return false;
    case core::EventType::PointerLeave:
        groupBox_->hovered = false;
        return false;
    default:
        return false;
    }
}

core::DuiAccessibilityData DuiGroupBox::CreateAccessibilityData() const
{
    if (!groupBox_->checkable)
        return {};
    // 与 DuiCheckBox 语义一致：勾选态即"内容已启用"
    return {core::DuiAccessibilityRole::CheckBox, groupBox_->title,
            groupBox_->checked ? "true" : "false", {}, true, {}};
}

bool DuiGroupBox::PerformAccessibilityAction(core::DuiAccessibilityAction action, std::string_view)
{
    if (!groupBox_->checkable || action != core::DuiAccessibilityAction::Invoke)
        return false;
    Toggle();
    return true;
}

core::Rect DuiGroupBox::ComputeContentRect(core::Rect outer, int titleStripHeight, DuiGroupBoxPadding padding) {
    const int left = outer.left + NonNegative(padding.left);
    const int top = outer.top + NonNegative(titleStripHeight) + NonNegative(padding.top);
    return {left, top, (std::max)(left, outer.right - NonNegative(padding.right)), (std::max)(top, outer.bottom - NonNegative(padding.bottom))};
}
void DuiGroupBox::Layout(core::Rect bounds) {
    SetBounds(bounds);
    if (groupBox_->content) groupBox_->content->Layout(ComputeContentRect(bounds, groupBox_->titleStripHeight, groupBox_->padding));
}
void DuiGroupBox::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible()) return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty()) return;
    const core::DuiTheme& theme = Theme();
    const int borderTop = bounds.top + groupBox_->titleStripHeight / 2;
    canvas.StrokeRoundedRect({bounds.left, borderTop, bounds.right, bounds.bottom}, groupBox_->cornerRadius,
                             BorderColor(), 1.0f);

    // 标题栏：复选框与标题文字都骑在边框上，先按两者范围镂空边框底下的底色
    const core::Rect check = CheckBoxRect();
    const bool hasCheck = !check.Empty();
    render::DuiTextStyle titleStyle = groupBox_->titleStyle;
    if (!groupBox_->titleStyleOverride)
        titleStyle.color = theme.Get(core::ThemeSlot::GroupText);
    const bool hasTitle = !groupBox_->title.empty();
    const render::DuiTextMetrics metrics = hasTitle
        ? canvas.MeasureText(groupBox_->title, titleStyle, {}) : render::DuiTextMetrics{};

    int titleLeft = bounds.left + kTitleInset;
    if (hasCheck)
        titleLeft = check.right + kCheckBoxTextGap;
    int cutLeft = bounds.left + kTitleInset;
    int cutRight = cutLeft;
    int cutHalfHeight = 0;
    if (hasCheck)
    {
        cutLeft = (std::min)(cutLeft, check.left - kCutoutMargin);
        cutRight = (std::max)(cutRight, check.right + kCutoutMargin);
        cutHalfHeight = (std::max)(cutHalfHeight, check.Height() / 2 + 1);
    }
    if (hasTitle)
    {
        cutRight = (std::max)(cutRight, titleLeft + metrics.size.width + kCutoutMargin);
        cutHalfHeight = (std::max)(cutHalfHeight, (metrics.size.height + 1) / 2);
    }
    if (cutRight > cutLeft && cutHalfHeight > 0)
    {
        canvas.FillRect({cutLeft, borderTop - cutHalfHeight, cutRight, borderTop + cutHalfHeight},
                        TitleBackgroundColor());
    }

    if (hasCheck)
    {
        // 复选框：与 DuiCheckBox 同一套配色
        const core::Color primary = theme.Get(core::ThemeSlot::BrandPrimary);
        const bool disabled = !Enabled();
        core::Color glyphFill = Checked() ? primary : theme.Get(core::ThemeSlot::SurfaceBackground);
        core::Color glyphBorder = theme.Get(core::ThemeSlot::ButtonChoiceBorder);
        if (disabled)
        {
            glyphFill = theme.Get(core::ThemeSlot::ControlDisabled);
            glyphBorder = theme.Get(core::ThemeSlot::ButtonChoiceDisabledText);
        }
        else if (groupBox_->hovered)
        {
            glyphBorder = primary;
        }
        canvas.FillRoundedRect(check, 0, glyphFill);
        canvas.StrokeRoundedRect(check, 0, glyphBorder, 1.0f);
        if (Checked())
        {
            render::DuiPath tick;
            tick.MoveTo({check.left + 3, check.top + 7});
            tick.LineTo({check.left + 6, check.top + 10});
            tick.LineTo({check.left + 11, check.top + 4});
            canvas.StrokePath(tick, theme.Get(core::ThemeSlot::TextOnPrimary), 2.0F);
        }
    }

    if (hasTitle)
        canvas.DrawText(groupBox_->title, {titleLeft, bounds.top, bounds.right, bounds.top + groupBox_->titleStripHeight},
                        titleStyle, render::DuiTextAlignment::Start, false);

    if (const auto* renderable = dynamic_cast<const render::DuiRenderable*>(groupBox_->content)) renderable->Paint(canvas, dirty);
}

} // namespace ysDui::controls::basic
