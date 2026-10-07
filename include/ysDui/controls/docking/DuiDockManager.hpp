/**
 * 文件名：DuiDockManager.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：停靠管理器：把 DuiDockTree 布局映射为可停靠、可拖拽分栏、可切换标签的控件树。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/controls/docking/DuiDockTree.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::ui {
class IUiHostFactory;
}

namespace ysDui::controls::docking {

namespace ui = ysDui::ui;

/**
 * 停靠管理器。
 *
 * 布局由 `DuiDockTree` 描述，视图侧直接复用既有控件：`Split` 映射为一个
 * `layout::DuiSplitter`（可拖拽改变比例），`Group` 映射为一个 `list::DuiTabPage`
 * （标签条 + 关闭 + 重排 + 溢出滚动）。窗格内容由本控件持有并挂到对应标签页上。
 *
 * 结构变更（增删窗格、左右切分）会重建控件树；纯尺寸变更（拖拽分隔条、切换标签）
 * 只回写到模型，不重建，避免丢失拖拽状态。
 */
class DuiDockManager final : public core::Control, public render::DuiRenderable
{
public:
    DuiDockManager();
    ~DuiDockManager() override;
    DuiDockManager(const DuiDockManager&) = delete;
    DuiDockManager& operator=(const DuiDockManager&) = delete;
    DuiDockManager(DuiDockManager&&) noexcept;
    DuiDockManager& operator=(DuiDockManager&&) noexcept;

    /**
     * 追加窗格到指定槽位。
     * @param id 窗格标识，须唯一且非空
     * @param title 标签文字
     * @param content 窗格内容；失败时不接管（随参数释放）
     * @param slot 目标槽位
     * @param icon 标签与悬浮窗标题栏的前置图标；空指针表示无图标
     * @return 成功返回 true
     */
    bool AddPane(std::string id, std::string title, std::unique_ptr<core::Control> content,
                 DuiDockSlot slot = DuiDockSlot::Center,
                 std::shared_ptr<const render::DuiImage> icon = {});

    /**
     * 在指定窗格旁切出新组并放入新窗格，用于构造嵌套分割。
     * @param fraction 新分割的切分比例；`slot` 为 `Center`（并入参照组）时忽略
     * @return 成功返回 true
     */
    bool AddPaneBeside(std::string_view anchorPaneId, std::string id, std::string title,
                       std::unique_ptr<core::Control> content, DuiDockSlot slot,
                       double fraction = 0.5);

    /**
     * 关闭并移除窗格；空组被回收、仅剩单个子节点的分割被折叠。
     * @return 窗格存在并移除成功返回 true
     */
    bool RemovePane(std::string_view id);

    /**
     * 移动窗格到目标窗格处。
     * @param slot `Center` 表示并入目标组成为标签，其余表示在目标组旁切出新组
     * @return 成功返回 true
     */
    bool MovePane(std::string_view id, std::string_view targetId,
                  DuiDockSlot slot = DuiDockSlot::Center, double fraction = 0.5);

    /**
     * 修改窗格标题。
     * @return 窗格存在返回 true
     */
    bool SetPaneTitle(std::string_view id, std::string title);

    /**
     * 设置窗格是否显示关闭按钮。
     * @return 窗格存在返回 true
     */
    bool SetPaneCloseable(std::string_view id, bool closeable);

    /**
     * 设置窗格的前置图标（标签页与悬浮窗标题栏共用同一张图）。
     * @param id 窗格标识
     * @param icon 图标；空指针表示清除
     * @return 窗格存在返回 true
     */
    bool SetPaneIcon(std::string_view id, std::shared_ptr<const render::DuiImage> icon);

    /** @return 是否存在该标识的窗格。 */
    [[nodiscard]] bool Contains(std::string_view id) const;
    /** @return 窗格总数。 */
    [[nodiscard]] int PaneCount() const;
    /** @return 全部窗格标识，按深度优先顺序。 */
    [[nodiscard]] std::vector<std::string> PaneIds() const;
    /** @return 当前活动窗格标识；无窗格时为空串。 */
    [[nodiscard]] std::string ActivePane() const;
    /** @return 窗格内容控件；不存在时为空指针。 */
    [[nodiscard]] core::Control* PaneContent(std::string_view id) const;

