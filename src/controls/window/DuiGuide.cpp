/**
 * 文件名：DuiGuide.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：新手引导控件实现：遮罩高亮、引导框定位与步骤流转。
 */
#include "ysDui/controls/window/DuiGuide.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/controls/basic/DuiButton.hpp"
#include "ysDui/controls/basic/DuiLabel.hpp"
#include "ysDui/controls/layout/DuiLayout.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiPaintChildren.hpp"
#include "ysDui/render/DuiPath.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::window {
namespace {

constexpr core::Size kDefaultCardSize{280, 140};     // 引导框默认固定尺寸
constexpr core::Color kOverlayColor{0, 0, 0, 100};   // 遮罩色：半透明黑
constexpr int kDefaultHighlightPadding = 8;          // 高亮框默认外扩内边距
constexpr int kHighlightRadius = 4;                  // 高亮框圆角
constexpr float kHighlightStroke = 2.0F;             // 高亮框线宽
constexpr int kCardRadius = 8;                       // 引导框圆角
constexpr int kCardGap = 12;                         // 引导框与高亮元素的间距
constexpr int kViewportMargin = 8;                   // 引导框距覆盖区边缘的最小间距
constexpr int kArrowSize = 8;                        // 指向元素的三角指示器半边宽

constexpr int kCardPadding = 16;     // 引导框内边距
constexpr int kCardInnerGap = 10;    // 引导框内元素间距
constexpr int kTitleHeight = 22;     // 标题行高
constexpr int kFooterHeight = 28;    // 底部（计数与按钮）行高
constexpr int kCounterWidth = 36;    // 计数区宽度
constexpr int kButtonGap = 8;        // 底部按钮间距
constexpr int kButtonHeight = 28;    // 按钮高度
constexpr int kSkipButtonWidth = 64; // 跳过按钮宽度
constexpr int kNavButtonWidth = 72;  // 导航按钮宽度

constexpr const char* kDefaultNextText = "Next";         // 下一步默认文案
constexpr const char* kDefaultPrevText = "Previous";     // 上一步默认文案
constexpr const char* kDefaultSkipText = "Skip";         // 跳过默认文案
constexpr const char* kDefaultFinishText = "Finish";     // 完成默认文案

/** @return 步骤的高亮框矩形（绝对坐标）：步骤元素相对本控件左上角，四周外扩 padding。 */
core::Rect HighlightRect(core::Rect bounds, const DuiGuideStep& step, int defaultPadding)
{
    const int padding = step.highlightPadding >= 0 ? step.highlightPadding : defaultPadding;
    return {bounds.left + step.element.left - padding,
            bounds.top + step.element.top - padding,
            bounds.left + step.element.right + padding,
            bounds.top + step.element.bottom + padding};
}

/**
 * 底部一行按当前可见按钮计算的最小宽度。
 * 底部含：计数、弹性占位、跳过（可选）、上一步（可选）、下一步（可选为「完成」）。
 * 这些子项主轴都是固定宽度，`DuiHBox` 不会压缩它们，因此引导框必须至少这么宽，
 * 否则按钮会溢出引导框右边缘（例如最后一步同时出现跳过/上一步/完成时）。
 */
int FooterMinimumWidth(bool showSkip, bool showPrev)
{
    int children = 2;  // 计数 + 弹性占位
    int width = kCounterWidth;
    if (showSkip)
    {
        width += kSkipButtonWidth;
        ++children;
    }
    if (showPrev)
    {
        width += kNavButtonWidth;
        ++children;
    }
    width += kNavButtonWidth;  // 下一步 / 完成始终显示
    ++children;
    return width + (children - 1) * kButtonGap;
}

/** @return 引导框矩形（绝对坐标），已按放置方向定位并约束在覆盖区内。 */
core::Rect ResolveCardBounds(core::Rect bounds, const DuiGuideStep& step, DuiGuideMode mode,
                             int defaultPadding, core::Size cardSize)
{
    const core::Size size{cardSize.width > 0 ? cardSize.width : kDefaultCardSize.width,
                          cardSize.height > 0 ? cardSize.height : kDefaultCardSize.height};
    int left = bounds.left + (bounds.Width() - size.width) / 2;
    int top = bounds.top + (bounds.Height() - size.height) / 2;
    if (mode != DuiGuideMode::Dialog)
    {
        const core::Rect element = HighlightRect(bounds, step, defaultPadding);
        switch (step.placement)
        {
        case ui::DuiPopupPlacement::Above:
            left = element.left + (element.Width() - size.width) / 2;
            top = element.top - kCardGap - size.height;
            break;
        case ui::DuiPopupPlacement::Right:
            left = element.right + kCardGap;
            top = element.top + (element.Height() - size.height) / 2;
            break;
        case ui::DuiPopupPlacement::CenterOwner:
            break;
        case ui::DuiPopupPlacement::Below:
        default:
            left = element.left + (element.Width() - size.width) / 2;
            top = element.bottom + kCardGap;
            break;
        }
    }
    const int minLeft = bounds.left + kViewportMargin;
    const int minTop = bounds.top + kViewportMargin;
    const int maxLeft = (std::max)(minLeft, bounds.right - kViewportMargin - size.width);
    const int maxTop = (std::max)(minTop, bounds.bottom - kViewportMargin - size.height);
    left = std::clamp(left, minLeft, maxLeft);
    top = std::clamp(top, minTop, maxTop);
    return {left, top, left + size.width, top + size.height};
}

/** 以四条边带围出矩形空洞的方式绘制遮罩，不依赖后端混合模式。 */
void PaintOverlay(render::Canvas& canvas, core::Rect bounds, core::Rect dirty, core::Rect hole)
{
    const core::Rect clip = core::Rect::Intersect(bounds, dirty);
    if (clip.Empty())
        return;
    const core::Rect inner = core::Rect::Intersect(hole, bounds);
    if (inner.Empty())
    {
        canvas.FillRect(clip, kOverlayColor);
        return;
    }
    const core::Rect bands[]{
        {bounds.left, bounds.top, bounds.right, inner.top},
        {bounds.left, inner.bottom, bounds.right, bounds.bottom},
        {bounds.left, inner.top, inner.left, inner.bottom},
        {inner.right, inner.top, bounds.right, inner.bottom},
    };
    for (const core::Rect& band : bands)
    {
        const core::Rect visible = core::Rect::Intersect(band, clip);
        if (!visible.Empty())
            canvas.FillRect(visible, kOverlayColor);
    }
}

/**
 * 在引导框靠向高亮元素的一侧绘制三角指示器。
 * 底边贴在引导框边缘、整体向引导框外突出；**只描两条斜边**，
 * 并把底边下方的卡片边框擦成底色，使三角与卡片连成一体（不再有横线分割）。
 */
void PaintArrow(render::Canvas& canvas, core::Rect card, core::Rect element,
                ui::DuiPopupPlacement placement, core::Color fill, core::Color stroke)
{
    // 三角沿引导框边缘的可用范围：内缩「圆角 + 半底边」，避免底边落在圆角上
    const int inset = kCardRadius + kArrowSize;
    if (card.Empty() || element.Empty() || placement == ui::DuiPopupPlacement::CenterOwner
        || card.Width() < inset * 2 || card.Height() < inset * 2)
        return;

    const int elementCenterX = (element.left + element.right) / 2;
    const int elementCenterY = (element.top + element.bottom) / 2;
    const int centerX = std::clamp(elementCenterX, card.left + inset, card.right - inset);
    const int centerY = std::clamp(elementCenterY, card.top + inset, card.bottom - inset);
    core::Point tip{};
    core::Point first{};
    core::Point second{};
    core::Rect baseErase{};  // 覆盖底边处卡片边框的底色带，使三角与卡片连通
    switch (placement)
    {
    case ui::DuiPopupPlacement::Above:
        tip = {centerX, card.bottom + kArrowSize};
        first = {centerX - kArrowSize, card.bottom};
        second = {centerX + kArrowSize, card.bottom};
        baseErase = {first.x, card.bottom - 1, second.x, card.bottom + 1};
        break;
    case ui::DuiPopupPlacement::Right:
        tip = {card.left - kArrowSize, centerY};
        first = {card.left, centerY - kArrowSize};
        second = {card.left, centerY + kArrowSize};
        baseErase = {card.left - 1, first.y, card.left + 1, second.y};
        break;
    case ui::DuiPopupPlacement::Below:
    default:
        tip = {centerX, card.top - kArrowSize};
        first = {centerX - kArrowSize, card.top};
        second = {centerX + kArrowSize, card.top};
        baseErase = {first.x, card.top - 1, second.x, card.top + 1};
        break;
    }

    render::DuiPath triangle;
    triangle.MoveTo(first);
    triangle.LineTo(tip);
    triangle.LineTo(second);
    triangle.Close();
    canvas.FillPath(triangle, fill);
    // 擦掉底边处的卡片边框：底边不带颜色，三角才与卡片表现为同一个外形
    canvas.FillRect(baseErase, fill);
    // 仅描两条斜边，与卡片外框连成一条闭合轮廓
    render::DuiPath outline;
    outline.MoveTo(first);
    outline.LineTo(tip);
    outline.LineTo(second);
    canvas.StrokePath(outline, stroke, 1.0F);
}

} // namespace

