#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::ui {
class DuiTextInput;
}

namespace ysDui::render {
class DuiImage;
}

namespace ysDui::controls::list {

enum class DuiTreeColumnKind
{
    Text,
    CheckBox,
    ProgressBar,
    Hyperlink,
    Icon,
    Image,
};

/** 节点复选框的聚合状态；父节点会根据子树实时计算 Mixed。 */
enum class DuiTreeCheckState
{
    Unchecked,
    Checked,
    Mixed,
};

/** 节点子项的惰性加载状态。 */
enum class DuiTreeLoadState
{
    NotRequested,
    Loading,
    Loaded,
    Failed,
};

/** 节点拖拽释放时相对目标节点的落点。 */
enum class DuiTreeDropPosition
{
    Before,
    Inside,
    After,
};

struct DuiTreeColumn final {
    std::string title;
    int width{120};
    int minimumWidth{40};
    render::DuiTextAlignment alignment{render::DuiTextAlignment::Start};
    bool sortable{true};
    bool editable{true};
    DuiTreeColumnKind kind{DuiTreeColumnKind::Text};
};

struct DuiTreeNode final {
    int id{-1};
    int parentId{-1};
    std::string label;
    std::string rightText;
    std::vector<std::string> cells;
    core::Color statusColor{};
    std::intptr_t value{};
    bool expanded{true};
    bool visible{true};
    bool selectable{true};
};

class DuiTreeView final : public core::Control, public render::DuiRenderable {
public:
    DuiTreeView();
    ~DuiTreeView() override;
    DuiTreeView(const DuiTreeView&) = delete;
    DuiTreeView& operator=(const DuiTreeView&) = delete;
    DuiTreeView(DuiTreeView&&) noexcept;
    DuiTreeView& operator=(DuiTreeView&&) noexcept;

    int AddRoot(std::string label, std::intptr_t value = 0);
    int AddChild(int parentId, std::string label, std::intptr_t value = 0);
    void Remove(int id);
    void Clear();
    [[nodiscard]] int RootCount() const;
    [[nodiscard]] int ChildCount(int parentId) const;
    /** @return 节点父 id；根节点或节点不存在返回 -1。 */
    [[nodiscard]] int ParentId(int id) const;
    [[nodiscard]] bool HasChildren(int id) const;

    void SetExpanded(int id, bool expanded);
    void ExpandAll();
    void CollapseAll();
    [[nodiscard]] bool Expanded(int id) const;
    [[nodiscard]] std::vector<int> ExpandedSnapshot() const;
    void RestoreExpanded(const std::vector<int>& ids);
    void SetItemVisible(int id, bool visible);
    void SetItemSelectable(int id, bool selectable);
    void SetFilter(std::function<bool(int)> filter);
    void ClearFilter();

    void SetLabel(int id, std::string label);
    [[nodiscard]] std::string Label(int id) const;
    /**
     * 设置单列树节点主标签下方的辅助文本。
     * @param id 节点标识。
     * @param subtitle UTF-8 副标题；空字符串恢复单行绘制。
     */
    void SetSubtitle(int id, std::string subtitle);
    /**
     * 读取节点副标题。
     * @param id 节点标识。
     * @return 节点不存在或未设置时返回空字符串。
     */
    [[nodiscard]] std::string Subtitle(int id) const;
    void SetRightText(int id, std::string text);
    void SetStatusColor(int id, core::Color color);
    void SetIcon(int id, std::shared_ptr<const render::DuiImage> icon);
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& IconAt(int id) const;
    /**
     * 设置节点图标是否以灰度形式绘制。
     * @param id 节点标识。
     * @param muted 为 true 时绘制缓存的灰度图像。
     */
    void SetIconMuted(int id, bool muted);
    /**
     * 查询节点图标是否为灰显状态。
     * @param id 节点标识。
     * @return 节点图标为灰显状态时返回 true。
     */
    [[nodiscard]] bool IconMuted(int id) const;
    void SetValue(int id, std::intptr_t value);
    [[nodiscard]] std::intptr_t Value(int id) const;

