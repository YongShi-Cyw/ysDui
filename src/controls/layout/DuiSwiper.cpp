/**
 * 文件名：DuiSwiper.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：实现轮播容器控件的页面布局、切换动画、自动播放与导航指示器绘制。
 */
#include "ysDui/controls/layout/DuiSwiper.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiPaintChildren.hpp"
#include "ysDui/render/DuiPath.hpp"

namespace ysDui::controls::layout {
namespace {

/** 水平轮播默认首选尺寸（16:9 视口）。 */
constexpr int DEFAULT_WIDTH = 480;
constexpr int DEFAULT_HEIGHT = 270;
/** 自动播放与过渡时长默认值（取 Element Plus 默认值）。 */
constexpr int DEFAULT_INTERVAL_MILLISECONDS = 5000;
constexpr int DEFAULT_DURATION_MILLISECONDS = 300;
/** 卡片模式下相邻页相对当前页的缩放比例（210/332，取自 Element Plus 默认值）。 */
constexpr double DEFAULT_CARD_SCALE = 210.0 / 332.0;
/**
 * 卡片模式下相邻页中心与当前页中心的间距（相对主轴长度）。
 * 取 0.75 让缩小的相邻页只有一小条越过视口边缘，形成参考文档中的“露边”效果。
 */
constexpr double CARD_STRIDE_RATIO = 0.75;
/** 相邻页缩放后的可见距离上限：超出后不再绘制，避免整页遍历时产生离屏绘制。 */
constexpr double CARD_VISIBLE_LIMIT = 1.9;
/** 默认样式下只有过渡中的两页可见，距离达到 1 即视为完全离屏。 */
constexpr double SLIDE_VISIBLE_LIMIT = 0.999;

/** 指示器半透明底色/前景的 alpha 上限。 */
constexpr int INDICATOR_INACTIVE_ALPHA = 120;
constexpr int ARROW_SURFACE_ALPHA = 210;

/** 单个导航器尺寸档位的几何参数。 */
struct DuiSwiperMetrics final
{
    int dotRadius;     // 圆点半径
    int barLength;     // 条状指示器长度（主轴方向）
    int barThickness;  // 条状指示器厚度
    int gap;           // 指示器槽位间距
    int arrowExtent;   // 翻页按钮边长
    int arrowInset;    // 翻页按钮距视口边缘的内缩
    int navStrip;      // Outside 摆放时占用的条带厚度
};

[[nodiscard]] DuiSwiperMetrics MetricsFor(DuiSwiperNavigationSize size)
{
    switch (size)
    {
        case DuiSwiperNavigationSize::Small:
            return {3, 12, 2, 6, 22, 6, 18};
        case DuiSwiperNavigationSize::Large:
            return {5, 22, 3, 10, 36, 12, 30};
        case DuiSwiperNavigationSize::Medium:
        default:
            return {4, 16, 2, 8, 28, 8, 24};
    }
}

[[nodiscard]] double EaseOutCubic(double value)
{
    const double remaining = 1.0 - std::clamp(value, 0.0, 1.0);
    return 1.0 - remaining * remaining * remaining;
}

[[nodiscard]] core::Color WithAlpha(core::Color color, int alpha)
{
    color.alpha = static_cast<unsigned char>(std::clamp(alpha, 0, 255));
    return color;
}

/** 绘制控件；页面本身不是渲染节点时回退为绘制其子节点。 */
void PaintControl(const core::Control& control, render::Canvas& canvas, core::Rect dirty)
{
    if (const auto* renderable = dynamic_cast<const render::DuiRenderable*>(&control))
        renderable->Paint(canvas, dirty);
    else
        render::PaintChildren(control, canvas, dirty);
}

/** 动画回调通过该共享状态回访控件，避免回调越过控件生命周期使用悬空指针。 */
struct CallbackState final
{
    DuiSwiper* owner{};
};

} // namespace

class DuiSwiper::Impl
{
public:
    DuiSwiper* owner{};
    std::vector<core::Control*> pages;
    std::shared_ptr<CallbackState> callbackState;

    int current{-1};      // 当前（过渡目标）页下标
    int from{-1};         // 过渡起始页下标
    int step{1};          // 过渡方向：+1 前进，-1 后退
    double progress{1.0}; // 过渡进度：1 表示已停稳
    bool pointerInside{}; // 指针是否位于轮播区域内