    /**
     * 选中指定窗格所在标签。
     * @return 窗格存在返回 true
     */
    bool SetActivePane(std::string_view id);

    /**
     * 序列化当前布局（含分隔条比例）。
     * @param text 输出 UTF-8 XML 文本
     * @return 成功返回 true
     */
    [[nodiscard]] bool SaveLayout(std::string& text) const;

    /**
     * 按给定布局重排当前窗格集合；未出现在布局中的窗格会被并入第一个组。
     * @return 解析并应用成功返回 true
     */
    bool LoadLayout(std::string_view text);

    /** 设置窗格被关闭（用户点关闭按钮或调用 `RemovePane`）后的通知。 */
    void SetPaneClosedHandler(std::function<void(std::string)> handler);
    /** 设置活动窗格变化通知。 */
    void SetActivePaneChangedHandler(std::function<void(std::string)> handler);
    /** 设置窗格被拖出为悬浮窗口后的通知。 */
    void SetPaneFloatedHandler(std::function<void(std::string)> handler);

    /**
     * 设置 UI 宿主工厂。
     * 用途：把窗格拖出为独立顶层悬浮窗口；未设置时拖出操作不生效。
     */
    void SetHostFactory(ui::IUiHostFactory* factory);

    /**
     * 把窗格拖出为独立悬浮窗口。
     * 窗格随即离开停靠区（含窄条）；该窗口关闭时窗格被关闭并触发 `SetPaneClosedHandler`。
     * 可用 `DockFloatingPane` 把内容收回停靠区（不触发关闭通知）。
     * @return 已设置宿主工厂且窗格存在返回 true
     */
    bool FloatPane(std::string_view id);
    /** @return 该窗格是否已拖出为悬浮窗口。 */
    [[nodiscard]] bool IsPaneFloating(std::string_view id) const;
    /**
     * 把悬浮窗口里的窗格收回停靠区。
     * 内容通过 `IFrameHost::DetachContent` 取回，并并入拖出前所在的组（该组已消失时回中央组）；
     * 收回不算关闭，不触发 `SetPaneClosedHandler`。
     * @return 收回成功返回 true；窗格未悬浮或后端不支持摘出内容时返回 false
     */
    bool DockFloatingPane(std::string_view id);
    /** @return 已拖出为悬浮窗口的窗格标识。 */
    [[nodiscard]] std::vector<std::string> FloatingPaneIds() const;

    /**
     * 设置窗格是否收起为边缘窄条；指针悬停窄条条目时临时滑出内容。
     * @param slot 收起时贴附的边；展开时忽略
     * @return 窗格存在且状态有变化返回 true
     */
    bool SetPaneAutoHide(std::string_view id, bool autoHide, DuiDockSlot slot = DuiDockSlot::Left);
    /** @return 是否收起到边缘窄条。 */
    [[nodiscard]] bool IsPaneAutoHidden(std::string_view id) const;
    /** 设置边缘窄条厚度。 */
    void SetAutoHideStripThickness(int pixels);

    /** 设置标签宽度范围，用于组内标签过多时的溢出滚动。 */
    void SetTabWidthRange(int minimum, int maximum);

    /** @return 是否正在拖动窗格。 */
    [[nodiscard]] bool Dragging() const;
    /** @return 拖动过程中命中的放置方式；未拖动时为空串。 */
    [[nodiscard]] std::string DropHint() const;

    void Layout(core::Rect bounds);
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;
    core::Control* HitTest(core::Point point) override;
    bool OnEvent(const core::Event& event) override;
    /** @return 只读布局模型，供外部查询结构。 */
    [[nodiscard]] const DuiDockTree& Tree() const;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    /**
     * 落地挂起的结构重建。
     * 标签关闭与标签重排由 `DuiTab` 的自身回调触发，此刻不能销毁正在执行的那个标签页，
     * 因此回调内只改模型并置挂起标记，由本函数在回调栈退出后的安全点执行重建。
     */
    void FlushRebuild();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::controls::docking
