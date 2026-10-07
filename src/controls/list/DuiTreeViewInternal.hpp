/**
 * 文件名：DuiTreeViewInternal.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：DuiTreeView 的私有实现细节（布局常量与 Impl 状态），
 *       供拆分后的多个 DuiTreeView*.cpp 实现文件共享。
 *       本头位于 src/ 下，不属于公开 API，不随 SDK 安装。
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ysDui/controls/list/DuiTreeView.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/ui/DuiTextInput.hpp"

namespace ysDui::controls::list {

/**
 * TreeView 布局与交互常量。
 * 集中在 detail 命名空间：这些名字（如 TextPadding、MinimumRowHeight）与同模块其他
 * 控件源码的匿名命名空间常量重名，若直接置于 list 命名空间并具外部链接，
 * 会使那些 TU 的无限定查找产生歧义。放在 detail 下并要求显式限定即可避免。
 */
namespace detail {
constexpr int MinimumRowHeight = 12; // 行高下限
constexpr int MinimumIndent = 8;     // 缩进下限
constexpr int GlyphWidth = 12;       // 展开/折叠三角所占宽度
constexpr int DefaultIconSize = 18;  // 默认节点图标边长
constexpr int MinimumIconSize = 8;   // 节点图标最小边长
constexpr int TextPadding = 6;       // 文字与单元格边缘的间距
constexpr int MinimumColumnWidth = 40; // 列宽下限
constexpr int DragThreshold = 4;     // 触发拖拽的位移阈值
} // namespace detail

class DuiTreeView::Impl {
public:
    std::vector<DuiTreeNode> nodes;
    std::unordered_map<int, std::shared_ptr<const render::DuiImage>> icons;
    std::unordered_map<int, std::string> subtitles;
    std::unordered_set<std::uint64_t> checkedCells;
    std::unordered_map<std::uint64_t, int> cellProgress;
    std::unordered_map<std::uint64_t, std::string> cellLinks;
    std::unordered_map<std::uint64_t, std::shared_ptr<const render::DuiImage>> cellImages;
    std::unordered_map<int, std::shared_ptr<const render::DuiImage>> mutedIcons;
    std::unordered_set<int> mutedIconIds;
    std::unordered_set<int> statusColorIds;
    std::vector<DuiTreeColumn> columns;
    std::vector<int> visibleIds;
    std::vector<int> selectedIds;
    std::unordered_map<int, DuiTreeCheckState> nodeChecks;
    std::unordered_map<int, DuiTreeLoadState> loadStates;
    std::function<bool(int)> filter;
    std::function<void(int)> selectionChanged;
    std::function<void(int)> itemActivated;
    std::function<void(int)> hoverChanged;
    std::function<void(int, int, std::string_view)> linkClicked;
    std::function<void(int, int)> columnClicked;
    std::function<void(int, int, bool)> cellCheckedChanged;
    std::function<void(int, int, int)> cellProgressChanged;
    std::function<void(int, DuiTreeCheckState)> nodeCheckChanged;
    std::function<void(int)> childrenLoadRequested;
    std::function<void(const std::vector<int>&, int, DuiTreeDropPosition)> nodeDragHandler;
    std::function<void(int, core::Point)> contextMenu;
    std::function<void(int, int, std::string_view)> cellEdited;
    ui::DuiTextInput* textInput{};
    std::shared_ptr<int> textInputLifetime;
    std::weak_ptr<void> textInputToken;
    int editId{-1};
    int editColumn{-1};
    int nextId{1};
    int selectedId{-1};
    int selectionAnchorId{-1};
    int hoveredId{-1};
    int rowHeight{28};
    int iconSize{detail::DefaultIconSize};
    int indent{18};
    int headerHeight{26};
    int sortColumn{-1};
    int sortDirection{};
    int frozenColumns{};
    int frozenRows{};
    bool editable{};
    bool zebra{};
    bool borderVisible{true}; // 是否绘制自身外框；由滚动容器承载时关闭，避免裁剩半截
    bool hoverHighlight{true}; // 是否绘制悬停行背景
    bool selectionHighlight{true}; // 是否绘制选中行背景
    bool multiSelect{};
    bool nodeChecksVisible{};
    bool nodeDragEnabled{};
    bool dragCandidate{};
    bool dragging{};
    core::Point dragStart{};
    int dragPressedId{-1};
    std::vector<int> dragSourceIds;
    int dragTargetId{-1};
    DuiTreeDropPosition dragTargetPosition{DuiTreeDropPosition::Inside};
    bool dragTargetValid{};
    render::DuiTextStyle textStyle{{34, 34, 40, 255}, {}, 9, false};
    render::DuiTextStyle subtitleStyle{{110, 110, 120, 255}, {}, 8, false};

    [[nodiscard]] DuiTreeNode* Find(int id)
    {
        for (auto& node : nodes)
            if (node.id == id) return &node;
        return nullptr;
    }

    [[nodiscard]] const DuiTreeNode* Find(int id) const
    {
        for (const auto& node : nodes)
            if (node.id == id) return &node;
        return nullptr;
    }

