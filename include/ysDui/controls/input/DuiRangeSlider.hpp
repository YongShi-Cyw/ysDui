/**
 * 文件名：DuiRangeSlider.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明双端范围滑块——用一个轨道同时表达下界与上界，用于区间筛选。
 */
#pragma once

#include <functional>
#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

/** 当前被拖动/命中的拇指。 */
enum class DuiRangeThumb {
    None, // 未命中任何拇指
    Low,  // 下界拇指
    High, // 上界拇指
};

/**
 * 双端范围滑块。
 * 与 DuiSlider 的区别：DuiSlider 是单值；本控件维护 [low, high] 区间并对 low <= high 做钳制。
 * 两个拇指可相互靠拢但不交叉，最近按下者决定拖动目标。
 */
class DuiRangeSlider final : public core::Control, public render::DuiRenderable {
public:
    DuiRangeSlider();
    ~DuiRangeSlider() override;
    DuiRangeSlider(const DuiRangeSlider&) = delete;
    DuiRangeSlider& operator=(const DuiRangeSlider&) = delete;
    DuiRangeSlider(DuiRangeSlider&&) noexcept;
    DuiRangeSlider& operator=(DuiRangeSlider&&) noexcept;

    /** 设置取值范围；minimum > maximum 时自动交换。区间端点会被钳制到新范围。 */
    void SetRange(int minimum, int maximum);
    [[nodiscard]] int Minimum() const;
    [[nodiscard]] int Maximum() const;

    /**
     * 设置区间。
     * @param low 下界；越界时钳制到 [minimum, maximum]。
     * @param high 上界；越界时钳制；并保证 low <= high（传入 low > high 时交换）。
     * @param notify 是否触发值变更回调。
     */
    void SetValues(int low, int high, bool notify = false);
    [[nodiscard]] int Low() const;
    [[nodiscard]] int High() const;

    /** 设置键盘/滚轮单步量。 */
    void SetLineSize(int value);
    [[nodiscard]] int LineSize() const;
    /** 设置纵向后两个拇指改为上下分布。 */
    void SetVertical(bool vertical);
    [[nodiscard]] bool Vertical() const;
    void SetValuesChangedHandler(std::function<void(int, int)> handler);

    /** @return 轨道矩形（不含拇指半径）。 */
    [[nodiscard]] core::Rect TrackRect() const;
    /** @return 指定拇指的中心点。 */
    [[nodiscard]] core::Point ThumbCenter(DuiRangeThumb thumb) const;
    /** @return 当前被按住的拇指。 */
    [[nodiscard]] DuiRangeThumb ActiveThumb() const;
    /** @return 坐标对应的取值（已钳制到范围）。 */
    [[nodiscard]] int ValueFromPoint(core::Point point) const;
    /** @return 坐标命中哪个拇指；超出拇指半径时返回 None。 */
    [[nodiscard]] DuiRangeThumb ThumbFromPoint(core::Point point) const;

    /** @return 双端滑块的首选尺寸。 */
    [[nodiscard]] core::Size DesiredSize() const override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    /** @return 有效（非空）的轨道内区间长度。 */
    [[nodiscard]] int TrackSpan() const;

    class Impl;
    std::unique_ptr<Impl> range_;
};

} // namespace ysDui::controls::input
