/**
 * 文件名：DuiSwiper.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：声明轮播容器控件——多页共享一个视口，支持滑动/淡入切换、自动播放、卡片样式与导航指示器。
 */
#pragma once

#include <functional>
#include <memory>

#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::layout {

/** 页面切换动画。 */
enum class DuiSwiperAnimation
{
    Slide, // 滑动：新旧页面沿主轴平移，最常用的切换方式
    Fade,  // 淡入淡出：新旧页面重叠，经背景色过渡（画布无分组透明度，不做真正的交叉溶解）
};

/** 轮播主轴方向。 */
enum class DuiSwiperDirection
{
    Horizontal, // 水平：页面左右排列，翻页按钮分列两侧
    Vertical,   // 垂直：页面上下排列，翻页按钮分列上下
};

/** 轮播样式。 */
enum class DuiSwiperType
{
    Default, // 默认：当前页占满视口，其余页完全隐藏
    Card,    // 卡片：当前页居中，相邻页按 CardScale 缩小并分列主轴两侧
};

/** 导航指示器的切换触发方式。 */
enum class DuiSwiperTrigger
{
    Click, // 点击指示器后切换
    Hover, // 指针悬停指示器即切换
};

/** 导航指示器样式。 */
enum class DuiSwiperIndicatorType
{
    Dots,     // 圆点：全部为等大圆点，当前项高亮
    DotsBar,  // 圆点条：非当前项为圆点，当前项拉长为圆角条
    Bars,     // 条状：全部为等宽细条，当前项高亮
    Fraction, // 分数：显示“当前页/总页数”文本
};

/** 指示器与翻页按钮的摆放位置。 */
enum class DuiSwiperPlacement
{
    Inside,  // 叠加在页面之上
    Outside, // 占用视口外的独立区域（水平在底部，垂直在右侧）
};

/** 翻页按钮显隐策略。 */
enum class DuiSwiperNavigationVisibility
{
    Always, // 始终显示
    Hover,  // 仅指针位于轮播区域时显示
    Never,  // 不显示
};

/** 导航器尺寸档位。 */
enum class DuiSwiperNavigationSize
{
    Small,  // 紧凑：小圆点与窄条
    Medium, // 默认档位
    Large,  // 宽松：大圆点与宽条
};

/** 导航器完整配置，对应参考文档的 navigation 属性。 */
struct DuiSwiperNavigation final
{
    DuiSwiperPlacement placement{DuiSwiperPlacement::Inside}; // 指示器/按钮的摆放位置
    DuiSwiperNavigationVisibility showSlideButton{DuiSwiperNavigationVisibility::Always}; // 翻页按钮显隐
    DuiSwiperNavigationSize size{DuiSwiperNavigationSize::Medium}; // 指示器与按钮尺寸档位
    DuiSwiperIndicatorType indicatorType{DuiSwiperIndicatorType::Dots}; // 指示器样式
};

/**
 * 轮播容器。
 * 用途：把若干页面控件放入同一个视口，按固定间隔或用户操作切换当前页。
 * 页面与视口的关系由 `Layout` 决定；切换动画需要宿主通过 `SetAnimationClock` 提供动画时钟，
 * 未提供时钟时切换立即完成（适合无宿主的静态渲染与单元测试）。
 */
class DuiSwiper final : public core::Control, public render::DuiRenderable
{
public:
    DuiSwiper();
    ~DuiSwiper() override;
    DuiSwiper(const DuiSwiper&) = delete;
    DuiSwiper& operator=(const DuiSwiper&) = delete;
    DuiSwiper(DuiSwiper&&) noexcept;
    DuiSwiper& operator=(DuiSwiper&&) noexcept;

    /** 追加一页；第一页自动成为当前页。 */
    void AddPage(std::unique_ptr<core::Control> page);
    /** @return 页面数量。 */
    [[nodiscard]] int PageCount() const;

    /**
     * 切换到指定页。
     * @param index 目标页下标；越界时忽略。
     * @param notify true 时在页切换时触发 `SetChangeHandler` 注册的回调。
     */
    void SetCurrentIndex(int index, bool notify = false);
    /** @return 当前（或过渡目标）页下标；无页面时为 -1。 */
    [[nodiscard]] int CurrentIndex() const;
    /** 前进一页；循环开启时从末页回到首页。 */
    void Next();
    /** 后退一页；循环开启时从首页回到末页。 */
    void Previous();

    void SetAnimation(DuiSwiperAnimation animation);
    [[nodiscard]] DuiSwiperAnimation Animation() const;
    void SetDirection(DuiSwiperDirection direction);
    [[nodiscard]] DuiSwiperDirection Direction() const;
    void SetType(DuiSwiperType type);
    [[nodiscard]] DuiSwiperType Type() const;
    void SetTrigger(DuiSwiperTrigger trigger);
    [[nodiscard]] DuiSwiperTrigger Trigger() const;
    /** 设置卡片样式下相邻页的缩放比例；取值范围 (0, 1]。 */
    void SetCardScale(double scale);
    [[nodiscard]] double CardScale() const;
    void SetNavigation(DuiSwiperNavigation navigation);
    [[nodiscard]] DuiSwiperNavigation Navigation() const;

    void SetAutoplay(bool autoplay);
    [[nodiscard]] bool Autoplay() const;
    /** 设置自动播放间隔（毫秒）。 */
    void SetInterval(int milliseconds);
    [[nodiscard]] int Interval() const;
    /** 设置单次切换动画时长（毫秒）；<= 0 表示立即切换。 */
    void SetDuration(int milliseconds);
    [[nodiscard]] int Duration() const;
    void SetLoop(bool loop);
    [[nodiscard]] bool Loop() const;
    void SetStopOnHover(bool stopOnHover);
    [[nodiscard]] bool StopOnHover() const;

    /** 注入动画时钟；传 nullptr 会取消进行中的过渡与自动播放。 */
    void SetAnimationClock(core::AnimationClock* clock);
    /** 注册页切换回调，参数为目标页下标。 */
    void SetChangeHandler(std::function<void(int)> handler);
    /** @return 当前过渡进度：1 表示已停稳，0 表示刚进入过渡。 */
    [[nodiscard]] double TransitionProgress() const;

    /** @return 后退按钮矩形；不可显示时为空矩形。 */
    [[nodiscard]] core::Rect PreviousButtonRect() const;
    /** @return 前进按钮矩形；不可显示时为空矩形。 */
    [[nodiscard]] core::Rect NextButtonRect() const;
    /** @return 指定指示器的槽位矩形；分数样式或越界时为空矩形。 */
    [[nodiscard]] core::Rect IndicatorRect(int index) const;

    void Layout(core::Rect bounds);
    [[nodiscard]] core::Size DesiredSize() const override;
    [[nodiscard]] core::Control* HitTest(core::Point point) override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    /** 若当前边界与上次布局不一致则重新布局，兼容只用 SetBounds 摆放子控件的容器。 */
    void EnsureLayout();

    class Impl;
    std::unique_ptr<Impl> swiper_;
};

} // namespace ysDui::controls::layout