    [[nodiscard]] bool MatchesFilter(const DuiTreeNode& node) const
    {
        return node.visible && (!filter || filter(node.id));
    }

    [[nodiscard]] bool FilterMatchesSubtree(int id) const
    {
        for (const auto& node : nodes)
        {
            if (node.parentId != id)
                continue;
            if (MatchesFilter(node) || FilterMatchesSubtree(node.id))
                return true;
        }
        return false;
    }

    [[nodiscard]] bool Visible(const DuiTreeNode& node) const
    {
        return node.visible && (!filter || MatchesFilter(node) || FilterMatchesSubtree(node.id));
    }

    [[nodiscard]] bool HasChildren(int id) const
    {
        for (const auto& node : nodes)
            if (node.parentId == id) return true;
        return false;
    }

    [[nodiscard]] int Depth(int id) const
    {
        int depth{};
        const DuiTreeNode* node = Find(id);
        while (node && node->parentId >= 0) {
            ++depth;
            node = Find(node->parentId);
        }
        return depth;
    }

    void AppendVisible(int parentId, bool ancestorsExpanded)
    {
        for (const auto& node : nodes) {
            if (node.parentId != parentId)
                continue;
            const bool nodeVisible = ancestorsExpanded && Visible(node);
            if (nodeVisible)
                visibleIds.push_back(node.id);
            AppendVisible(node.id, nodeVisible && (node.expanded || filter));
        }
    }

    void RebuildVisible()
    {
        visibleIds.clear();
        AppendVisible(-1, true);
        selectedIds.erase(std::remove_if(selectedIds.begin(), selectedIds.end(), [this](int id)
        {
            return std::find(visibleIds.begin(), visibleIds.end(), id) == visibleIds.end();
        }), selectedIds.end());
        selectedId = selectedIds.empty() ? -1 : selectedIds.back();
    }

    void SetSelection(int id, bool selected)
    {
        const auto it = std::find(selectedIds.begin(), selectedIds.end(), id);
        if (selected && it == selectedIds.end()) selectedIds.push_back(id);
        else if (!selected && it != selectedIds.end()) selectedIds.erase(it);
        selectedId = selectedIds.empty() ? -1 : selectedIds.back();
    }

    [[nodiscard]] bool IsDescendant(const DuiTreeNode& node, int ancestor) const
    {
        int parent = node.parentId;
        while (parent >= 0) {
            if (parent == ancestor) return true;
            const DuiTreeNode* parentNode = Find(parent);
            parent = parentNode ? parentNode->parentId : -1;
        }
        return false;
    }

    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& RenderIconAt(int id) const
    {
        static const std::shared_ptr<const render::DuiImage> empty;
        if (mutedIconIds.find(id) != mutedIconIds.end()) {
            const auto muted = mutedIcons.find(id);
            return muted == mutedIcons.end() ? empty : muted->second;
        }
        const auto icon = icons.find(id);
        return icon == icons.end() ? empty : icon->second;
    }