class DuiGuide::Impl {
public:
    std::vector<DuiGuideStep> steps;                 // 引导步骤
    DuiGuideMode mode{DuiGuideMode::Popup};          // 引导框类型
    bool showOverlay{true};                          // 是否绘制遮罩
    int highlightPadding{kDefaultHighlightPadding};  // 默认高亮内边距
    core::Size cardSize{kDefaultCardSize};           // 引导框固定尺寸
    bool hideCounter{};                              // 是否隐藏计数
    bool hidePrev{};                                 // 是否隐藏上一步按钮
    bool hideSkip{};                                 // 是否隐藏跳过按钮
    DuiGuideButtonProps nextProps{kDefaultNextText, true};      // 下一步按钮
    DuiGuideButtonProps prevProps{kDefaultPrevText, false};     // 上一步按钮
    DuiGuideButtonProps skipProps{kDefaultSkipText, false};     // 跳过按钮
    DuiGuideButtonProps finishProps{kDefaultFinishText, true};  // 完成按钮
    int current{};                                   // 当前步骤下标
    int defaultCurrent{};                            // Show 起始步骤
    bool active{};                                   // 是否显示中

    std::function<void(int, int)> changed;              // 步骤变化
    std::function<void(int, int, int)> nextClicked;     // 下一步点击
    std::function<void(int, int, int)> prevClicked;     // 上一步点击
    std::function<void(int, int)> skipped;              // 跳过
    std::function<void(int, int)> finished;             // 完成