    DuiSwiperAnimation animation{DuiSwiperAnimation::Slide};
    DuiSwiperDirection direction{DuiSwiperDirection::Horizontal};
    DuiSwiperType type{DuiSwiperType::Default};
    DuiSwiperTrigger trigger{DuiSwiperTrigger::Click};
    double cardScale{DEFAULT_CARD_SCALE};
    DuiSwiperNavigation navigation{};

    bool autoplay{true};
    int interval{DEFAULT_INTERVAL_MILLISECONDS};
    int duration{DEFAULT_DURATION_MILLISECONDS};
    bool loop{true};
    bool stopOnHover{true};

    core::AnimationClock* clock{};
    core::AnimationClock::TaskId transitionTask{};
    core::AnimationClock::TaskId autoplayTask{};
    std::function<void(int)> changeHandler;

    core::Rect laidOutBounds{}; // 上次 Layout 使用的边界，用于检测外部只改了 Bounds 的情况
    core::Rect viewport{};
    core::Rect navigationArea{};
    core::Rect previousButton{};
    core::Rect nextButton{};
    std::vector<core::Rect> indicatorBounds;

    [[nodiscard]] DuiSwiperMetrics metrics() const { return MetricsFor(navigation.size); }
    [[nodiscard]] bool animating() const { return progress < 1.0; }
    [[nodiscard]] int pageCount() const { return static_cast<int>(pages.size()); }

    void cancelTransition()
    {
        if (clock != nullptr && transitionTask != 0)
            clock->Cancel(transitionTask);
        transitionTask = {};
    }

    void cancelAutoplay()
    {
        if (clock != nullptr && autoplayTask != 0)
            clock->Cancel(autoplayTask);
        autoplayTask = {};
    }

    /** 停稳到目标页并恢复自动播放计时。 */
    void settle()
    {
        progress = 1.0;
        from = current;
        if (owner != nullptr)
            owner->Layout(owner->Bounds());
        scheduleAutoplay();
    }

    /** 沿主轴的长度：水平取视口宽，垂直取视口高。 */
    [[nodiscard]] int mainExtent() const
    {
        return direction == DuiSwiperDirection::Horizontal ? viewport.Width() : viewport.Height();
    }

    [[nodiscard]] core::Rect offsetRect(core::Rect rect, int offset) const
    {
        if (direction == DuiSwiperDirection::Horizontal)
            return {rect.left + offset, rect.top, rect.right + offset, rect.bottom};
        return {rect.left, rect.top + offset, rect.right, rect.bottom + offset};
    }

    /**
     * 页面相对当前页的有符号距离（单位：页）。
     * 停稳时为整数（0 表示当前页）；过渡期间叠加 `step * (1 - progress)`，
     * 使旧页从 0 滑向 -step，新页从 step 滑向 0。
     */
    [[nodiscard]] double pageDistance(int index) const
    {
        const int count = pageCount();
        if (count <= 0 || current < 0 || current >= count)
            return 0.0;
        int offset = index - current;
        if (loop)
        {
            offset = ((offset % count) + count) % count;
            if (offset > count / 2)
                offset -= count;
        }
        double distance = static_cast<double>(offset);
        if (animating())
            distance += static_cast<double>(step) * (1.0 - progress);
        return distance;
    }

    [[nodiscard]] double cardScaleForDistance(double distance) const
    {
        const double shrink = (1.0 - cardScale) * (std::min)(1.0, std::abs(distance));
        return std::clamp(1.0 - shrink, 0.05, 1.0);
    }

    [[nodiscard]] int mainOffset(double distance) const
    {
        const double stride = type == DuiSwiperType::Card
            ? static_cast<double>(mainExtent()) * CARD_STRIDE_RATIO
            : static_cast<double>(mainExtent());
        return static_cast<int>(std::lround(distance * stride));
    }

    [[nodiscard]] double visibleLimit() const
    {
        return type == DuiSwiperType::Card ? CARD_VISIBLE_LIMIT : SLIDE_VISIBLE_LIMIT;
    }

    [[nodiscard]] bool pageVisible(int index) const
    {
        if (animation == DuiSwiperAnimation::Fade && animating())
            return index == current || index == from;
        return std::abs(pageDistance(index)) < visibleLimit();
    }