    [[nodiscard]] static std::uint64_t CellKey(int id, int column)
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(id)) << 32U)
            | static_cast<std::uint32_t>(column);
    }

    [[nodiscard]] static int CellNodeId(std::uint64_t key)
    {
        return static_cast<int>(key >> 32U);
    }

    [[nodiscard]] DuiTreeCheckState CheckState(int id) const
    {
        const auto it = nodeChecks.find(id);
        if (it != nodeChecks.end()) return it->second;
        bool hasChild{};
        bool allChecked{true};
        bool anyChecked{};
        for (const auto& node : nodes) {
            if (node.parentId != id) continue;
            hasChild = true;
            const auto state = CheckState(node.id);
            allChecked = allChecked && state == DuiTreeCheckState::Checked;
            anyChecked = anyChecked || state != DuiTreeCheckState::Unchecked;
        }
        if (hasChild) return allChecked ? DuiTreeCheckState::Checked
                                        : anyChecked ? DuiTreeCheckState::Mixed : DuiTreeCheckState::Unchecked;
        return DuiTreeCheckState::Unchecked;
    }

    void SetCheckRecursive(int id, DuiTreeCheckState state)
    {
        nodeChecks[id] = state;
        for (const auto& node : nodes)
            if (node.parentId == id) SetCheckRecursive(node.id, state);
    }

    void ReorderChildren(int parentId, int column, int direction)
    {
        std::vector<std::size_t> indices;
        std::vector<DuiTreeNode> siblings;
        for (std::size_t index{}; index < nodes.size(); ++index) {
            if (nodes[index].parentId != parentId) continue;
            indices.push_back(index);
            siblings.push_back(nodes[index]);
        }
        const auto valueAt = [column](const DuiTreeNode& node) -> std::string_view {
            if (column == 0) return node.label;
            return column < static_cast<int>(node.cells.size()) ? node.cells[static_cast<std::size_t>(column)] : std::string_view{};
        };
        std::stable_sort(siblings.begin(), siblings.end(), [valueAt, direction](const auto& left, const auto& right) {
            const std::string_view leftText = valueAt(left);
            const std::string_view rightText = valueAt(right);
            if (leftText == rightText) return left.id < right.id;
            return direction > 0 ? leftText < rightText : leftText > rightText;
        });
        for (std::size_t index{}; index < indices.size(); ++index) nodes[indices[index]] = std::move(siblings[index]);
        for (const auto& node : nodes) if (node.parentId == parentId) ReorderChildren(node.id, column, direction);
    }

    void ClearDragState()
    {
        dragCandidate = false;
        dragging = false;
        dragPressedId = -1;
        dragSourceIds.clear();
        dragTargetId = -1;
        dragTargetValid = false;
        dragTargetPosition = DuiTreeDropPosition::Inside;
    }

    [[nodiscard]] bool IsDescendantId(int id, int ancestor) const
    {
        const auto* node = Find(id);
        while (node && node->parentId >= 0) {
            if (node->parentId == ancestor) return true;
            node = Find(node->parentId);
        }
        return false;
    }

    [[nodiscard]] std::vector<int> DragRoots(int pressedId) const
    {
        std::vector<int> result = selectedIds;
        if (std::find(result.begin(), result.end(), pressedId) == result.end())
            result = {pressedId};
        result.erase(std::remove_if(result.begin(), result.end(), [this](int id) { return Find(id) == nullptr; }), result.end());
        std::vector<int> roots;
        for (const int id : result)
        {
            const bool hasSelectedAncestor = std::any_of(result.begin(), result.end(), [this, id](int other)
            {
                return other != id && IsDescendantId(id, other);
            });
            if (!hasSelectedAncestor) roots.push_back(id);
        }
        return roots;
    }

    [[nodiscard]] bool ValidDrop(int targetId, DuiTreeDropPosition position) const
    {
        const auto* target = Find(targetId);
        if (targetId < 0 || !target || !target->selectable
            || std::find(visibleIds.begin(), visibleIds.end(), targetId) == visibleIds.end()
            || dragSourceIds.empty()) return false;
        for (const int sourceId : dragSourceIds) {
            if (sourceId == targetId || IsDescendantId(targetId, sourceId)) return false;
        }
        if (position == DuiTreeDropPosition::Inside)
            return true;
        if (std::find(dragSourceIds.begin(), dragSourceIds.end(), targetId) != dragSourceIds.end()) return false;
        return true;
    }

    bool MoveDraggedNodes(int targetId, DuiTreeDropPosition position)
    {
        if (!ValidDrop(targetId, position)) return false;
        std::unordered_map<int, std::vector<int>> children;
        std::unordered_map<int, DuiTreeNode> nodeById;
        for (const auto& node : nodes) {
            children[node.parentId].push_back(node.id);
            nodeById.emplace(node.id, node);
        }
        const auto originalChildren = children;
        const auto originalParents = [&nodeById]()
        {
            std::unordered_map<int, int> parents;
            for (const auto& [id, node] : nodeById) parents.emplace(id, node.parentId);
            return parents;
        }();
        std::vector<int> moveOrder = dragSourceIds;
        std::stable_sort(moveOrder.begin(), moveOrder.end(), [this](int left, int right)
        {
            const auto leftIt = std::find_if(nodes.begin(), nodes.end(), [left](const DuiTreeNode& node) { return node.id == left; });
            const auto rightIt = std::find_if(nodes.begin(), nodes.end(), [right](const DuiTreeNode& node) { return node.id == right; });
            return leftIt < rightIt;
        });
        for (const int sourceId : moveOrder) {
            auto& siblings = children[Find(sourceId)->parentId];
            siblings.erase(std::remove(siblings.begin(), siblings.end(), sourceId), siblings.end());
        }
        const auto* target = Find(targetId);
        const int destinationParent = position == DuiTreeDropPosition::Inside ? targetId : target->parentId;
        auto& destination = children[destinationParent];
        std::size_t insertion = position == DuiTreeDropPosition::Inside ? destination.size() :
            static_cast<std::size_t>(std::distance(destination.begin(), std::find(destination.begin(), destination.end(), targetId))
                + (position == DuiTreeDropPosition::After ? 1 : 0));
        insertion = (std::min)(insertion, destination.size());
        destination.insert(destination.begin() + static_cast<std::ptrdiff_t>(insertion), moveOrder.begin(), moveOrder.end());
        for (const int sourceId : moveOrder)
            nodeById[sourceId].parentId = destinationParent;
        bool changed = children != originalChildren;
        for (const auto& [id, node] : nodeById)
            changed = changed || node.parentId != originalParents.at(id);
        if (!changed) return false;
        std::vector<DuiTreeNode> ordered;
        std::unordered_set<int> visited;
        const auto append = [&](auto&& self, int parentId) -> void
        {
            const auto childrenIt = children.find(parentId);
            if (childrenIt == children.end()) return;
            for (const int id : childrenIt->second) {
                if (!visited.insert(id).second) continue;
                ordered.push_back(nodeById.at(id));
                self(self, id);
            }
        };
        append(append, -1);
        for (const auto& node : nodes) {
            if (visited.insert(node.id).second)
                ordered.push_back(nodeById.at(node.id));
        }
        nodes = std::move(ordered);
        RebuildVisible();
        return true;
    }
};

} // namespace ysDui::controls::list