    layout::DuiVBox* card{};          // 引导框容器
    basic::DuiLabel* titleLabel{};    // 标题
    basic::DuiLabel* bodyLabel{};     // 正文
    basic::DuiLabel* counterLabel{};  // 步骤计数
    basic::DuiButton* skipButton{};   // 跳过
    basic::DuiButton* prevButton{};   // 上一步
    basic::DuiButton* nextButton{};   // 下一步 / 完成
    core::Rect cardBounds;            // 最近一次布局算出的引导框矩形
};

DuiGuide::DuiGuide() : guide_(std::make_unique<Impl>())
{
    // 引导框：标题 / 正文 / 底部（计数 + 按钮），由控件自身布局
    auto card = std::make_unique<layout::DuiVBox>();
    card->SetPadding(kCardPadding);
    card->SetGap(kCardInnerGap);

    auto title = std::make_unique<basic::DuiLabel>();
    guide_->titleLabel = title.get();
    card->AddChild(std::move(title), layout::DuiLayoutHint{}.Fixed(kTitleHeight));

    auto body = std::make_unique<basic::DuiLabel>();
    body->SetWordWrap(true);
    guide_->bodyLabel = body.get();
    card->AddChild(std::move(body), layout::DuiLayoutHint{}.Flexible());

    auto footer = std::make_unique<layout::DuiHBox>();
    footer->SetGap(kButtonGap);

    auto counter = std::make_unique<basic::DuiLabel>();
    guide_->counterLabel = counter.get();
    footer->AddChild(std::move(counter), layout::DuiLayoutHint{}
                                             .Fixed(kCounterWidth, 20)
                                             .CrossAlign(layout::DuiLayoutAlign::Center));

    // 余量占位：把按钮推到引导框右侧
    footer->AddChild(std::make_unique<core::Control>(), layout::DuiLayoutHint{}.Flexible());

    auto skip = std::make_unique<basic::DuiButton>();
    skip->SetVariant(basic::DuiButtonVariant::Default);
    skip->SetClickHandler([this] { Skip(); });
    guide_->skipButton = skip.get();
    footer->AddChild(std::move(skip), layout::DuiLayoutHint{}.Fixed(kSkipButtonWidth, kButtonHeight));

    auto prev = std::make_unique<basic::DuiButton>();
    prev->SetVariant(basic::DuiButtonVariant::Default);
    prev->SetClickHandler([this] { Prev(); });
    guide_->prevButton = prev.get();
    footer->AddChild(std::move(prev), layout::DuiLayoutHint{}.Fixed(kNavButtonWidth, kButtonHeight));

    auto next = std::make_unique<basic::DuiButton>();
    next->SetVariant(basic::DuiButtonVariant::Primary);
    next->SetClickHandler([this] { Next(); });
    guide_->nextButton = next.get();
    footer->AddChild(std::move(next), layout::DuiLayoutHint{}.Fixed(kNavButtonWidth, kButtonHeight));

    card->AddChild(std::move(footer), layout::DuiLayoutHint{}.Fixed(kFooterHeight));
    guide_->card = card.get();
    AddChild(std::move(card));

    SetVisible(false);
}

