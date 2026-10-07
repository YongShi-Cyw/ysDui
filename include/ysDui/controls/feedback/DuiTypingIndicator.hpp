/**
 * 文件名：DuiTypingIndicator.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：声明"正在输入"指示器：若干点按序脉冲，用于等待首个回答片段的场合。
 */
#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::core {
class AnimationClock;
}

namespace ysDui::controls::feedback {

/**
 * 正在输入指示器。
 * 说明：只表达"对方正在生成"，不承载文本；需要气泡外观时把它放进 `DuiChatBubble` 的内容里。
 * 动画由 `core::AnimationClock` 驱动，未设置时钟时静态显示（三点同亮度）。
 */
class DuiTypingIndicator final : public core::Control, public render::DuiRenderable {
public:
    DuiTypingIndicator();
    ~DuiTypingIndicator() override;
    DuiTypingIndicator(const DuiTypingIndicator&) = delete;
    DuiTypingIndicator& operator=(const DuiTypingIndicator&) = delete;
    DuiTypingIndicator(DuiTypingIndicator&&) noexcept;
    DuiTypingIndicator& operator=(DuiTypingIndicator&&) noexcept;

    /**
     * 设置动画时钟。
     * @param clock 宿主时钟；空指针表示停止动画（静态显示）
     */
    void SetAnimationClock(core::AnimationClock* clock);
    /**
     * 启停脉冲动画；停用时三点恢复同亮度。
     * @param active true 播放
     */
    void SetActive(bool active);
    /** @return 是否正在播放动画 */
    [[nodiscard]] bool Active() const;
    /**
     * 设置动画相位，便于测试与外部同步。
     * @param milliseconds 周期内毫秒数（周期 1200ms，超出按周期取模）
     */
    void SetPhase(int milliseconds);
    /** @return 当前相位（毫秒） */
    [[nodiscard]] int Phase() const;
    /**
     * 设置点的数量。
     * @param count 点数；至少 1
     */
    void SetDotCount(int count);
    /** @return 点数量 */
    [[nodiscard]] int DotCount() const;
    /** 设置单个点的半径。 */
    void SetDotRadius(int pixels);
    /** @return 点的半径 */
    [[nodiscard]] int DotRadius() const;
    /** 设置点之间的间距。 */
    void SetGap(int pixels);
    /** @return 点之间的间距 */
    [[nodiscard]] int Gap() const;
    /** 设置点颜色；不设置时取主题的次要文本色。 */
    void SetColor(core::Color color);
    /** @return 点颜色（显式设置或主题值） */
    [[nodiscard]] core::Color Color() const;

    /**
     * 计算首选尺寸。
     * @return 包住全部点的最小尺寸
     */
    [[nodiscard]] core::Size DesiredSize() const override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    /** 按当前相位推进动画并在周期结束时续排下一次。 */
    void ScheduleNextTick();
    /** @return 指定点在当前相位下的亮度系数（0.3–1.0）。 */
    [[nodiscard]] double DotPulse(int index) const;

    class Impl;
    std::unique_ptr<Impl> typing_;
};

} // namespace ysDui::controls::feedback