    /** 依据摆放位置计算视口与导航区域。 */
    void updateAreas()
    {
        if (owner == nullptr)
            return;
        const core::Rect bounds = owner->Bounds();
        viewport = bounds;
        navigationArea = {};
        const int strip = metrics().navStrip;
        if (direction == DuiSwiperDirection::Horizontal)
        {
            if (navigation.placement == DuiSwiperPlacement::Outside)
            {
                const int split = (std::max)(bounds.top, bounds.bottom - strip);
                viewport.bottom = split;
                navigationArea = {bounds.left, split, bounds.right, bounds.bottom};
            }
            else
            {
                const int split = (std::max)(bounds.top, bounds.bottom - strip);
                navigationArea = {bounds.left, split, bounds.right, bounds.bottom};
            }
        }
        else
        {
            if (navigation.placement == DuiSwiperPlacement::Outside)
            {
                const int split = (std::max)(bounds.left, bounds.right - strip);
                viewport.right = split;
                navigationArea = {split, bounds.top, bounds.right, bounds.bottom};
            }
            else
            {
                const int split = (std::max)(bounds.left, bounds.right - strip);
                navigationArea = {split, bounds.top, bounds.right, bounds.bottom};
            }
        }
    }

    void updateButtonRects()
    {
        previousButton = {};
        nextButton = {};
        if (navigation.showSlideButton == DuiSwiperNavigationVisibility::Never || pages.empty())
            return;
        const DuiSwiperMetrics metric = metrics();
        const int half = metric.arrowExtent / 2;
        if (direction == DuiSwiperDirection::Horizontal)
        {
            const int centerY = viewport.top + viewport.Height() / 2;
            previousButton = {viewport.left + metric.arrowInset, centerY - half,
                              viewport.left + metric.arrowInset + metric.arrowExtent, centerY + half};
            nextButton = {viewport.right - metric.arrowInset - metric.arrowExtent, centerY - half,
                          viewport.right - metric.arrowInset, centerY + half};
        }
        else
        {
            const int centerX = viewport.left + viewport.Width() / 2;
            previousButton = {centerX - half, viewport.top + metric.arrowInset,
                              centerX + half, viewport.top + metric.arrowInset + metric.arrowExtent};
            nextButton = {centerX - half, viewport.bottom - metric.arrowInset - metric.arrowExtent,
                          centerX + half, viewport.bottom - metric.arrowInset};
        }
    }

    void updateIndicatorRects()
    {
        indicatorBounds.clear();
        const int count = pageCount();
        if (count <= 0 || navigation.indicatorType == DuiSwiperIndicatorType::Fraction)
            return;
        const DuiSwiperMetrics metric = metrics();
        const bool bar = navigation.indicatorType == DuiSwiperIndicatorType::DotsBar
            || navigation.indicatorType == DuiSwiperIndicatorType::Bars;
        const int slot = bar ? metric.barLength + metric.gap : 2 * metric.dotRadius + metric.gap;
        const int cross = navigation.indicatorType == DuiSwiperIndicatorType::Bars
            ? metric.barThickness
            : 2 * metric.dotRadius;
        indicatorBounds.reserve(static_cast<std::size_t>(count));
        if (direction == DuiSwiperDirection::Horizontal)
        {
            int left = navigationArea.left + (navigationArea.Width() - slot * count) / 2;
            const int top = navigationArea.top + (navigationArea.Height() - cross) / 2;
            for (int index = 0; index < count; ++index)
            {
                indicatorBounds.push_back({left, top, left + slot, top + cross});
                left += slot;
            }
        }
        else
        {
            int top = navigationArea.top + (navigationArea.Height() - slot * count) / 2;
            const int left = navigationArea.left + (navigationArea.Width() - cross) / 2;
            for (int index = 0; index < count; ++index)
            {
                indicatorBounds.push_back({left, top, left + cross, top + slot});
                top += slot;
            }
        }
    }

    /** 重新摆放所有页面，并同步可见性。 */
    void layoutPages()
    {
        const int count = pageCount();
        for (int index = 0; index < count; ++index)
            pages[index]->SetVisible(false);
        if (count == 0 || current < 0 || current >= count)
            return;
        for (int index = 0; index < count; ++index)
        {
            if (!pageVisible(index))
                continue;
            const bool faded = animation == DuiSwiperAnimation::Fade && animating();
            const int offset = faded ? 0 : mainOffset(pageDistance(index));
            pages[index]->SetVisible(true);
            pages[index]->Layout(offsetRect(viewport, offset));
        }
    }

