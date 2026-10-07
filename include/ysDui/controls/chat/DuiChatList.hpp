/**
 * 文件名：DuiChatList.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：声明变高虚拟列表：按可见窗口惰性测量并布局条目，支持追加时贴底跟随。
 */
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::chat {

/**
 * 变高虚拟列表（对话消息流）。
 *
 * 解决的问题：消息高度由正文（`DuiMarkdownView` 等）在给定宽度下换行决定，无法预先知道；
 * 同时又不能为了知道高度而把所有历史消息都解析一遍。
 *
 * 做法：
 * - 条目以 `std::unique_ptr` 被本控件拥有，但**只有可见窗口（含预布局余量）内的条目会被 `Layout`**，
 *   窗口外的条目既不排版也不绘制——这是虚拟化收益的来源（未排版的 `DuiMarkdownView` 不会解析）。
 * - 未测量的条目按 `EstimatedItemHeight()` 估值参与偏移计算；测量过的条目用真实高度。
 *   估值是**固定值**（不随测量结果漂移），因此两次测量之间偏移保持稳定。
 * - 测量只发生在窗口内，窗口之上条目的偏移在同一帧内不变，所以测量不会把用户正在看的内容挤动
 *   （代价是：窗口外条目在测量前的滚动条长度是近似值，沿途测量后逐步收敛）。
 *
 * 使用约定：
 * - 追加消息用 `AppendItem`；流式更新后对**该条**调用 `InvalidateItem` 触发重新测量。
 * - 内容变化只影响该条自身高度，后续条目的偏移由本控件统一重算，无需调用方干预。
 * - 宽度变化会使全部测量失效并重新测量当前窗口。
 * - 贴底跟随：`SetFollowBottom(true)`；用户把视图移离末尾会自动关闭，回到末尾自动开启。
 *
 * 已知限制：窗口外条目的高度仍是估值，因此滚动条长度在沿途测量过程中会逐步收敛。
 */
class DuiChatList final : public core::Control, public render::DuiRenderable {
public:
    DuiChatList();
    ~DuiChatList() override;
    DuiChatList(const DuiChatList&) = delete;
    DuiChatList& operator=(const DuiChatList&) = delete;
    // 不提供移动：持有滚动条与条目的非拥有裸指针，移动收益低而风险高（与 DuiVirtualList 一致）
    DuiChatList(DuiChatList&&) = delete;
    DuiChatList& operator=(DuiChatList&&) = delete;

    // ---- 条目 ----

    /**
     * 设置条目总数：超出部分被移除，不足部分补空位。
     * 说明：这是**结构性装配**，会清除未读计数（与 `InsertItem` 同）；新增消息请用 `AppendItem`。
     * @param count 目标条目数
     */
    void SetItemCount(int count);
    /** @return 条目总数（含尚未设置控件的空位） */
    [[nodiscard]] int ItemCount() const;
    /**
     * 替换指定条目的控件。
     * @param index 条目下标；越界忽略
     * @param item 新控件（可为空表示占位）
     */
    void SetItem(int index, std::unique_ptr<core::Control> item);
    /** @return 条目控件；空位或越界返回空 */
    [[nodiscard]] core::Control* Item(int index) const;
    /**
     * 追加条目，返回新条目下标。
     *
     * 语义：**按"一条新消息到达"处理**——贴底跟随关闭时会计入 `UnseenCount()`。
     * 载入历史等结构性装配请用 `InsertItem` / `SetItemCount`（它们会清除未读计数）。
     * @param item 条目控件（可为空）
     * @return 新条目下标
     */
    int AppendItem(std::unique_ptr<core::Control> item);
    /**
     * 在指定位置插入条目（结构性装配）。
     *
     * 语义：**会清除未读计数**——插入通常意味着"重新装配内容"（载入历史、恢复会话）而非"新消息到达"。
     * 需要未读语义时请用 `AppendItem`。
     * @param index 插入位置；越界时追加到末尾
     * @param item 条目控件（可为空）
     */
    void InsertItem(int index, std::unique_ptr<core::Control> item);
    /**
     * 移除条目。
     * @param index 条目下标；越界忽略
     */
    void RemoveItem(int index);
    /** 移除全部条目（滚动条不受影响）。 */
    void ClearItems();

    // ---- 高度失效 ----

    /**
     * 标记某条目高度失效，下次布局重新测量。
     * 用途：流式追加内容后调用；只影响该条自身，后续条目偏移由本控件重算。
     * @param index 条目下标；越界忽略
     */
    void InvalidateItem(int index);
    /** 标记全部条目高度失效（宽度变化时内部自动调用）。 */
    void InvalidateAllItems();

    // ---- 滚动与贴底 ----

    /**
     * 设置滚动位置（内容坐标）。
     * @param pixels 目标位置，自动夹取到合法范围
     */
    void SetScrollOffset(int pixels);
    /** @return 当前滚动位置 */
    [[nodiscard]] int ScrollOffset() const;
    /** 滚到末尾，并把贴底跟随置为开启。 */
    void ScrollToBottom();
    /**
     * 设置贴底跟随：开启即滚到末尾，并在内容高度变化时保持贴底。
     * 视图被移离末尾时自动关闭，回到末尾时自动开启。
     * @param follow true 贴底
     */
    void SetFollowBottom(bool follow);
    /** @return 是否处于贴底跟随状态 */
    [[nodiscard]] bool FollowBottom() const;
    /** @return 是否已滚到末尾（无可滚动余量时视为已在末尾） */
    [[nodiscard]] bool AtBottom() const;
    /**
     * 滚动到使指定条目可见（必要时最小幅度滚动）。
     * @param index 条目下标；越界忽略
     */
    void EnsureItemVisible(int index);

    // ---- 未读条目 ----

    /**
     * @return 未读条目数，即"贴底跟随关闭期间追加的条目"数量。
     *
     * 为什么由列表负责：只有本控件知道"追加发生在用户没看末尾的时候"。
     * 外部无法从条目数变化推断——用户可能只是删了一条又加了一条。
     * 该值是**派生**的：已到末尾（或内容不足一屏）时恒为 0，因此手动滚到底、
     * `ScrollToBottom()`、`SetFollowBottom(true)`、窗口变大到内容全部可见，都会自动归零。
     */
    [[nodiscard]] int UnseenCount() const;
    /** 手动清除未读计数（不改变滚动位置）。 */
    void ClearUnseen();
    /**
     * 设置未读计数变化回调。
     *
     * 用途：驱动"回到底部 / N 条新消息"浮层——**浮层本身由页面组合**（`DuiButton` 叠在列表上），
     * 本控件只报告状态，不提供按钮。参考 Gallery 的 ChatStream 演示。
     *
     * 触发点：追加条目、滚动位置变化、清除计数。由于 `UnseenCount()` 是派生值，
     * 需要绝对精确的页面也可以在自己 `Layout()` 里直接读取它。
     * @param handler 回调，参数为新的未读计数；空回调表示取消
     */
    void SetUnseenChangedHandler(std::function<void(int)> handler);

    // ---- 观测 ----

    /** @return 全部条目的内容总高度（未测量条目按估值计入） */
    [[nodiscard]] int ContentHeight() const;
    /**
     * 条目在内容坐标中的矩形（不含滚动偏移）。
     * @param index 条目下标
     * @return 内容坐标矩形；越界返回空
     */
    [[nodiscard]] core::Rect ItemContentRect(int index) const;
    /**
     * 条目当前的屏幕矩形。
     * @param index 条目下标
     * @return 屏幕矩形；越界返回空
     */
    [[nodiscard]] core::Rect ItemRect(int index) const;
    /** @return 当前布局窗口内的首个条目下标；无条目时为 -1 */
    [[nodiscard]] int FirstVisibleIndex() const;
    /** @return 当前布局窗口内的末个条目下标；无条目时为 -1 */
    [[nodiscard]] int LastVisibleIndex() const;
    /**
     * 命中测试：返回指定屏幕点对应的条目下标。
     * @param point 屏幕坐标
     * @return 条目下标；不在条目上返回 -1
     */
    [[nodiscard]] int IndexFromPoint(core::Point point) const;
    /** @return 已用真实高度测量过的条目数（诊断与测试用） */
    [[nodiscard]] int MeasuredItemCount() const;

    // ---- 外观与参数 ----

    /**
     * 设置未测量条目的高度估值。
     * 说明：该值是固定常量，不随测量结果漂移；设置后全部偏移立即按新估值重算。
     * @param pixels 估算高度；至少 1
     */
    void SetEstimatedItemHeight(int pixels);
    /** @return 未测量条目的高度估值 */
    [[nodiscard]] int EstimatedItemHeight() const;
    /**
     * 设置预布局余量。
     * 说明：窗口上下各多排版该高度的条目，减少快速滚动时的空白；越大开销越高。
     * @param pixels 余量像素
     */
    void SetOverscan(int pixels);
    /** @return 预布局余量 */
    [[nodiscard]] int Overscan() const;
    /** 设置条目之间的竖直间距。 */
    void SetItemSpacing(int pixels);
    /** @return 条目之间的竖直间距 */
    [[nodiscard]] int ItemSpacing() const;
    /** 设置列表底色；不设置时取主题 ListBackground。 */
    void SetBackgroundColor(core::Color color);
    /** @return 列表底色 */
    [[nodiscard]] core::Color BackgroundColor() const;
    /** 设置滚动行步长（一次滚轮/方向键的像素数）；不设置时取估值。 */
    void SetScrollLineSize(int pixels);
    /** @return 滚动行步长 */
    [[nodiscard]] int ScrollLineSize() const;

    void Layout(core::Rect bounds) override;
    [[nodiscard]] core::Control* HitTest(core::Point point) override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    /** 重建各条目偏移与内容总高度（未测量条目按估值计入）。 */
    void RebuildOffsets() const;
    /** 测量窗口内尚未测量或已失效的条目。@return 本轮是否发生了测量 */
    bool MeasureWindow();
    /** 依据当前滚动位置计算需要布局的条目区间。 */
    void ComputeWindow() const;
    /** 求出内容坐标落在指定 y 处的条目下标。@return 下标；越界返回 -1 */
    [[nodiscard]] int IndexFromContentY(int contentY) const;
    /** @return 最大滚动位置（内容高度 - 视口高度，下限 0） */
    [[nodiscard]] int MaxOffset() const;
    /** 夹取滚动位置到合法范围。 */
    void ClampScroll();
    /**
     * 比较派生的未读值与上次已上报的值，不同则触发回调。
     * 只在非布局期调用（`Layout` 末尾除外，见实现注释）。
     */
    void SyncUnseen();
    /**
     * 在指定位置插入条目。
     * @param index 插入位置（已夹取）
     * @param item 条目控件（可为空）
     * @param countsAsUnread true 表示按"新消息到达"处理（`AppendItem` 路径）
     */
    void InsertAt(int index, std::unique_ptr<core::Control> item, bool countsAsUnread);
    /** 迭代测量与定位，直至偏移稳定（含贴底处理）。 */
    void RunLayoutPasses();
    /** 按最终偏移摆放窗口内条目的几何。 */
    void PlaceWindow();
    /** 同步滚动条的范围、页长、显隐与位置。 */
    void UpdateScrollBar();

    class Impl;
    std::unique_ptr<Impl> list_;
};

} // namespace ysDui::controls::chat
