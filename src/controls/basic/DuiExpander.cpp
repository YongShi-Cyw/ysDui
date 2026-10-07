#include "ysDui/controls/basic/DuiExpander.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiPath.hpp"

namespace ysDui::controls::basic {
namespace {
constexpr core::Color kTitleForeground{0, 100, 135, 255};
constexpr core::Color kTitleHoverBackground{218, 236, 240, 255};
constexpr core::Color kTitleHoverForeground{0, 0, 0, 255};

int NonNegative(int value) { return (std::max)(0, value); }
int Rounded(double value) { return static_cast<int>(value + 0.5); }
double EaseOutCubic(double progress)
{
    const double inverse = 1.0 - progress;
    return 1.0 - inverse * inverse * inverse;
}

struct DuiExpanderCallbackState final {
    DuiExpander* owner{};
};
}

class DuiExpander::Impl {
public:
    core::AnimationClock* clock{};
    core::AnimationClock::TaskId task{};
    std::shared_ptr<DuiExpanderCallbackState> callbackState;
    core::Control* content{};
    std::string title;
    std::function<void(bool)> changed;
    std::function<void()> desiredSizeChanged;
    render::DuiTextStyle titleStyle{{40, 40, 50, 255}, {}, 9, false};
    core::Color titleBackground{240, 240, 244, 255};
    core::Color border{210, 210, 216, 255};
    bool titleStyleOverride{};
    bool titleBackgroundOverride{};
    bool borderOverride{};
    core::Size contentSize{120, 36};
    DuiExpanderPadding padding;
    bool expanded{true};
    double progress{1.0};
    int duration{200};
    int titleHeight{28};
};

DuiExpander::DuiExpander() : expander_(std::make_unique<Impl>()) {
    expander_->callbackState = std::make_shared<DuiExpanderCallbackState>();
    expander_->callbackState->owner = this;
}
DuiExpander::~DuiExpander() {
    SetAnimationClock(nullptr);
    expander_->callbackState->owner = nullptr;
}
void DuiExpander::SetTitle(std::string title) { expander_->title = std::move(title); }
const std::string& DuiExpander::Title() const { return expander_->title; }
void DuiExpander::SetAnimationClock(core::AnimationClock* clock) {
    if (expander_->clock && expander_->task) expander_->clock->Cancel(expander_->task);
    expander_->clock = clock;
    expander_->task = {};
}
void DuiExpander::SetAnimationDuration(int milliseconds) { expander_->duration = NonNegative(milliseconds); }
int DuiExpander::AnimationDuration() const { return expander_->duration; }
void DuiExpander::SetExpandedChangedHandler(std::function<void(bool)> handler) { expander_->changed = std::move(handler); }
void DuiExpander::SetDesiredSizeChangedHandler(std::function<void()> handler)
{
    expander_->desiredSizeChanged = std::move(handler);
}
void DuiExpander::NotifyDesiredSizeChanged()
{
    if (expander_->desiredSizeChanged)
        expander_->desiredSizeChanged();
}
void DuiExpander::SetContent(std::unique_ptr<core::Control> content) {
    if (expander_->content) (void)RemoveChild(expander_->content);
    expander_->content = content.get();
    if (content) AddChild(std::move(content));
    Layout(Bounds());
}
core::Control* DuiExpander::Content() const { return expander_->content; }
void DuiExpander::SetContentSize(core::Size size)
{
    expander_->contentSize = {NonNegative(size.width), NonNegative(size.height)};
    Layout(Bounds());
    NotifyDesiredSizeChanged();
}
core::Size DuiExpander::ContentSize() const { return expander_->contentSize; }
void DuiExpander::SetTitleStripHeight(int pixels)
{
    expander_->titleHeight = (std::max)(16, pixels);
    Layout(Bounds());
    NotifyDesiredSizeChanged();
}
int DuiExpander::TitleStripHeight() const { return expander_->titleHeight; }
void DuiExpander::SetPadding(DuiExpanderPadding padding)
{
    expander_->padding = {NonNegative(padding.left), NonNegative(padding.top),
                          NonNegative(padding.right), NonNegative(padding.bottom)};
    Layout(Bounds());
    NotifyDesiredSizeChanged();
}
DuiExpanderPadding DuiExpander::Padding() const { return expander_->padding; }
void DuiExpander::SetTitleStyle(render::DuiTextStyle style) { expander_->titleStyle = std::move(style); expander_->titleStyleOverride = true; }
void DuiExpander::SetTitleBackgroundColor(core::Color color) { expander_->titleBackground = color; expander_->titleBackgroundOverride = true; }
void DuiExpander::SetBorderColor(core::Color color) { expander_->border = color; expander_->borderOverride = true; }
core::Rect DuiExpander::TitleRect() const { const auto bounds = Bounds(); return {bounds.left, bounds.top, bounds.right, bounds.top + expander_->titleHeight}; }
core::Size DuiExpander::DesiredSize() const {
    const int contentHeight = expander_->padding.top + expander_->contentSize.height + expander_->padding.bottom;
    return {(std::max)(120, expander_->contentSize.width + expander_->padding.left + expander_->padding.right), expander_->titleHeight + Rounded(expander_->progress * contentHeight)};
}
void DuiExpander::Layout(core::Rect bounds) {
    SetBounds({bounds.left, bounds.top, bounds.right, bounds.top + DesiredSize().height});
    if (!expander_->content) return;
    const auto current = Bounds();
    const int top = current.top + expander_->titleHeight + expander_->padding.top;
    expander_->content->SetBounds({current.left + expander_->padding.left, top, (std::max)(current.left + expander_->padding.left, current.right - expander_->padding.right), top + expander_->contentSize.height});
    expander_->content->SetVisible(expander_->expanded || expander_->progress > 0.0);
}
void DuiExpander::SetExpanded(bool expanded, bool animate) {
    const bool changed = expander_->expanded != expanded;
    const double target = expanded ? 1.0 : 0.0;
    if (!changed && (!animate || expander_->progress == target)) return;
    if (expander_->clock && expander_->task) expander_->clock->Cancel(expander_->task);
    expander_->task = {};
    expander_->expanded = expanded;
    if (!animate || !expander_->clock || expander_->duration == 0) {
        expander_->progress = target;
        Layout(Bounds());
        NotifyDesiredSizeChanged();
    } else {
        const double from = expander_->progress;
        const std::weak_ptr<DuiExpanderCallbackState> callbackState = expander_->callbackState;
        expander_->task = expander_->clock->Schedule(expander_->duration, [callbackState, from, target](double fraction) {
            const auto state = callbackState.lock();
            if (!state || !state->owner) return;
            state->owner->expander_->progress = from + (target - from) * EaseOutCubic(fraction);
            state->owner->Layout(state->owner->Bounds());
            if (fraction >= 1.0) state->owner->expander_->task = {};
            state->owner->NotifyDesiredSizeChanged();
        });
    }
    if (changed && expander_->changed) expander_->changed(expanded);
}
bool DuiExpander::Expanded() const { return expander_->expanded; }
core::DuiAccessibilityData DuiExpander::CreateAccessibilityData() const
{
    core::DuiAccessibilityData data{
        core::DuiAccessibilityRole::Group, expander_->title,
        expander_->expanded ? "expanded" : "collapsed", {}, true, {}};
    data.patterns = core::DuiAccessibilityPattern::ExpandCollapse;
    data.expanded = expander_->expanded;
    return data;
}
bool DuiExpander::PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                             std::string_view)
{
    if (!Enabled() || (action != core::DuiAccessibilityAction::Expand
                       && action != core::DuiAccessibilityAction::Collapse))
    {
        return false;
    }
    SetExpanded(action == core::DuiAccessibilityAction::Expand);
    return true;
}
bool DuiExpander::OnEvent(const core::Event& event) {
    if (!Enabled()) return false;
    if (event.type == core::EventType::PointerMove) {
        SetHovered(TitleRect().Contains(event.position));
        return false;
    }
    const bool keyboardToggle = event.type == core::EventType::KeyDown
        && (event.key == core::key::Enter || event.key == core::key::Space);
    if (!keyboardToggle
        && (event.type != core::EventType::PointerDown || !TitleRect().Contains(event.position)))
    {
        return false;
    }
    SetExpanded(!Expanded(), true);
    return true;
}
void DuiExpander::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible()) return;
    const core::Rect visible = core::Rect::Intersect(Bounds(), dirty);
    if (visible.Empty()) return;
    const core::Rect title = TitleRect();
    const core::Color titleBackground = expander_->titleBackgroundOverride ? expander_->titleBackground
        : Hovered() ? kTitleHoverBackground : Theme().Get(core::ThemeSlot::SurfaceBackground);
    const core::Color border = expander_->borderOverride ? expander_->border
        : Theme().Get(core::ThemeSlot::PanelBorder);
    render::DuiTextStyle titleStyle = expander_->titleStyle;
    if (!expander_->titleStyleOverride) {
        titleStyle.color = Hovered() ? kTitleHoverForeground : kTitleForeground;
        titleStyle.bold = true;
    }
    canvas.FillRect(title, titleBackground);
    if (expander_->progress > 0.0 && visible.bottom > title.bottom)
        canvas.FillRect({visible.left + 1, title.bottom, visible.right - 1, visible.bottom - 1},
                        Theme().Get(core::ThemeSlot::SurfaceBackground));
    canvas.FillRect({visible.left, visible.top, visible.right, visible.top + 1}, border);
    canvas.FillRect({visible.left, visible.bottom - 1, visible.right, visible.bottom}, border);
    canvas.FillRect({visible.left, visible.top + 1, visible.left + 1, visible.bottom - 1}, border);
    canvas.FillRect({visible.right - 1, visible.top + 1, visible.right, visible.bottom - 1}, border);
    const int centerX = title.left + 14;
    const int centerY = (title.top + title.bottom) / 2 - 1;
    render::DuiPath chevron;
    if (expander_->expanded)
    {
        chevron.MoveTo({centerX - 4, centerY - 2});
        chevron.LineTo({centerX + 4, centerY - 2});
        chevron.LineTo({centerX, centerY + 2});
    }
    else
    {
        chevron.MoveTo({centerX - 2, centerY - 4});
        chevron.LineTo({centerX - 2, centerY + 4});
        chevron.LineTo({centerX + 2, centerY});
    }
    chevron.Close();
    canvas.FillPath(chevron, Hovered() ? kTitleHoverForeground : kTitleForeground);
    canvas.DrawText(expander_->title, {title.left + 28, title.top, title.right - 8, title.bottom}, titleStyle, render::DuiTextAlignment::Start, false);
    if (!expander_->content || expander_->progress <= 0.0) return;
    canvas.PushClip({visible.left, title.bottom, visible.right, visible.bottom});
    if (const auto* renderable = dynamic_cast<const render::DuiRenderable*>(expander_->content)) renderable->Paint(canvas, visible);
    canvas.PopClip();
}

} // namespace ysDui::controls::basic