    void scheduleAutoplay()
    {
        if (autoplayTask != 0 && clock != nullptr)
            clock->Cancel(autoplayTask);
        autoplayTask = {};
        if (!autoplay || clock == nullptr || pageCount() <= 1 || animating())
            return;
        // 悬停暂停：指针在区域内时不安排下一次自动播放
        if (stopOnHover && pointerInside)
            return;
        const std::weak_ptr<CallbackState> weak = callbackState;
        autoplayTask = clock->Schedule((std::max)(1, interval), [weak](double fraction)
        {
            if (fraction < 1.0)
                return;
            const std::shared_ptr<CallbackState> state = weak.lock();
            if (state == nullptr || state->owner == nullptr)
                return;
            Impl& self = *state->owner->swiper_;
            self.autoplayTask = {};
            self.advanceAutoplay();
        });
    }

    void advanceAutoplay()
    {
        if (owner == nullptr || pageCount() <= 1)
            return;
        int next = current + 1;
        if (next >= pageCount())
        {
            if (!loop)
                return;
            next = 0;
        }
        owner->SetCurrentIndex(next, true);
    }

    void scheduleTransition()
    {
        if (owner == nullptr)
            return;
        if (clock == nullptr || duration <= 0)
        {
            settle();
            return;
        }
        if (transitionTask != 0)
            clock->Cancel(transitionTask);
        const double startProgress = progress;
        const std::weak_ptr<CallbackState> weak = callbackState;
        transitionTask = clock->Schedule(duration, [weak, startProgress](double fraction)
        {
            const std::shared_ptr<CallbackState> state = weak.lock();
            if (state == nullptr || state->owner == nullptr)
                return;
            Impl& self = *state->owner->swiper_;
            self.progress = startProgress + (1.0 - startProgress) * EaseOutCubic(fraction);
            const bool finished = fraction >= 1.0;
            if (finished)
            {
                self.transitionTask = {};
                self.progress = 1.0;
                self.from = self.current;
            }
            state->owner->Layout(state->owner->Bounds());
            if (finished)
                self.scheduleAutoplay();
        });
    }

    void beginTransition(int target, int transitionStep, bool notify)
    {
        if (owner == nullptr)
            return;
        cancelTransition();
        from = current;
        current = target;
        step = transitionStep;
        progress = 0.0;
        owner->Layout(owner->Bounds());
        if (notify && changeHandler)
            changeHandler(current);
        if (clock != nullptr && duration > 0)
        {
            scheduleTransition();
            return;
        }
        settle();
    }

    void paintPage(render::Canvas& canvas, int index) const
    {
        const core::Rect pageBounds = pages[index]->Bounds();
        // 淡入模式下新旧页重叠，缩放会与溶解效果互相干扰，故不参与卡片缩放。
        const bool cardScaleActive = type == DuiSwiperType::Card
            && animation != DuiSwiperAnimation::Fade;
        const double scale = cardScaleActive ? cardScaleForDistance(pageDistance(index)) : 1.0;
        if (scale >= 1.0 - 1e-6)
        {
            PaintControl(*pages[index], canvas, pageBounds);
            return;
        }
        const core::Point center{(pageBounds.left + pageBounds.right) / 2,
                                 (pageBounds.top + pageBounds.bottom) / 2};
        render::DuiCanvasTransform transform;
        transform.scale = scale;
        transform.offset = {center.x - static_cast<int>(std::lround(center.x * scale)),
                            center.y - static_cast<int>(std::lround(center.y * scale))};
        canvas.PushTransform(transform);
        PaintControl(*pages[index], canvas, pageBounds);
        canvas.PopTransform();
    }

    /**
     * 绘制所有可见页面。
     * 按距离升序绘制：当前页先画，缩小的相邻页随后覆盖在视口边缘，
     * 保证卡片样式下相邻页能露出视口（否则会被全宽的当前页完全遮住）。
     */
    void paintPages(render::Canvas& canvas) const
    {
        if (current < 0 || current >= pageCount())
            return;
        std::vector<int> visible;
        for (int index = 0; index < pageCount(); ++index)
            if (pageVisible(index))
                visible.push_back(index);
        std::stable_sort(visible.begin(), visible.end(), [this](int left, int right)
        {
            return std::abs(pageDistance(left)) < std::abs(pageDistance(right));
        });
        for (const int index : visible)
            paintPage(canvas, index);
    }

