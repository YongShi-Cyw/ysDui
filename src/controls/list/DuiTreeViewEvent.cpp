/**
 * 文件名：DuiTreeViewEvent.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：DuiTreeView 的事件处理（指针选择、拖拽重排、双击编辑、键盘导航），
 *       由 DuiTreeView.cpp 拆分而来，共享 DuiTreeViewInternal.hpp 中的 Impl 与布局常量。
 */
#include "ysDui/controls/list/DuiTreeView.hpp"

#include "DuiTreeViewInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace ysDui::controls::list {

bool DuiTreeView::OnEvent(const core::Event& event)
{
    if (!Enabled()) return false;
    if (event.type == core::EventType::PointerDown && event.button == core::PointerButton::Secondary) {
        const int id = HitTestId(event.position);
        if (id >= 0 && tree_->contextMenu) {
            tree_->contextMenu(id, event.position);
            return true;
        }
    }
    if (event.type == core::EventType::PointerMove) {
        if (tree_->dragging) {
            tree_->dragTargetId = HitTestId(event.position);
            tree_->dragTargetValid = false;
            if (tree_->dragTargetId >= 0) {
                const int row = VisibleRow(tree_->dragTargetId);
                const int header = tree_->columns.empty() ? 0 : tree_->headerHeight;
                const int rowTop = Bounds().top + header + row * tree_->rowHeight;
                const int offset = event.position.y - rowTop;
                const int third = (std::max)(1, tree_->rowHeight / 3);
                tree_->dragTargetPosition = offset < third ? DuiTreeDropPosition::Before
                    : offset >= tree_->rowHeight - third ? DuiTreeDropPosition::After
                    : DuiTreeDropPosition::Inside;
                tree_->dragTargetValid = tree_->ValidDrop(tree_->dragTargetId, tree_->dragTargetPosition);
            }
        }
        else if (tree_->dragCandidate
                 && (std::abs(event.position.x - tree_->dragStart.x) >= detail::DragThreshold
                     || std::abs(event.position.y - tree_->dragStart.y) >= detail::DragThreshold)) {
            tree_->dragging = true;
            SetCaptured(true);
            tree_->dragTargetId = HitTestId(event.position);
            tree_->dragTargetValid = false;
            if (tree_->dragTargetId >= 0) {
                const int row = VisibleRow(tree_->dragTargetId);
                const int header = tree_->columns.empty() ? 0 : tree_->headerHeight;
                const int rowTop = Bounds().top + header + row * tree_->rowHeight;
                const int offset = event.position.y - rowTop;
                const int third = (std::max)(1, tree_->rowHeight / 3);
                tree_->dragTargetPosition = offset < third ? DuiTreeDropPosition::Before
                    : offset >= tree_->rowHeight - third ? DuiTreeDropPosition::After
                    : DuiTreeDropPosition::Inside;
                tree_->dragTargetValid = tree_->ValidDrop(tree_->dragTargetId, tree_->dragTargetPosition);
            }
        }
        const int hovered = HitTestId(event.position);
        if (hovered != tree_->hoveredId) {
            tree_->hoveredId = hovered;
            if (tree_->hoverChanged)
                tree_->hoverChanged(hovered);
        }
        return hovered >= 0;
    }
    if (event.type == core::EventType::PointerLeave || event.type == core::EventType::PointerCancel) {
        if (event.type == core::EventType::PointerCancel && (tree_->dragCandidate || tree_->dragging)) {
            tree_->ClearDragState();
            SetCaptured(false);
            return true;
        }
        if (tree_->dragging) {
            tree_->dragTargetId = -1;
            tree_->dragTargetValid = false;
            return true;
        }
        if (tree_->dragCandidate) tree_->ClearDragState();
        if (tree_->hoveredId != -1) {
            tree_->hoveredId = -1;
            if (tree_->hoverChanged) tree_->hoverChanged(-1);
        }
        return false;
    }
    if (event.type == core::EventType::PointerDown) {
        if (Editing()) CommitEdit();
        if (!tree_->columns.empty() && event.position.y >= Bounds().top
            && event.position.y < Bounds().top + tree_->headerHeight) {
            int left = Bounds().left;
            for (int column{}; column < ColumnCount(); ++column) {
                const auto& definition = tree_->columns[static_cast<std::size_t>(column)];
                if (event.position.x >= left && event.position.x < left + definition.width) {
                    if (definition.sortable) {
                        const int direction = SortColumn() == column && SortDirection() > 0 ? -1 : 1;
                        SetSortIndicator(column, direction);
                        if (tree_->columnClicked)
                            tree_->columnClicked(column, direction);
                    }
                    return true;
                }
                left += definition.width;
            }
            return false;
        }
        const int id = HitTestId(event.position);
        const auto* node = tree_->Find(id);
        if (!node) return false;
        const int row = VisibleRow(id);
        const int depth = tree_->Depth(id);
        const int checkLeft = Bounds().left + depth * tree_->indent + detail::GlyphWidth + 2;
        const core::Rect nodeCheck{checkLeft, Bounds().top + tree_->headerHeight + row * tree_->rowHeight + (tree_->rowHeight - 14) / 2,
                                   checkLeft + 14, Bounds().top + tree_->headerHeight + row * tree_->rowHeight + (tree_->rowHeight - 14) / 2 + 14};
        if (tree_->columns.empty() && tree_->nodeChecksVisible && nodeCheck.Contains(event.position)) {
            const auto next = NodeCheckState(id) == DuiTreeCheckState::Checked
                ? DuiTreeCheckState::Unchecked : DuiTreeCheckState::Checked;
            SetNodeCheckState(id, next);
            return true;
        }
        if (!tree_->columns.empty()) {
            int left = Bounds().left;
            for (int column{}; column < ColumnCount(); ++column) {
                const auto& definition = tree_->columns[static_cast<std::size_t>(column)];
                const core::Rect cell{left, Bounds().top + tree_->headerHeight + VisibleRow(id) * tree_->rowHeight,
                                      left + definition.width, Bounds().top + tree_->headerHeight + (VisibleRow(id) + 1) * tree_->rowHeight};
                if (definition.kind == DuiTreeColumnKind::CheckBox && cell.Contains(event.position)) {
                    const bool checked = !CellChecked(id, column);
                    SetCellChecked(id, column, checked);
                    if (tree_->cellCheckedChanged)
                        tree_->cellCheckedChanged(id, column, checked);
                    return true;
                }
                if (definition.kind == DuiTreeColumnKind::ProgressBar && cell.Contains(event.position)) {
                    const int progress = (event.position.x - cell.left) * 100 / (std::max)(1, cell.Width());
                    SetCellProgress(id, column, progress);
                    if (tree_->cellProgressChanged)
                        tree_->cellProgressChanged(id, column, CellProgress(id, column));
                    return true;
                }
                if (definition.kind == DuiTreeColumnKind::Hyperlink && cell.Contains(event.position)) {
                    if (tree_->linkClicked)
                        tree_->linkClicked(id, column, CellLinkUrl(id, column));
                    return true;
                }
                left += definition.width;
            }
        }
        const int glyphRight = Bounds().left + depth * tree_->indent + detail::GlyphWidth;
        if (event.position.x < glyphRight && !HasChildren(id)
            && NodeLoadState(id) == DuiTreeLoadState::NotRequested && tree_->childrenLoadRequested) {
            tree_->loadStates[id] = DuiTreeLoadState::Loading;
            tree_->childrenLoadRequested(id);
        }
        else if (HasChildren(id) && event.position.x < glyphRight) {
            SetExpanded(id, !node->expanded);
        }
        else if (node->selectable) {
            const bool shift = (event.modifiers & core::modifier::Shift) != 0U;
            const bool control = (event.modifiers & core::modifier::Control) != 0U;
            if (!tree_->multiSelect || (!shift && !control && !IsSelected(id))) {
                SetSelectedId(id);
            }
            else if (!shift && !control && IsSelected(id)) {
                tree_->selectedId = id;
                tree_->selectionAnchorId = id;
            }
            else if (shift && tree_->selectionAnchorId >= 0) {
                const int anchorRow = VisibleRow(tree_->selectionAnchorId);
                const int targetRow = VisibleRow(id);
                if (anchorRow >= 0 && targetRow >= 0) {
                    const int first = (std::min)(anchorRow, targetRow);
                    const int last = (std::max)(anchorRow, targetRow);
                    tree_->selectedIds.clear();
                    for (int index = first; index <= last; ++index) {
                        const int rangeId = IdAtVisibleRow(index);
                        const auto* rangeNode = tree_->Find(rangeId);
                        if (rangeNode && rangeNode->selectable)
                            tree_->selectedIds.push_back(rangeId);
                    }
                    tree_->selectedId = id;
                    if (tree_->selectionChanged) tree_->selectionChanged(id);
                }
            }
            else {
                const bool selected = IsSelected(id);
                tree_->SetSelection(id, !selected);
                tree_->selectionAnchorId = id;
                if (tree_->selectionChanged) tree_->selectionChanged(tree_->selectedId);
            }
            const bool inDisclosureArea = event.position.x < glyphRight;
            if (tree_->nodeDragEnabled && event.button == core::PointerButton::Primary && !inDisclosureArea) {
                tree_->dragCandidate = true;
                tree_->dragStart = event.position;
                tree_->dragPressedId = id;
                tree_->dragSourceIds = tree_->DragRoots(id);
            }
        }
        return true;
    }
    if (event.type == core::EventType::PointerUp) {
        if (tree_->dragging) {
            const std::vector<int> sourceIds = tree_->dragSourceIds;
            const int targetId = tree_->dragTargetId;
            const DuiTreeDropPosition position = tree_->dragTargetPosition;
            const bool moved = tree_->dragTargetValid && tree_->MoveDraggedNodes(targetId, position);
            tree_->ClearDragState();
            SetCaptured(false);
            if (moved && tree_->nodeDragHandler)
                tree_->nodeDragHandler(sourceIds, targetId, position);
            return true;
        }
        if (tree_->dragCandidate) {
            tree_->ClearDragState();
            return false;
        }
    }
    if (event.type == core::EventType::PointerDoubleClick) {
        tree_->ClearDragState();
        SetCaptured(false);
        const int id = HitTestId(event.position);
        if (id < 0) return false;
        if (const auto* node = tree_->Find(id); node && node->selectable)
            SetSelectedId(id, false);
        // 不可编辑时双击激活；可编辑时保持原有就地编辑行为
        if (!Editable()) {
            if (tree_->itemActivated)
                tree_->itemActivated(id);
            return true;
        }
        if (!tree_->columns.empty()) {
            int left = Bounds().left;
            for (int column = 0; column < ColumnCount(); ++column) {
                const auto& definition = tree_->columns[static_cast<std::size_t>(column)];
                if (event.position.x >= left && event.position.x < left + definition.width)
                    return BeginEdit(id, column);
                left += definition.width;
            }
        }
        return BeginEdit(id, 0);
    }
    if (event.type != core::EventType::KeyDown || VisibleCount() == 0) return false;
    if (event.key == core::key::Enter && Editing()) { CommitEdit(); return true; }
    if (event.key == core::key::Enter && !Editing() && SelectedId() >= 0 && tree_->itemActivated) {
        tree_->itemActivated(SelectedId());
        return true;
    }
    if (event.key == core::key::Escape && Editing()) { CancelEdit(); return true; }
    if (event.key == core::key::Function1 && SelectedId() >= 0) return BeginEdit(SelectedId(), 0);
    int row = VisibleRow(SelectedId());
    auto selectableAt = [this](int start, int step) {
        for (int index = start; index >= 0 && index < VisibleCount(); index += step) {
            const auto* node = tree_->Find(IdAtVisibleRow(index));
            if (node && node->selectable) return index;
        }
        return -1;
    };
    auto selectRow = [this](int target, unsigned int modifiers) {
        if (target < 0) return;
        const int id = IdAtVisibleRow(target);
        if ((modifiers & core::modifier::Shift) != 0U && tree_->multiSelect
            && tree_->selectionAnchorId >= 0) {
            const int anchor = VisibleRow(tree_->selectionAnchorId);
            if (anchor >= 0) {
                tree_->selectedIds.clear();
                const int first = (std::min)(anchor, target);
                const int last = (std::max)(anchor, target);
                for (int index = first; index <= last; ++index) {
                    const int rangeId = IdAtVisibleRow(index);
                    const auto* node = tree_->Find(rangeId);
                    if (node && node->selectable) tree_->selectedIds.push_back(rangeId);
                }
                tree_->selectedId = id;
                if (tree_->selectionChanged) tree_->selectionChanged(id);
                return;
            }
        }
        SetSelectedId(id);
    };
    if (event.key == core::key::Up || event.key == core::key::Down) {
        const int step = event.key == core::key::Up ? -1 : 1;
        const int start = row < 0 ? (step > 0 ? 0 : VisibleCount() - 1) : row + step;
        const int target = selectableAt(start, step);
        if (target < 0 && row < 0) return false;
        selectRow(target < 0 ? row : target, event.modifiers);
    }
    else if (event.key == core::key::Home || event.key == core::key::End) {
        const int target = selectableAt(event.key == core::key::Home ? 0 : VisibleCount() - 1,
                                       event.key == core::key::Home ? 1 : -1);
        selectRow(target, event.modifiers);
    }
    else if (event.key == core::key::PageUp || event.key == core::key::PageDown) {
        const int step = (std::max)(1, Bounds().Height() / tree_->rowHeight - 1);
        const int targetRow = (std::clamp)(row < 0 ? 0 : row + (event.key == core::key::PageUp ? -step : step),
                                           0, VisibleCount() - 1);
        const int target = selectableAt(targetRow, event.key == core::key::PageUp ? -1 : 1);
        selectRow(target < 0 ? targetRow : target, event.modifiers);
    }
    else if (event.key == core::key::Left && SelectedId() >= 0) {
        const auto* node = tree_->Find(SelectedId());
        if (node && node->expanded && HasChildren(SelectedId())) SetExpanded(SelectedId(), false);
        else if (node && node->parentId >= 0) SetSelectedId(node->parentId);
        else return false;
    }
    else if (event.key == core::key::Space && SelectedId() >= 0) {
        if (!HasChildren(SelectedId())) return false;
        SetExpanded(SelectedId(), !tree_->Find(SelectedId())->expanded);
    }
    else if (event.key == core::key::Right && SelectedId() >= 0) {
        const auto* node = tree_->Find(SelectedId());
        if (!node || !HasChildren(SelectedId())) return false;
        if (!node->expanded) SetExpanded(SelectedId(), true);
        else {
            const int childRow = row + 1;
            const int child = selectableAt(childRow, 1);
            if (child == childRow) selectRow(child, event.modifiers);
        }
    }
    else return false;
    return true;
}

} // namespace ysDui::controls::list
