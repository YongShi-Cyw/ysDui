/**
 * 文件名：DuiToastCenter.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明消息堆叠管理器——从边缘依次堆叠多条 DuiToast，支持分组去重与重复计数。
 */
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "ysDui/controls/basic/DuiToast.hpp"
#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"

namespace ysDui::controls::basic {

/** 单条消息的配置。 */
struct DuiToastOptions final
{
    DuiToastType type{DuiToastType::Information};
    DuiToastAppearance appearance{DuiToastAppearance::Light};
    /** 停留时长；0 表示不自动关闭。 */
    int durationMilliseconds{3000};
    bool showClose{};
    /** 自定义图标；空则使用类型默认字形。 */
    std::shared_ptr<const render::DuiImage> icon;
    /** 固定底色；未设置时按外观自动配色（浅色按类型混出，深色用主题默认）。 */
    std::optional<core::Color> background;
};

/**
 * 消息堆叠管理器。
 * 对应 Element Plus Message 的容器语义：多条消息从顶边（或底边）依次堆叠，
 * 每条按前一条的高度顺延。单个消息的渲染完全复用 DuiToast。
 *
 * 与直接使用 DuiToast 的差别：
 * - 自动堆叠，调用方只需 Show，不必自己算偏移；
 * - 分组去重：相同文字的消息合并为一条并累加重复计数（对应 grouping / repeatNum）；
 * - 生命周期自管理：消息结束后自动移除，无需调用方干预。
 *
 * 使用约束：
 * - 本控件在 `HitTest` 中只认领各消息的关闭按钮，其余位置继续穿透，故不会遮挡下层控件；
 * - 但绘制顺序仍是插入序，请把本控件作为父容器的**最后一个子节点**加入，否则消息可能被其他控件覆盖。
 */
class DuiToastCenter final : public core::Control, public render::DuiRenderable {
public:
    DuiToastCenter();
    ~DuiToastCenter() override;
    DuiToastCenter(const DuiToastCenter&) = delete;
    DuiToastCenter& operator=(const DuiToastCenter&) = delete;
    DuiToastCenter(DuiToastCenter&&) = delete;
    DuiToastCenter& operator=(DuiToastCenter&&) = delete;

    /** 注入动画时钟；未注入时消息不会自动消失。 */
    void SetAnimationClock(core::AnimationClock* clock);
    /** 设置堆叠方向；Bottom 时从可用区域底边向上堆叠。 */
    void SetPlacement(DuiToastPlacement placement);
    [[nodiscard]] DuiToastPlacement Placement() const;
    /** 设置首条消息距停靠边的偏移。 */
    void SetEdgeOffset(int pixels);
    [[nodiscard]] int EdgeOffset() const;
    /** 设置相邻消息的间距。 */
    void SetGap(int pixels);
    [[nodiscard]] int Gap() const;
    void SetMaxWidth(int pixels);
    [[nodiscard]] int MaxWidth() const;
    void SetTextMeasurer(render::DuiTextMeasurer* measurer);
    /** 启用后，文字相同的消息合并为一条并累加重复计数。 */
    void SetGrouping(bool enabled);
    [[nodiscard]] bool Grouping() const;

    /**
     * 显示一条消息。
     * @param message UTF-8 文本；为空时忽略。
     * @param options 本条消息的类型、外观、时长等配置。
     * @return 是否产生了新的可见消息（分组命中时返回 false，表示复用已有条目）。
     */
    bool Show(std::string message, DuiToastOptions options = {});
    /** 立即关闭并移除全部消息。 */
    void Clear();
    /**
     * 清理已结束的消息。
     * 由 Layout/Show 自动调用；消息若在动画回调中结束，会在下一次安全时机被移除，
     * 不在回调栈内销毁控件，避免释放后回调访问。
     */
    void Purge();

    [[nodiscard]] int Count() const;
    /** @return 第 index 条消息文本；越界返回空串。 */
    [[nodiscard]] const std::string& MessageAt(int index) const;
    /** @return 第 index 条消息的重复计数；越界返回 0。 */
    [[nodiscard]] int RepeatNumAt(int index) const;
    /** @return 第 index 条消息的重复计数徽标矩形；计数不大于 1 或越界时为空。 */
    [[nodiscard]] core::Rect BadgeRectAt(int index) const;

    void Layout(core::Rect availableBounds);
    /** @return 覆盖层无固有尺寸，恒为 {0, 0}。 */
    [[nodiscard]] core::Size DesiredSize() const override;
    /** 仅认领各消息的关闭按钮，其余位置穿透给下层控件。 */
    [[nodiscard]] core::Control* HitTest(core::Point point) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> center_;

    /** 标记某条消息已结束，等待 Purge 在回调之外移除。 */
    void retire(const DuiToast* toast);
};

} // namespace ysDui::controls::basic