    void paintIndicator(render::Canvas& canvas, int index) const
    {
        if (index < 0 || index >= static_cast<int>(indicatorBounds.size()))
            return;
        const core::Rect slot = indicatorBounds[index];
        const bool active = index == current;
        const core::Color brand = owner->Theme().Get(core::ThemeSlot::BrandPrimary);
        const core::Color muted = WithAlpha(owner->Theme().Get(core::ThemeSlot::BorderHeavy),
                                            INDICATOR_INACTIVE_ALPHA);
        const DuiSwiperMetrics metric = metrics();
        const auto centered = [](core::Rect slotRect, int width, int height)
        {
            const int left = slotRect.left + (slotRect.Width() - width) / 2;
            const int top = slotRect.top + (slotRect.Height() - height) / 2;
            return core::Rect{left, top, left + width, top + height};
        };
        switch (navigation.indicatorType)
        {
            case DuiSwiperIndicatorType::Bars:
            {
                const core::Rect bar = centered(slot, metric.barLength, metric.barThickness);
                canvas.FillRoundedRect(bar, metric.barThickness / 2, active ? brand : muted);
                break;
            }
            case DuiSwiperIndicatorType::DotsBar:
            {
                if (active)
                {
                    const core::Rect bar = centered(slot, metric.barLength, 2 * metric.dotRadius);
                    canvas.FillRoundedRect(bar, metric.dotRadius, brand);
                }
                else
                {
                    canvas.FillEllipse(centered(slot, 2 * metric.dotRadius, 2 * metric.dotRadius), muted);
                }
                break;
            }
            case DuiSwiperIndicatorType::Dots:
            case DuiSwiperIndicatorType::Fraction:
            default:
            {
                const int radius = active ? metric.dotRadius + 1 : metric.dotRadius;
                canvas.FillEllipse(centered(slot, 2 * radius, 2 * radius), active ? brand : muted);
                break;
            }
        }
    }

    void paintArrow(render::Canvas& canvas, core::Rect rect, bool forward, bool enabled) const
    {
        const core::Color surface = WithAlpha(owner->Theme().Get(core::ThemeSlot::SurfaceBackground),
                                              ARROW_SURFACE_ALPHA);
        const core::Color border = owner->Theme().Get(core::ThemeSlot::PanelBorder);
        const core::Color glyph = enabled ? owner->Theme().Get(core::ThemeSlot::TextDefault)
                                          : owner->Theme().Get(core::ThemeSlot::TextDisabled);
        const int radius = rect.Height() / 2;
        canvas.FillRoundedRect(rect, radius, surface);
        canvas.StrokeRoundedRect(rect, radius, border, 1.0f);
        const core::Point center{rect.left + rect.Width() / 2, rect.top + rect.Height() / 2};
        const int half = (std::max)(3, rect.Height() / 4);
        const int depth = (std::max)(2, rect.Height() / 6);
        render::DuiPath path;
        if (direction == DuiSwiperDirection::Horizontal)
        {
            if (forward)
            {
                path.MoveTo({center.x - depth / 2, center.y - half});
                path.LineTo({center.x + depth, center.y});
                path.LineTo({center.x - depth / 2, center.y + half});
            }
            else
            {
                path.MoveTo({center.x + depth / 2, center.y - half});
                path.LineTo({center.x - depth, center.y});
                path.LineTo({center.x + depth / 2, center.y + half});
            }
        }
        else if (forward)
        {
            path.MoveTo({center.x - half, center.y - depth / 2});
            path.LineTo({center.x, center.y + depth});
            path.LineTo({center.x + half, center.y - depth / 2});
        }
        else
        {
            path.MoveTo({center.x - half, center.y + depth / 2});
            path.LineTo({center.x, center.y - depth});
            path.LineTo({center.x + half, center.y + depth / 2});
        }
        path.Close();
        canvas.FillPath(path, glyph);
    }

