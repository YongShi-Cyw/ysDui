#include "ysDui/controls/graph/DuiNodeEditor.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "DuiNodeEditorInternal.hpp"
#include "../input/DuiTextInputPointer.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::graph {
using namespace detail;
namespace {

/** @return 事件是否带指定修饰键。 */
bool HasModifier(const core::Event& event, unsigned int modifier)
{
    return (event.modifiers & modifier) != 0U;
}

/** 删除 UTF-8 字符串最后一个码点。 */
void PopUtf8(std::string& text)
{
    if (text.empty())
        return;
    std::size_t index = text.size() - 1;
    while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80)
        --index;
    text.erase(index);
}

} // namespace

bool DuiNodeEditor::OnEvent(const core::Event& event)
{
    Impl& state = *editor_;
    const core::Rect bounds = Bounds();
    if (!Enabled() || bounds.Empty())
        return false;

    // 悬停状态：除按键与取消外都重算，保证高亮与实际命中一致
    if (event.type != core::EventType::KeyDown && event.type != core::EventType::KeyUp
        && event.type != core::EventType::PointerCancel
        && event.type != core::EventType::PointerLeave)
    {
        const DuiNodeId previousHover = state.hovered.node;
        state.hovered = state.HitTest(event.position);
        if (previousHover != state.hovered.node)
            state.PlaceEmbeddedControls();
    }

    if (state.renamingNode.Valid() && event.type == core::EventType::TextInput)
    {
        if (state.renameInput == nullptr)
            state.HandleRenameText(event.text);
        return true;
    }
    if (state.renamingNode.Valid() && event.type == core::EventType::PointerDown)
        state.FinishRename(true);

    if (state.renamingNode.Valid() && state.renameInput != nullptr)
    {
        if (ysDui::controls::detail::HandleTextInputPointer(
                *this, state.renameInput, state.RenameScreenRect(), event, state.renameSelecting,
                core::DuiPointerCursor::Arrow))
            return true;
    }

    // 内嵌控件：只路由给当前命中的顶层节点，避免下层控件抢走上层节点的点击。
    // 已捕获的控件（拖选文本、按住箭头）仍继续收事件。中/右键留给画布。
    const bool viewOrMenuButton = event.type == core::EventType::PointerDown
        && (event.button == core::PointerButton::Middle
            || event.button == core::PointerButton::Secondary);
    if (!viewOrMenuButton && state.EmbeddedVisible() && !state.hovered.onMinimap)
    {
        const auto routeWidget = [&](core::Control* child) -> bool
        {
            if (child == nullptr || !child->EffectivelyVisible())
                return false;
            if (!child->Bounds().Contains(event.position) && !child->Captured())
                return false;
            return child->OnEvent(event);
        };
        const auto routeNodeWidgets = [&](DuiNodeId nodeId) -> bool
        {
            for (const auto& entry : state.pinContents)
            {
                const DuiGraphPin* pin = state.graph.FindPin(entry.first);
                if (pin != nullptr && pin->node == nodeId && routeWidget(entry.second))
                    return true;
            }
            for (const auto& entry : state.contents)
            {
                if (entry.first == nodeId && routeWidget(entry.second))
                    return true;
            }
            return false;
        };
        for (const auto& entry : state.pinContents)
        {
            if (entry.second != nullptr && entry.second->Captured() && routeWidget(entry.second))
                return true;
        }
        for (const auto& entry : state.contents)
        {
            if (entry.second != nullptr && entry.second->Captured() && routeWidget(entry.second))
                return true;
        }
        if (!state.hovered.pin.Valid() && state.hovered.node.Valid()
            && routeNodeWidgets(state.hovered.node))
            return true;
    }

    // ---------------------------------------------------------- 滚轮缩放
    if (event.type == core::EventType::PointerWheel)
    {
        if (!bounds.Contains(event.position))
            return false;
        state.autoFit = false;
        const double factor = event.wheelDelta > 0 ? state.style.zoomStep
                                                   : 1.0 / state.style.zoomStep;
        SetZoom(state.transform.scale * factor, event.position);
        return true;
    }

    // ---------------------------------------------------------- 右键节点菜单 / 中右键平移
    if (event.type == core::EventType::PointerDown
        && (event.button == core::PointerButton::Middle
            || event.button == core::PointerButton::Secondary))
    {
        if (event.button == core::PointerButton::Secondary
            && (state.hovered.node.Valid() || state.hovered.group.Valid()
                || state.hovered.link.Valid()))
        {
            if (state.hovered.node.Valid() || state.hovered.group.Valid())
            {
                const DuiNodeId target = state.hovered.node.Valid() ? state.hovered.node
                                                                    : state.hovered.group;
                if (!state.IsNodeSelected(target))
                    state.SelectNode(target, false);
            }
            if (state.hovered.link.Valid() && !state.IsLinkSelected(state.hovered.link))
                state.SelectLink(state.hovered.link, false);
            if (state.selectionDirty && state.selectionChanged)
            {
                state.selectionDirty = false;
                state.selectionChanged();
            }
            (void)state.ShowContextMenu(event.position);
            return true;
        }
        state.autoFit = false;
        state.interaction = Interaction::Panning;
        state.dragStartScreen = event.position;
        state.panStartOffset = state.transform.offset;
        SetCaptured(true);
        return true;
    }

    // ---------------------------------------------------------- 按下
    if (event.type == core::EventType::PointerDown
        && event.button == core::PointerButton::Primary)
    {
        if (state.hovered.onMinimap)
        {
            state.autoFit = false;
            state.interaction = Interaction::NavigatingMinimap;
            state.PanFromMinimap(event.position);
            SetCaptured(true);
            return true;
        }
        // 引脚：开始拉线（只读模式除外）
        if (state.hovered.pin.Valid() && !state.readOnly)
        {
            state.interaction = Interaction::CreatingLink;
            state.linkFrom = state.hovered.pin;
            state.linkCursorCanvas = state.transform.ToCanvas(event.position);
            state.linkCandidate = {};
            state.linkCandidateValid = false;
            state.RefreshCompatiblePins();
            SetCaptured(true);
            return true;
        }
        // 分组右下角：调整尺寸
        if (state.hovered.onGroupResize && state.hovered.group.Valid() && !state.readOnly)
        {
            state.interaction = Interaction::ResizingGroup;
            state.resizingGroup = state.hovered.group;
            state.dragStartScreen = event.position;
            state.dragHistoryPending = true; // 首次位移时再 Commit，纯点击不占撤销栈
            SetCaptured(true);
            return true;
        }
        // 节点或分组：拖动；未选中的先选中。已在多选中的节点上按下时保留选择，便于批量拖拽
        if (state.hovered.node.Valid() || state.hovered.group.Valid())
        {
            const DuiNodeId target = state.hovered.node.Valid() ? state.hovered.node
                                                                : state.hovered.group;
            const bool shift = HasModifier(event, core::modifier::Shift);
            const bool control = HasModifier(event, core::modifier::Control);
            const bool append = shift || control;
            const bool alreadySelected = state.IsNodeSelected(target);
            state.pendingToggle = {};
            state.dragDuplicate = false;
            if (!alreadySelected)
            {
                state.SelectNode(target, append);
            }
            else if (control && !shift)
            {
                // Ctrl+点击已选中：无拖动则抬起时取消选中；有拖动则复制
                state.pendingToggle = target;
            }
            // Shift+已选中 或 普通已选中：保持多选，进入批量拖动
            if (state.selectionDirty && state.selectionChanged)
            {
                state.selectionDirty = false;
                state.selectionChanged();
            }
            if (!state.readOnly && state.IsNodeSelected(target))
            {
                const DuiGraphNode* node = state.graph.FindNode(target);
                const bool dragAsGroup = node != nullptr && node->kind == DuiNodeKind::Group
                    && state.selectedNodes.size() == 1;
                state.interaction = dragAsGroup ? Interaction::DraggingGroup
                                                : Interaction::DraggingNodes;
                state.dragStartScreen = event.position;
                state.dragLastCanvas = state.transform.ToCanvas(event.position);
                state.dragHistoryPending = true;
                state.dragDuplicate = control && !state.readOnly;
                SetCaptured(true);
            }
            return true;
        }
        // 连线：选中
        if (state.hovered.link.Valid())
        {
            state.SelectLink(state.hovered.link,
                             HasModifier(event, core::modifier::Control)
                                 || HasModifier(event, core::modifier::Shift));
            if (state.selectionChanged)
                state.selectionChanged();
            return true;
        }
        // 空白：开始框选；Shift 追加，不先清空
        if (!HasModifier(event, core::modifier::Shift))
            state.ClearSelection();
        state.interaction = Interaction::BoxSelecting;
        state.dragStartScreen = event.position;
        state.selectionBox = {event.position.x, event.position.y, event.position.x, event.position.y};
        SetCaptured(true);
        if (state.selectionChanged)
            state.selectionChanged();
        return true;
    }

    // ---------------------------------------------------------- 移动
    if (event.type == core::EventType::PointerMove)
    {
        switch (state.interaction)
        {
        case Interaction::Panning:
            state.transform.offset = {state.panStartOffset.x + (event.position.x - state.dragStartScreen.x),
                                      state.panStartOffset.y + (event.position.y - state.dragStartScreen.y)};
            state.PlaceEmbeddedControls();
            return true;
        case Interaction::NavigatingMinimap:
            state.PanFromMinimap(event.position);
            return true;
        case Interaction::BoxSelecting:
            state.selectionBox = {state.dragStartScreen.x, state.dragStartScreen.y,
                                  event.position.x, event.position.y};
            return true;
        case Interaction::DraggingNodes:
        case Interaction::DraggingGroup:
        {
            const core::Point canvas = state.transform.ToCanvas(event.position);
            core::Point delta{canvas.x - state.dragLastCanvas.x, canvas.y - state.dragLastCanvas.y};
            if (delta.x == 0 && delta.y == 0)
                return true;
            if (state.snapToGrid)
            {
                const int grid = state.style.gridSize;
                const core::Point anchor = state.transform.ToCanvas(state.dragStartScreen);
                const core::Point snapped =
                    {static_cast<int>(std::lround(static_cast<double>(canvas.x) / grid)) * grid,
                     static_cast<int>(std::lround(static_cast<double>(canvas.y) / grid)) * grid};
                delta = {snapped.x - state.dragLastCanvas.x, snapped.y - state.dragLastCanvas.y};
                (void)anchor;
            }
            if (delta.x == 0 && delta.y == 0)
                return true;
            state.pendingToggle = {};
            if (state.dragDuplicate)
            {
                if (state.DuplicateSelectionForDrag())
                    state.dragHistoryPending = false;
                state.dragDuplicate = false;
            }
            // 首次实际位移前记录坐标，使 Ctrl+Z 能还原拖动前的位置且不整图拷贝
            if (state.dragHistoryPending)
            {
                state.dragPositionsBefore = state.CapturePositions(state.selectedNodes);
                state.dragHistoryPending = false;
            }
            // 拖动全部选中节点；若被拖的节点属于某分组，则整组一起移动
            const std::vector<DuiNodeId> moving = state.selectedNodes;
            for (const DuiNodeId node : moving)
            {
                DuiGraphNode* entry = state.graph.FindNode(node);
                if (entry == nullptr)
                    continue;
                if (entry->kind == DuiNodeKind::Group)
                    continue; // 分组在下面按 MoveGroup 统一处理
                entry->position.x += delta.x;
                entry->position.y += delta.y;
            }
            for (const DuiNodeId node : moving)
            {
                const DuiGraphNode* entry = state.graph.FindNode(node);
                if (entry != nullptr && entry->kind == DuiNodeKind::Group)
                    (void)state.graph.MoveGroup(node, delta);
            }
            state.dragLastCanvas = canvas;
            std::vector<DuiNodeId> moved = moving;
            for (const DuiNodeId node : moving)
            {
                const DuiGraphNode* entry = state.graph.FindNode(node);
                if (entry == nullptr || entry->kind != DuiNodeKind::Group)
                    continue;
                for (const auto& candidate : state.graph.Nodes())
                {
                    if (state.graph.NodeBelongsTo(candidate.id, node))
                        moved.push_back(candidate.id);
                }
            }
            state.TranslateLayouts(moved, delta);
            return true;
        }
        case Interaction::ResizingGroup:
        {
            DuiGraphNode* group = state.graph.FindNode(state.resizingGroup);
            if (group == nullptr)
                return true;
            const core::Point canvas = state.transform.ToCanvas(event.position);
            const core::Size next{canvas.x - group->position.x, canvas.y - group->position.y};
            if (next.width == group->size.width && next.height == group->size.height)
                return true;
            if (state.dragHistoryPending)
            {
                state.dragPositionsBefore = {{state.resizingGroup, group->position}};
                state.dragSizesBefore = {{state.resizingGroup, group->size}};
                state.dragHistoryPending = false;
            }
            state.graph.SetGroupSize(state.resizingGroup, next);
            state.LayoutGraph();
            return true;
        }
        case Interaction::CreatingLink:
        {
            state.linkCursorCanvas = state.transform.ToCanvas(event.position);
            // 已命中引脚优先；否则吸附最近可连接引脚
            const DuiPinId target = state.ResolveLinkTarget(event.position, state.linkFrom);
            state.linkCandidate = target;
            state.linkCandidateValid = target.Valid()
                && state.CanCreateLink(state.linkFrom, target);
            if (target.Valid() && !state.linkCandidateValid)
            {
                // 允许反向拖拽（从输入引脚拉到输出引脚）时互换两端
                state.linkCandidateValid = state.CanCreateLink(target, state.linkFrom);
            }
            return true;
        }
        case Interaction::None:
        default:
            return state.hovered.pin.Valid() || state.hovered.node.Valid()
                || state.hovered.link.Valid() || state.hovered.group.Valid();
        }
    }

    // ---------------------------------------------------------- 抬起
    if (event.type == core::EventType::PointerUp)
    {
        if (state.interaction == Interaction::CreatingLink)
        {
            const DuiPinId target = state.ResolveLinkTarget(event.position, state.linkFrom);
            if (target.Valid() && !state.readOnly)
            {
                DuiPinId start = state.linkFrom;
                DuiPinId end = target;
                bool legal = state.CanCreateLink(start, end);
                if (!legal && state.CanCreateLink(target, state.linkFrom))
                {
                    start = target;
                    end = state.linkFrom;
                    legal = true;
                }
                if (legal)
                    (void)state.CommitNewLink(start, end);
            }
            else if (!state.readOnly && HasModifier(event, core::modifier::Alt)
                     && state.linkFrom.Valid())
            {
                // Alt + 空白松手：断开该引脚上的全部连线（常见蓝图手感）
                const std::vector<DuiLinkId> links = state.graph.LinksOfPin(state.linkFrom);
                if (!links.empty())
                    (void)state.CommitBreakPinLinks(state.linkFrom);
            }
            state.linkFrom = {};
            state.linkCandidate = {};
            state.linkCandidateValid = false;
            state.compatiblePins.clear();
        }
        else if (state.interaction == Interaction::BoxSelecting)
        {
            // 按画布坐标判定框选命中，避免与屏幕变换耦合
            const core::Point a = state.transform.ToCanvas({state.selectionBox.left, state.selectionBox.top});
            const core::Point b = state.transform.ToCanvas({state.selectionBox.right, state.selectionBox.bottom});
            const core::Rect box{(std::min)(a.x, b.x), (std::min)(a.y, b.y),
                                 (std::max)(a.x, b.x), (std::max)(a.y, b.y)};
            for (const auto& node : state.graph.Nodes())
            {
                const core::Rect nodeBounds{node.position.x, node.position.y,
                                            node.position.x + node.size.width,
                                            node.position.y + node.size.height};
                if (!core::Rect::Intersect(box, nodeBounds).Empty())
                    state.SelectNode(node.id, true);
            }
            if (state.selectionChanged)
                state.selectionChanged();
        }
        else if (state.interaction != Interaction::None)
        {
            if ((state.interaction == Interaction::DraggingNodes
                 || state.interaction == Interaction::DraggingGroup)
                && state.dragHistoryPending && state.pendingToggle.Valid())
            {
                state.selectedNodes.erase(
                    std::remove(state.selectedNodes.begin(), state.selectedNodes.end(),
                                state.pendingToggle),
                    state.selectedNodes.end());
                state.pendingToggle = {};
                if (state.selectionChanged)
                    state.selectionChanged();
            }
            // 拖动节点/分组结束后把结果通知出去，便于宿主标记文档已修改
            if (state.interaction == Interaction::DraggingNodes
                || state.interaction == Interaction::DraggingGroup
                || state.interaction == Interaction::ResizingGroup)
            {
                if (state.interaction == Interaction::DraggingNodes
                    || state.interaction == Interaction::DraggingGroup)
                    state.graph.RefreshGroupMembership();
                if (!state.dragDuplicateDelta.addedNodes.empty())
                {
                    StructureDelta finalDelta;
                    for (const DuiGraphNode& node : state.dragDuplicateDelta.addedNodes)
                    {
                        if (const DuiGraphNode* live = state.graph.FindNode(node.id))
                            finalDelta.addedNodes.push_back(*live);
                    }
                    for (const DuiGraphPin& pin : state.dragDuplicateDelta.addedPins)
                    {
                        if (const DuiGraphPin* live = state.graph.FindPin(pin.id))
                            finalDelta.addedPins.push_back(*live);
                    }
                    for (const DuiGraphLink& link : state.dragDuplicateDelta.addedLinks)
                    {
                        if (const DuiGraphLink* live = state.graph.FindLink(link.id))
                            finalDelta.addedLinks.push_back(*live);
                    }
                    state.CommitStructure(std::move(finalDelta));
                    state.dragDuplicateDelta = {};
                }
                else if (state.interaction == Interaction::ResizingGroup
                    && !state.dragSizesBefore.empty())
                {
                    StructureDelta delta;
                    delta.positionsBefore = state.dragPositionsBefore;
                    delta.sizesBefore = state.dragSizesBefore;
                    if (const DuiGraphNode* group = state.graph.FindNode(state.resizingGroup))
                    {
                        delta.positionsAfter.emplace_back(state.resizingGroup, group->position);
                        delta.sizesAfter.emplace_back(state.resizingGroup, group->size);
                    }
                    state.CommitStructure(std::move(delta));
                    state.dragPositionsBefore.clear();
                    state.dragSizesBefore.clear();
                }
                else if (!state.dragPositionsBefore.empty())
                    state.CommitPositions(std::move(state.dragPositionsBefore));
                state.dragPositionsBefore.clear();
                state.dragSizesBefore.clear();
                if (state.graphChanged)
                    state.graphChanged();
            }
        }
        state.dragHistoryPending = false;
        state.dragDuplicate = false;
        state.pendingToggle = {};
        state.dragDuplicateDelta = {};
        state.interaction = Interaction::None;
        SetCaptured(false);
        return true;
    }

    if (event.type == core::EventType::PointerCancel)
    {
        state.linkFrom = {};
        state.linkCandidate = {};
        state.linkCandidateValid = false;
        state.compatiblePins.clear();
        state.dragHistoryPending = false;
        state.dragDuplicate = false;
        state.pendingToggle = {};
        state.dragDuplicateDelta = {};
        state.dragPositionsBefore.clear();
        state.dragSizesBefore.clear();
        state.interaction = Interaction::None;
        SetCaptured(false);
        return true;
    }

    if (event.type == core::EventType::PointerDoubleClick)
    {
        // 双击标题栏进入重命名（与 F2 相同）；其余区域仍做视图适配
        if (state.hovered.node.Valid())
        {
            const NodeLayout* layout = state.LayoutOf(state.hovered.node);
            if (layout != nullptr
                && state.transform.ToScreenRect(layout->header).Contains(event.position)
                && !state.readOnly)
            {
                if (!state.IsNodeSelected(state.hovered.node))
                    state.SelectNode(state.hovered.node, false);
                return state.BeginRename();
            }
            FrameSelection();
        }
        else
            FrameAll();
        return true;
    }

    // ---------------------------------------------------------- 键盘
    if (event.type != core::EventType::KeyDown)
        return false;
    if (state.renamingNode.Valid())
    {
        if (event.key == core::key::Enter)
        {
            state.FinishRename(true);
            return true;
        }
        if (event.key == core::key::Escape)
        {
            state.FinishRename(false);
            return true;
        }
        if (event.key == core::key::Backspace)
        {
            if (state.renameInput == nullptr)
                PopUtf8(state.renameBuffer);
            return true;
        }
        return state.renameInput == nullptr;
    }
    if (event.key == core::key::Function2 && !state.readOnly)
        return state.BeginRename();
    if (event.key == core::key::Delete && !state.readOnly)
        return DeleteSelection();
    if (event.key == core::key::Function1) // F：适配内容
    {
        FrameAll();
        return true;
    }
    if (event.key == '0' && HasModifier(event, core::modifier::Control))
    {
        ResetView();
        return true;
    }
    if (HasModifier(event, core::modifier::Control))
    {
        if (event.key == 'X' && !state.readOnly)
            return CutSelection();
        if (event.key == 'C')
            return CopySelection();
        if (event.key == 'V' && !state.readOnly)
            return PasteClipboard({});
        if (event.key == 'D' && !state.readOnly)
            return DuplicateSelection();
        if (event.key == 'F')
            return FindAndFocus({});
        if (event.key == 'Z' && !state.readOnly)
            return Undo();
        if (event.key == 'Y' && !state.readOnly)
            return Redo();
        if (event.key == 'A')
        {
            state.ClearSelection();
            for (const auto& node : state.graph.Nodes())
                state.SelectNode(node.id, true);
            if (state.selectionChanged)
                state.selectionChanged();
            return true;
        }
    }
    return false;
}

} // namespace ysDui::controls::graph