DuiGuide::~DuiGuide() = default;

void DuiGuide::SetSteps(std::vector<DuiGuideStep> steps)
{
    guide_->steps = std::move(steps);
    const int total = StepCount();
    guide_->current = std::clamp(guide_->current, 0, (std::max)(0, total - 1));
    guide_->defaultCurrent = std::clamp(guide_->defaultCurrent, 0, (std::max)(0, total - 1));
    if (guide_->active)
    {
        syncCard();
        Layout(Bounds());
    }
}

const std::vector<DuiGuideStep>& DuiGuide::Steps() const { return guide_->steps; }
int DuiGuide::StepCount() const { return static_cast<int>(guide_->steps.size()); }

void DuiGuide::SetMode(DuiGuideMode mode)
{
    guide_->mode = mode;
    if (guide_->active)
        Layout(Bounds());
}
DuiGuideMode DuiGuide::Mode() const { return guide_->mode; }

void DuiGuide::SetShowOverlay(bool show) { guide_->showOverlay = show; }
bool DuiGuide::ShowOverlay() const { return guide_->showOverlay; }
void DuiGuide::SetHighlightPadding(int padding)
{
    guide_->highlightPadding = (std::max)(0, padding);
    if (guide_->active)
        Layout(Bounds());
}
int DuiGuide::HighlightPadding() const { return guide_->highlightPadding; }
void DuiGuide::SetCardSize(core::Size size)
{
    guide_->cardSize = size;
    if (guide_->active)
        Layout(Bounds());
}
core::Size DuiGuide::CardSize() const { return guide_->cardSize; }

void DuiGuide::SetHideCounter(bool hide) { guide_->hideCounter = hide; syncCard(); }
bool DuiGuide::HideCounter() const { return guide_->hideCounter; }
void DuiGuide::SetHidePrev(bool hide)
{
    guide_->hidePrev = hide;
    syncCard();
    if (guide_->active)
        Layout(Bounds());
}
bool DuiGuide::HidePrev() const { return guide_->hidePrev; }
void DuiGuide::SetHideSkip(bool hide)
{
    guide_->hideSkip = hide;
    syncCard();
    if (guide_->active)
        Layout(Bounds());
}
bool DuiGuide::HideSkip() const { return guide_->hideSkip; }