    void paintNavigation(render::Canvas& canvas) const
    {
        const int count = pageCount();
        if (count <= 0)
            return;
        if (navigation.indicatorType == DuiSwiperIndicatorType::Fraction)
        {
            render::DuiTextStyle style;
            style.color = owner->Theme().Get(core::ThemeSlot::TextDefault);
            style.pointSize = navigation.size == DuiSwiperNavigationSize::Small ? 8
                : (navigation.size == DuiSwiperNavigationSize::Large ? 11 : 9);
            const std::string text = std::to_string(current + 1) + "/" + std::to_string(count);
            canvas.DrawText(text, navigationArea, style, render::DuiTextAlignment::Center, false);
        }
        else
        {
            for (int index = 0; index < static_cast<int>(indicatorBounds.size()); ++index)
                paintIndicator(canvas, index);
        }

        if (navigation.showSlideButton == DuiSwiperNavigationVisibility::Never)
            return;
        const bool hoverOnly = navigation.showSlideButton == DuiSwiperNavigationVisibility::Hover;
        if (hoverOnly && !pointerInside)
            return;
        const bool canPrevious = loop || current > 0;
        const bool canNext = loop || current < count - 1;
        paintArrow(canvas, previousButton, false, canPrevious);
        paintArrow(canvas, nextButton, true, canNext);
    }
};

DuiSwiper::DuiSwiper() : swiper_(std::make_unique<Impl>())
{
    swiper_->owner = this;
    swiper_->callbackState = std::make_shared<CallbackState>();
    swiper_->callbackState->owner = this;
}

DuiSwiper::~DuiSwiper()
{
    if (swiper_ == nullptr)
        return;
    swiper_->cancelTransition();
    swiper_->cancelAutoplay();
    swiper_->callbackState->owner = nullptr;
}

DuiSwiper::DuiSwiper(DuiSwiper&& other) noexcept
    : core::Control(std::move(other)), swiper_(std::move(other.swiper_))
{
    if (swiper_ != nullptr)
        swiper_->owner = this;
}

DuiSwiper& DuiSwiper::operator=(DuiSwiper&& other) noexcept
{
    if (this == &other)
        return *this;
    if (swiper_ != nullptr)
    {
        swiper_->cancelTransition();
        swiper_->cancelAutoplay();
        swiper_->callbackState->owner = nullptr;
    }
    core::Control::operator=(std::move(other));
    swiper_ = std::move(other.swiper_);
    if (swiper_ != nullptr)
        swiper_->owner = this;
    return *this;
}

void DuiSwiper::AddPage(std::unique_ptr<core::Control> page)
{
    if (page == nullptr)
        return;
    core::Control* raw = page.get();
    core::Control::AddChild(std::move(page));
    swiper_->pages.push_back(raw);
    if (swiper_->current < 0)
        swiper_->current = 0;
    Layout(Bounds());
    swiper_->scheduleAutoplay();
}

int DuiSwiper::PageCount() const
{
    return swiper_->pageCount();
}

void DuiSwiper::SetCurrentIndex(int index, bool notify)
{
    Impl& self = *swiper_;
    const int count = self.pageCount();
    if (index < 0 || index >= count)
        return;
    if (index == self.current)
    {
        // 目标是当前页：若正在过渡则立即停稳，否则无需处理
        if (!self.animating())
            return;
        self.cancelTransition();
        self.settle();
        return;
    }
    int delta = index - self.current;
    if (self.loop && count > 0)
    {
        delta = ((delta % count) + count) % count;
        if (delta > count / 2)
            delta -= count;
    }
    const int transitionStep = delta < 0 ? -1 : 1;
    self.beginTransition(index, transitionStep, notify);
}

int DuiSwiper::CurrentIndex() const
{
    return swiper_->current;
}

void DuiSwiper::Next()
{
    Impl& self = *swiper_;
    const int count = self.pageCount();
    if (count <= 1)
        return;
    int next = self.current + 1;
    if (next >= count)
    {
        if (!self.loop)
            return;
        next = 0;
    }
    SetCurrentIndex(next, true);
}

void DuiSwiper::Previous()
{
    Impl& self = *swiper_;
    const int count = self.pageCount();
    if (count <= 1)
        return;
    int previous = self.current - 1;
    if (previous < 0)
    {
        if (!self.loop)
            return;
        previous = count - 1;
    }
    SetCurrentIndex(previous, true);
}

void DuiSwiper::SetAnimation(DuiSwiperAnimation animation)
{
    swiper_->animation = animation;
    Layout(Bounds());
}

DuiSwiperAnimation DuiSwiper::Animation() const
{
    return swiper_->animation;
}

void DuiSwiper::SetDirection(DuiSwiperDirection direction)
{
    swiper_->direction = direction;
    Layout(Bounds());
}

DuiSwiperDirection DuiSwiper::Direction() const
{
    return swiper_->direction;
}

void DuiSwiper::SetType(DuiSwiperType type)
{
    swiper_->type = type;
    Layout(Bounds());
}

DuiSwiperType DuiSwiper::Type() const
{
    return swiper_->type;
}

void DuiSwiper::SetTrigger(DuiSwiperTrigger trigger)
{
    swiper_->trigger = trigger;
}

DuiSwiperTrigger DuiSwiper::Trigger() const
{
    return swiper_->trigger;
}

void DuiSwiper::SetCardScale(double scale)
{
    if (scale <= 0.0 || scale > 1.0)
        return;
    swiper_->cardScale = scale;
    Layout(Bounds());
}

double DuiSwiper::CardScale() const
{
    return swiper_->cardScale;
}

void DuiSwiper::SetNavigation(DuiSwiperNavigation navigation)
{
    swiper_->navigation = navigation;
    Layout(Bounds());
}

DuiSwiperNavigation DuiSwiper::Navigation() const
{
    return swiper_->navigation;
}

void DuiSwiper::SetAutoplay(bool autoplay)
{
    swiper_->autoplay = autoplay;
    swiper_->scheduleAutoplay();
}

bool DuiSwiper::Autoplay() const
{
    return swiper_->autoplay;
}

void DuiSwiper::SetInterval(int milliseconds)
{
    swiper_->interval = milliseconds;
    swiper_->scheduleAutoplay();
}

int DuiSwiper::Interval() const
{
    return swiper_->interval;
}

void DuiSwiper::SetDuration(int milliseconds)
{
    swiper_->duration = milliseconds;
}

int DuiSwiper::Duration() const
{
    return swiper_->duration;
}

void DuiSwiper::SetLoop(bool loop)
{
    swiper_->loop = loop;
    Layout(Bounds());
}

bool DuiSwiper::Loop() const
{
    return swiper_->loop;
}

void DuiSwiper::SetStopOnHover(bool stopOnHover)
{
    swiper_->stopOnHover = stopOnHover;
    swiper_->scheduleAutoplay();
}

bool DuiSwiper::StopOnHover() const
{
    return swiper_->stopOnHover;
}

void DuiSwiper::SetAnimationClock(core::AnimationClock* clock)
{
    Impl& self = *swiper_;
    self.cancelTransition();
    self.cancelAutoplay();
    self.clock = clock;
    // 若此前停在半途的过渡上，直接停稳到目标页，避免进度永远小于 1
    if (self.animating())
    {
        self.progress = 1.0;
        self.from = self.current;
        Layout(Bounds());
    }
    self.scheduleAutoplay();
}

void DuiSwiper::SetChangeHandler(std::function<void(int)> handler)
{
    swiper_->changeHandler = std::move(handler);
}

double DuiSwiper::TransitionProgress() const
{
    return swiper_->progress;
}

core::Rect DuiSwiper::PreviousButtonRect() const
{
    return swiper_->previousButton;
}

core::Rect DuiSwiper::NextButtonRect() const
{
    return swiper_->nextButton;
}

core::Rect DuiSwiper::IndicatorRect(int index) const
{
    if (index < 0 || index >= static_cast<int>(swiper_->indicatorBounds.size()))
        return {};
    return swiper_->indicatorBounds[index];
}

void DuiSwiper::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    Impl& self = *swiper_;
    self.updateAreas();
    self.updateButtonRects();
    self.updateIndicatorRects();
    self.layoutPages();
    self.laidOutBounds = bounds;
}