    int AddColumn(DuiTreeColumn column);
    [[nodiscard]] int ColumnCount() const;
    [[nodiscard]] DuiTreeColumn ColumnAt(int column) const;
    void SetColumnEditable(int column, bool editable);
    void SetSortIndicator(int column, int direction);
    [[nodiscard]] int SortColumn() const;
    [[nodiscard]] int SortDirection() const;
    void SetColumnClickedHandler(std::function<void(int, int)> handler);
    /** 设置节点右键菜单通知；菜单实际由宿主通过现有弹出菜单能力创建。 */
    void SetContextMenuHandler(std::function<void(int, core::Point)> handler);
    /** 注入平台输入代理；树节点内容仍由 TreeView 自绘。 */
    void SetTextInput(ui::DuiTextInput* textInput);
    /** 开始编辑节点标签或指定可编辑列。 */
    bool BeginEdit(int id, int column = 0);
    /** 提交当前编辑并触发编辑回调。 */
    void CommitEdit();
    /** 取消当前编辑，不改变节点文本。 */
    void CancelEdit();
    [[nodiscard]] bool Editing() const;
    void SetCellEditedHandler(std::function<void(int, int, std::string_view)> handler);
    void SetCellText(int id, int column, std::string text);
    [[nodiscard]] std::string CellText(int id, int column) const;
    void SetCellChecked(int id, int column, bool checked);
    [[nodiscard]] bool CellChecked(int id, int column) const;
    void SetCellCheckedChangedHandler(std::function<void(int, int, bool)> handler);
    void SetCellProgress(int id, int column, int progress);
    [[nodiscard]] int CellProgress(int id, int column) const;
    void SetCellProgressChangedHandler(std::function<void(int, int, int)> handler);
    void SetCellLink(int id, int column, std::string text, std::string url);
    [[nodiscard]] std::string CellLinkUrl(int id, int column) const;
    void SetLinkClickedHandler(std::function<void(int, int, std::string_view)> handler);
    void SetCellImage(int id, int column, std::shared_ptr<const render::DuiImage> image);
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& CellImageAt(int id, int column) const;
    /** 设置节点勾选状态，并递归应用到所有子节点。 */
    void SetNodeCheckState(int id, DuiTreeCheckState state, bool notify = true);
    [[nodiscard]] DuiTreeCheckState NodeCheckState(int id) const;
    void SetNodeChecksVisible(bool visible);
    [[nodiscard]] bool NodeChecksVisible() const;
    void SetNodeCheckChangedHandler(std::function<void(int, DuiTreeCheckState)> handler);
    void SetNodeLoadState(int id, DuiTreeLoadState state);
    [[nodiscard]] DuiTreeLoadState NodeLoadState(int id) const;
    /** 设置节点展开时触发的子项加载回调。回调完成后由宿主添加子节点并更新状态。 */
    void SetChildrenLoadRequestedHandler(std::function<void(int)> handler);
    /** 启用 TreeView 内部节点拖拽重排；默认关闭。 */
    void SetNodeDragEnabled(bool enabled);
    [[nodiscard]] bool NodeDragEnabled() const;
    /** 设置节点移动完成后的通知回调；回调执行时树结构已更新。 */
    void SetNodeDragHandler(std::function<void(const std::vector<int>&, int, DuiTreeDropPosition)> handler);
    [[nodiscard]] bool Dragging() const;
    [[nodiscard]] std::vector<int> DragSourceIds() const;
    [[nodiscard]] int DragTargetId() const;
    [[nodiscard]] DuiTreeDropPosition DragTargetPosition() const;
    void SetHeaderHeight(int pixels);
    [[nodiscard]] int HeaderHeight() const;
    void SetEditable(bool editable);
    [[nodiscard]] bool Editable() const;
    void SetZebra(bool enabled);
    [[nodiscard]] bool Zebra() const;
    /**
     * 设置是否绘制自身外框。
     * 用途：交给 `DuiScrollView` 等滚动容器时，内容比视口高，自绘外框的底边会被视口裁掉、
     *      右边又止于视口边界（框不住滚动条），此时由容器画外框，本控件关掉。
     */
    void SetBorderVisible(bool visible);
    [[nodiscard]] bool BorderVisible() const;
    /** 是否绘制鼠标悬停行背景；关闭后仅保留选中高亮 */
    void SetHoverHighlight(bool enabled);
    [[nodiscard]] bool HoverHighlight() const;
    /** 是否绘制选中行背景 */
    void SetSelectionHighlight(bool enabled);
    [[nodiscard]] bool SelectionHighlight() const;
    void SetFrozenColumns(int count);
    [[nodiscard]] int FrozenColumns() const;
    void SetFrozenRows(int count);
    [[nodiscard]] int FrozenRows() const;

    [[nodiscard]] int SelectedId() const;
    void SetSelectedId(int id, bool notify = true);
    /** 启用后支持 Ctrl 多选和 Shift 范围选择；默认关闭以保持单选行为。 */
    void SetMultiSelect(bool enabled);
    [[nodiscard]] bool MultiSelect() const;
    /** 查询节点是否位于当前选择集合中。 */
    [[nodiscard]] bool IsSelected(int id) const;
    /** 返回当前选择集合，顺序与用户选择顺序一致。 */
    [[nodiscard]] std::vector<int> SelectedIds() const;
    /** 清空选择；notify 为 true 时通过现有回调发送 -1。 */
    void ClearSelection(bool notify = true);
    void SetSelectionChangedHandler(std::function<void(int)> handler);
    /**
     * 设置节点激活回调（不可编辑时双击，或 Enter）。
     * @param handler 被激活的节点标识。
     */
    void SetItemActivatedHandler(std::function<void(int)> handler);
    /**
     * 设置指针进入或离开节点时的回调。
     * @param handler 节点标识；指针未位于节点上时传入 -1。
     */
    void SetHoverChangedHandler(std::function<void(int)> handler);
    /**
     * 获取当前悬停节点。
     * @return 当前节点标识；未悬停时返回 -1。
     */
    [[nodiscard]] int HoveredId() const;
    [[nodiscard]] int VisibleCount() const;
    [[nodiscard]] int IdAtVisibleRow(int row) const;
    [[nodiscard]] int VisibleRow(int id) const;
    [[nodiscard]] int ContentHeight() const;
    [[nodiscard]] int HitTestId(core::Point point) const;
    void SetRowHeight(int pixels);
    [[nodiscard]] int RowHeight() const;
    /**
     * 设置节点图标绘制边长（逻辑像素）。
     * @param pixels 边长；过小时按最小值。
     */
    void SetIconSize(int pixels);
    /** @return 节点图标绘制边长。 */
    [[nodiscard]] int IconSize() const;
    void SetIndent(int pixels);
    [[nodiscard]] int Indent() const;
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    void PlaceTextInput();
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;
    bool PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                    std::string_view value = {}) override;

    class Impl;
    std::unique_ptr<Impl> tree_;
};

} // namespace ysDui::controls::list