void DuiGuide::SetNextButtonProps(DuiGuideButtonProps props) { guide_->nextProps = std::move(props); syncCard(); }
void DuiGuide::SetPrevButtonProps(DuiGuideButtonProps props) { guide_->prevProps = std::move(props); syncCard(); }
void DuiGuide::SetSkipButtonProps(DuiGuideButtonProps props) { guide_->skipProps = std::move(props); syncCard(); }
void DuiGuide::SetFinishButtonProps(DuiGuideButtonProps props) { guide_->finishProps = std::move(props); syncCard(); }

void DuiGuide::SetCurrent(int current) { applyStep(current, false); }
int DuiGuide::Current() const { return guide_->current; }
void DuiGuide::SetDefaultCurrent(int current) { guide_->defaultCurrent = current; }
int DuiGuide::DefaultCurrent() const { return guide_->defaultCurrent; }

void DuiGuide::SetChangeHandler(std::function<void(int, int)> handler) { guide_->changed = std::move(handler); }
void DuiGuide::SetNextStepClickHandler(std::function<void(int, int, int)> handler)
{
    guide_->nextClicked = std::move(handler);
}
void DuiGuide::SetPrevStepClickHandler(std::function<void(int, int, int)> handler)
{
    guide_->prevClicked = std::move(handler);
}
void DuiGuide::SetSkipHandler(std::function<void(int, int)> handler) { guide_->skipped = std::move(handler); }
void DuiGuide::SetFinishHandler(std::function<void(int, int)> handler) { guide_->finished = std::move(handler); }

void DuiGuide::Show()
{
    const int total = StepCount();
    if (total <= 0)
    {
        Hide();
        return;
    }
    guide_->current = std::clamp(guide_->defaultCurrent, 0, total - 1);
    guide_->active = true;
    SetVisible(true);
    syncCard();
    Layout(Bounds());
}

void DuiGuide::Hide()
{
    guide_->active = false;
    SetVisible(false);
}

bool DuiGuide::Active() const { return guide_->active; }

void DuiGuide::Next()
{
    const int total = StepCount();
    if (total <= 0 || !guide_->active)
        return;
    if (guide_->current >= total - 1)
    {
        const int finishedStep = guide_->current;
        Hide();
        if (guide_->finished)
            guide_->finished(finishedStep, total);
        return;
    }
    const int next = guide_->current + 1;
    if (guide_->nextClicked)
        guide_->nextClicked(next, guide_->current, total);
    applyStep(next, true);
}

void DuiGuide::Prev()
{
    const int total = StepCount();
    if (total <= 0 || !guide_->active || guide_->current <= 0)
        return;
    const int previous = guide_->current - 1;
    if (guide_->prevClicked)
        guide_->prevClicked(previous, guide_->current, total);
    applyStep(previous, true);
}

void DuiGuide::Skip()
{
    const int total = StepCount();
    if (total <= 0 || !guide_->active)
        return;
    const int currentStep = guide_->current;
    Hide();
    if (guide_->skipped)
        guide_->skipped(currentStep, total);
}

void DuiGuide::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    updateCardLayout();
}

core::Size DuiGuide::DesiredSize() const { return {}; }

void DuiGuide::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const Impl& state = *guide_;
    const int total = StepCount();
    if (!state.active || total <= 0 || state.current < 0 || state.current >= total)
        return;
    const core::Rect bounds = Bounds();
    if (bounds.Empty())
        return;

    const DuiGuideStep& step = state.steps[static_cast<std::size_t>(state.current)];
    const core::DuiTheme& theme = Theme();
    const bool highlight = state.mode != DuiGuideMode::Dialog;
    const core::Rect element = highlight ? HighlightRect(bounds, step, state.highlightPadding) : core::Rect{};

    if (state.showOverlay)
    {
        PaintOverlay(canvas, bounds, dirty, element);
        if (!element.Empty())
            canvas.StrokeRoundedRect(element, kHighlightRadius,
                                     theme.Get(core::ThemeSlot::BrandPrimary), kHighlightStroke);
    }

    const core::Rect card = state.cardBounds;
    if (!card.Empty())
    {
        const core::Color surface = theme.Get(core::ThemeSlot::SurfaceBackground);
        const core::Color border = theme.Get(core::ThemeSlot::PopupBorder);
        canvas.FillRoundedRect(card, kCardRadius, surface);
        canvas.StrokeRoundedRect(card, kCardRadius, border, 1.0F);
        if (highlight)
            PaintArrow(canvas, card, element, step.placement, surface, border);
    }

    render::PaintChildren(*this, canvas, dirty);
}