void DuiSwiper::EnsureLayout()
{
    if (swiper_->laidOutBounds != Bounds())
        Layout(Bounds());
}

core::Size DuiSwiper::DesiredSize() const
{
    return {DEFAULT_WIDTH, DEFAULT_HEIGHT};
}

core::Control* DuiSwiper::HitTest(core::Point point)
{
    if (!EffectivelyVisible() || !Enabled() || !Bounds().Contains(point))
        return nullptr;
    EnsureLayout();
    const Impl& self = *swiper_;
    if (self.previousButton.Contains(point) || self.nextButton.Contains(point))
        return this;
    for (const core::Rect& indicator : self.indicatorBounds)
    {
        if (indicator.Contains(point))
            return this;
    }
    if (self.current >= 0 && self.current < self.pageCount())
    {
        core::Control* page = self.pages[self.current];
        if (page->EffectivelyVisible() && page->Bounds().Contains(point))
        {
            if (core::Control* hit = page->HitTest(point))
                return hit;
        }
    }
    return this;
}

bool DuiSwiper::OnEvent(const core::Event& event)
{
    Impl& self = *swiper_;
    const int count = self.pageCount();
    switch (event.type)
    {
        case core::EventType::PointerMove:
        {
            const bool inside = Bounds().Contains(event.position);
            if (inside != self.pointerInside)
            {
                self.pointerInside = inside;
                if (self.stopOnHover)
                    self.scheduleAutoplay();
            }
            if (inside && self.trigger == DuiSwiperTrigger::Hover)
            {
                for (int index = 0; index < static_cast<int>(self.indicatorBounds.size()); ++index)
                {
                    if (self.indicatorBounds[index].Contains(event.position) && index != self.current)
                    {
                        SetCurrentIndex(index, true);
                        break;
                    }
                }
            }
            return false;
        }
        case core::EventType::PointerLeave:
        {
            if (self.pointerInside)
            {
                self.pointerInside = false;
                self.scheduleAutoplay();
            }
            return false;
        }
        case core::EventType::PointerDown:
        {
            if (self.previousButton.Contains(event.position)
                && (self.loop || self.current > 0))
            {
                Previous();
                return true;
            }
            if (self.nextButton.Contains(event.position)
                && (self.loop || self.current < count - 1))
            {
                Next();
                return true;
            }
            for (int index = 0; index < static_cast<int>(self.indicatorBounds.size()); ++index)
            {
                if (self.indicatorBounds[index].Contains(event.position))
                {
                    SetCurrentIndex(index, true);
                    return true;
                }
            }
            return false;
        }
        case core::EventType::KeyDown:
        {
            if (count <= 0)
                return false;
            if (self.direction == DuiSwiperDirection::Horizontal)
            {
                if (event.key == core::key::Left)
                {
                    Previous();
                    return true;
                }
                if (event.key == core::key::Right)
                {
                    Next();
                    return true;
                }
            }
            else
            {
                if (event.key == core::key::Up)
                {
                    Previous();
                    return true;
                }
                if (event.key == core::key::Down)
                {
                    Next();
                    return true;
                }
            }
            if (event.key == core::key::Home)
            {
                SetCurrentIndex(0, true);
                return true;
            }
            if (event.key == core::key::End)
            {
                SetCurrentIndex(count - 1, true);
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

void DuiSwiper::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const_cast<DuiSwiper*>(this)->EnsureLayout();
    const Impl& self = *swiper_;
    if (self.viewport.Empty() || self.current < 0 || self.current >= self.pageCount())
        return;
    if (core::Rect::Intersect(Bounds(), dirty).Empty())
        return;

    const core::Color surface = Theme().Get(core::ThemeSlot::SurfaceBackground);
    canvas.PushClip(self.viewport);
    if (self.animation == DuiSwiperAnimation::Fade && self.animating()
        && self.from >= 0 && self.from < self.pageCount() && self.from != self.current)
    {
        // 淡入淡出按“淡出→淡入”实现：先铺底色，再让当前阶段页面溶解回底色。
        canvas.FillRect(self.viewport, surface);
        if (self.progress < 0.5)
        {
            self.paintPage(canvas, self.from);
            canvas.FillRect(self.viewport, WithAlpha(surface,
                static_cast<int>(std::lround(self.progress * 2.0 * 255.0))));
        }
        else
        {
            self.paintPage(canvas, self.current);
            canvas.FillRect(self.viewport, WithAlpha(surface,
                static_cast<int>(std::lround((1.0 - (self.progress - 0.5) * 2.0) * 255.0))));
        }
    }
    else
    {
        canvas.FillRect(self.viewport, surface);
        self.paintPages(canvas);
    }
    canvas.PopClip();

    self.paintNavigation(canvas);
}

} // namespace ysDui::controls::layout