void DuiGuide::applyStep(int index, bool notify)
{
    const int total = StepCount();
    if (index < 0 || index >= total || index == guide_->current)
        return;
    guide_->current = index;
    syncCard();
    Layout(Bounds());
    if (notify && guide_->changed)
        guide_->changed(guide_->current, total);
}

void DuiGuide::syncCard()
{
    Impl& state = *guide_;
    const int total = StepCount();
    if (total <= 0 || state.current < 0 || state.current >= total)
        return;
    const DuiGuideStep& step = state.steps[static_cast<std::size_t>(state.current)];
    const core::DuiTheme& theme = Theme();

    state.titleLabel->SetText(step.title);
    state.titleLabel->SetStyle({theme.Get(core::ThemeSlot::TextDefault), {}, 10, true});
    state.bodyLabel->SetStyle({theme.Get(core::ThemeSlot::TextSubtle), {}, 9, false});
    state.bodyLabel->SetText(step.body);
    state.counterLabel->SetStyle({theme.Get(core::ThemeSlot::TextSubtle), {}, 9, false});
    state.counterLabel->SetText(state.hideCounter
                                   ? std::string{}
                                   : std::to_string(state.current + 1) + "/" + std::to_string(total));

    const bool lastStep = state.current == total - 1;
    state.skipButton->SetVisible(!state.hideSkip);
    // 首步不显示「上一步」，与常见引导交互一致
    state.prevButton->SetVisible(!state.hidePrev && state.current > 0);

    const DuiGuideButtonProps& nextProps = lastStep ? state.finishProps : state.nextProps;
    state.nextButton->SetText(nextProps.text.empty()
                                  ? (lastStep ? kDefaultFinishText : kDefaultNextText)
                                  : nextProps.text);
    state.nextButton->SetVariant(nextProps.primary ? basic::DuiButtonVariant::Primary
                                                   : basic::DuiButtonVariant::Default);

    state.skipButton->SetText(state.skipProps.text.empty() ? kDefaultSkipText : state.skipProps.text);
    state.skipButton->SetVariant(state.skipProps.primary ? basic::DuiButtonVariant::Primary
                                                         : basic::DuiButtonVariant::Default);
    state.prevButton->SetText(state.prevProps.text.empty() ? kDefaultPrevText : state.prevProps.text);
    state.prevButton->SetVariant(state.prevProps.primary ? basic::DuiButtonVariant::Primary
                                                         : basic::DuiButtonVariant::Default);
}

void DuiGuide::updateCardLayout()
{
    Impl& state = *guide_;
    const core::Rect bounds = Bounds();
    const int total = StepCount();
    if (state.card == nullptr || bounds.Empty() || total <= 0 || state.current < 0
        || state.current >= total)
    {
        state.cardBounds = {};
        return;
    }
    // 宽度需容纳当前步骤可见的全部底部按钮；不足时按需要加宽，避免按钮溢出引导框
    core::Size cardSize = state.cardSize;
    const int minimumWidth = kCardPadding * 2
        + FooterMinimumWidth(!state.hideSkip, !state.hidePrev && state.current > 0);
    if (cardSize.width < minimumWidth)
        cardSize.width = minimumWidth;
    state.cardBounds = ResolveCardBounds(bounds, state.steps[static_cast<std::size_t>(state.current)],
                                         state.mode, state.highlightPadding, cardSize);
    state.card->Layout(state.cardBounds);
}

} // namespace ysDui::controls::window
