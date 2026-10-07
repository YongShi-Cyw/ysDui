#include "ysDui/controls/list/DuiTreeView.hpp"

#include "DuiTreeViewInternal.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/ui/DuiTextInput.hpp"

namespace ysDui::controls::list {

DuiTreeView::DuiTreeView() : tree_(std::make_unique<Impl>()) {}
DuiTreeView::~DuiTreeView()
{
    SetCaptured(false);
    tree_->textInputLifetime.reset();
    tree_->textInputToken.reset();
    tree_->textInput = nullptr;
}
DuiTreeView::DuiTreeView(DuiTreeView&&) noexcept = default;
DuiTreeView& DuiTreeView::operator=(DuiTreeView&&) noexcept = default;

int DuiTreeView::AddRoot(std::string label, std::intptr_t value)
{
    const int id = tree_->nextId++;
    tree_->nodes.push_back({id, -1, std::move(label), {}, std::vector<std::string>(tree_->columns.size()), {}, value});
    if (!tree_->columns.empty())
        tree_->nodes.back().cells[0] = tree_->nodes.back().label;
    tree_->RebuildVisible();
    return id;
}

int DuiTreeView::AddChild(int parentId, std::string label, std::intptr_t value)
{
    if (!tree_->Find(parentId)) return -1;
    const int id = tree_->nextId++;
    tree_->nodes.push_back({id, parentId, std::move(label), {}, std::vector<std::string>(tree_->columns.size()), {}, value});
    if (!tree_->columns.empty())
        tree_->nodes.back().cells[0] = tree_->nodes.back().label;
    tree_->RebuildVisible();
    return id;
}

void DuiTreeView::Remove(int id)
{
    if (!tree_->Find(id)) return;
    const bool dragSourceRemoved = std::any_of(tree_->dragSourceIds.begin(), tree_->dragSourceIds.end(),
        [this, id](int sourceId)
        {
            return sourceId == id || tree_->IsDescendantId(sourceId, id) || tree_->IsDescendantId(id, sourceId);
        });
    if ((tree_->dragCandidate || tree_->dragging)
        && (dragSourceRemoved || tree_->dragPressedId == id || tree_->IsDescendantId(tree_->dragPressedId, id)
            || tree_->IsDescendantId(id, tree_->dragPressedId)
            || tree_->dragTargetId == id)) {
        tree_->ClearDragState();
        SetCaptured(false);
    }
    if (const auto* edited = tree_->Find(tree_->editId); tree_->editId == id || (edited && tree_->IsDescendant(*edited, id)))
        CancelEdit();
    for (auto it = tree_->icons.begin(); it != tree_->icons.end();) {
        const auto* node = tree_->Find(it->first);
        if (it->first == id || (node && tree_->IsDescendant(*node, id)))
            it = tree_->icons.erase(it);
        else
            ++it;
    }
    for (auto it = tree_->subtitles.begin(); it != tree_->subtitles.end();) {
        const auto* node = tree_->Find(it->first);
        if (it->first == id || (node && tree_->IsDescendant(*node, id)))
            it = tree_->subtitles.erase(it);
        else
            ++it;
    }
    for (auto it = tree_->checkedCells.begin(); it != tree_->checkedCells.end();) {
        const int nodeId = Impl::CellNodeId(*it);
        const auto* node = tree_->Find(nodeId);
        if (nodeId == id || (node && tree_->IsDescendant(*node, id)))
            it = tree_->checkedCells.erase(it);
        else
            ++it;
    }
    for (auto it = tree_->cellProgress.begin(); it != tree_->cellProgress.end();) {
        const int nodeId = Impl::CellNodeId(it->first);
        const auto* node = tree_->Find(nodeId);
        if (nodeId == id || (node && tree_->IsDescendant(*node, id)))
            it = tree_->cellProgress.erase(it);
        else
            ++it;
    }
    for (auto it = tree_->cellLinks.begin(); it != tree_->cellLinks.end();) {
        const int nodeId = Impl::CellNodeId(it->first);
        const auto* node = tree_->Find(nodeId);
        if (nodeId == id || (node && tree_->IsDescendant(*node, id)))
            it = tree_->cellLinks.erase(it);
        else
            ++it;
    }
    for (auto it = tree_->cellImages.begin(); it != tree_->cellImages.end();) {
        const int nodeId = Impl::CellNodeId(it->first);
        const auto* node = tree_->Find(nodeId);
        if (nodeId == id || (node && tree_->IsDescendant(*node, id)))
            it = tree_->cellImages.erase(it);
        else
            ++it;
    }
    for (auto it = tree_->mutedIcons.begin(); it != tree_->mutedIcons.end();) {
        const auto* node = tree_->Find(it->first);
        if (it->first == id || (node && tree_->IsDescendant(*node, id)))
            it = tree_->mutedIcons.erase(it);
        else
            ++it;
    }
    for (auto it = tree_->mutedIconIds.begin(); it != tree_->mutedIconIds.end();) {
        const auto* node = tree_->Find(*it);
        if (*it == id || (node && tree_->IsDescendant(*node, id)))
            it = tree_->mutedIconIds.erase(it);
        else
            ++it;
    }
    for (auto it = tree_->statusColorIds.begin(); it != tree_->statusColorIds.end();) {
        const auto* node = tree_->Find(*it);
        if (*it == id || (node && tree_->IsDescendant(*node, id)))
            it = tree_->statusColorIds.erase(it);
        else
            ++it;
    }
    for (auto it = tree_->nodeChecks.begin(); it != tree_->nodeChecks.end();) {
        const auto* node = tree_->Find(it->first);
        if (it->first == id || (node && tree_->IsDescendant(*node, id))) it = tree_->nodeChecks.erase(it);
        else ++it;
    }
    for (auto it = tree_->loadStates.begin(); it != tree_->loadStates.end();) {
        const auto* node = tree_->Find(it->first);
        if (it->first == id || (node && tree_->IsDescendant(*node, id))) it = tree_->loadStates.erase(it);
        else ++it;
    }
    tree_->nodes.erase(std::remove_if(tree_->nodes.begin(), tree_->nodes.end(), [this, id](const DuiTreeNode& node) {
        return node.id == id || tree_->IsDescendant(node, id);
    }), tree_->nodes.end());
    tree_->RebuildVisible();
}

void DuiTreeView::Clear()
{
    CancelEdit();
    tree_->ClearDragState();
    SetCaptured(false);
    tree_->nodes.clear();
    tree_->icons.clear();
    tree_->subtitles.clear();
    tree_->checkedCells.clear();
    tree_->cellProgress.clear();
    tree_->cellLinks.clear();
    tree_->cellImages.clear();
    tree_->mutedIcons.clear();
    tree_->mutedIconIds.clear();
    tree_->statusColorIds.clear();
    tree_->nodeChecks.clear();
    tree_->loadStates.clear();
    tree_->visibleIds.clear();
    tree_->selectedIds.clear();
    tree_->selectedId = -1;
    tree_->selectionAnchorId = -1;
    tree_->hoveredId = -1;
    tree_->filter = {};
    tree_->sortColumn = -1;
    tree_->sortDirection = 0;
    tree_->nextId = 1;
}
int DuiTreeView::RootCount() const { return static_cast<int>(std::count_if(tree_->nodes.begin(), tree_->nodes.end(), [](const DuiTreeNode& node) { return node.parentId < 0; })); }
int DuiTreeView::ChildCount(int parentId) const { return static_cast<int>(std::count_if(tree_->nodes.begin(), tree_->nodes.end(), [parentId](const DuiTreeNode& node) { return node.parentId == parentId; })); }
int DuiTreeView::ParentId(int id) const { const DuiTreeNode* node = tree_->Find(id); return node != nullptr ? node->parentId : -1; }
bool DuiTreeView::HasChildren(int id) const { return tree_->HasChildren(id); }

void DuiTreeView::SetExpanded(int id, bool expanded)
{
    if (auto* node = tree_->Find(id)) {
        node->expanded = expanded;
        tree_->RebuildVisible();
        if (expanded && !HasChildren(id) && NodeLoadState(id) == DuiTreeLoadState::NotRequested
            && tree_->childrenLoadRequested) {
            tree_->loadStates[id] = DuiTreeLoadState::Loading;
            tree_->childrenLoadRequested(id);
        }
    }
}
void DuiTreeView::ExpandAll() { for (auto& node : tree_->nodes) node.expanded = true; tree_->RebuildVisible(); }
void DuiTreeView::CollapseAll() { for (auto& node : tree_->nodes) node.expanded = false; tree_->RebuildVisible(); }
bool DuiTreeView::Expanded(int id) const { const auto* node = tree_->Find(id); return node && node->expanded; }
std::vector<int> DuiTreeView::ExpandedSnapshot() const { std::vector<int> result; for (const auto& node : tree_->nodes) if (node.expanded) result.push_back(node.id); return result; }
void DuiTreeView::RestoreExpanded(const std::vector<int>& ids) { for (int id : ids) if (auto* node = tree_->Find(id)) node->expanded = true; tree_->RebuildVisible(); }
void DuiTreeView::SetItemVisible(int id, bool visible) { if (auto* node = tree_->Find(id)) { node->visible = visible; tree_->RebuildVisible(); } }
void DuiTreeView::SetItemSelectable(int id, bool selectable)
{
    if (auto* node = tree_->Find(id)) {
        node->selectable = selectable;
        if (!selectable && IsSelected(id)) {
            tree_->SetSelection(id, false);
            if (tree_->selectionChanged) tree_->selectionChanged(tree_->selectedId);
        }
    }
}
void DuiTreeView::SetFilter(std::function<bool(int)> filter) { tree_->filter = std::move(filter); tree_->RebuildVisible(); }
void DuiTreeView::ClearFilter() { tree_->filter = {}; tree_->RebuildVisible(); }
void DuiTreeView::SetLabel(int id, std::string label) { if (auto* node = tree_->Find(id)) { node->label = std::move(label); if (!node->cells.empty()) node->cells[0] = node->label; } }
std::string DuiTreeView::Label(int id) const { const auto* node = tree_->Find(id); return node ? node->label : std::string{}; }
void DuiTreeView::SetSubtitle(int id, std::string subtitle)
{
    if (!tree_->Find(id)) return;
    if (subtitle.empty())
        tree_->subtitles.erase(id);
    else
        tree_->subtitles[id] = std::move(subtitle);
}

std::string DuiTreeView::Subtitle(int id) const
{
    const auto it = tree_->subtitles.find(id);
    return it == tree_->subtitles.end() ? std::string{} : it->second;
}

void DuiTreeView::SetRightText(int id, std::string text) { if (auto* node = tree_->Find(id)) node->rightText = std::move(text); }
void DuiTreeView::SetStatusColor(int id, core::Color color)
{
    if (auto* node = tree_->Find(id)) {
        node->statusColor = color;
        if (color.alpha != 0)
            tree_->statusColorIds.insert(id);
        else
            tree_->statusColorIds.erase(id);
    }
}
void DuiTreeView::SetIcon(int id, std::shared_ptr<const render::DuiImage> icon)
{
    if (!tree_->Find(id)) return;
    if (icon) {
        tree_->icons[id] = std::move(icon);
        if (tree_->mutedIconIds.find(id) != tree_->mutedIconIds.end())
            tree_->mutedIcons[id] = render::DuiImage::CreateGrayscale(*tree_->icons[id]);
    }
    else {
        tree_->icons.erase(id);
        tree_->mutedIcons.erase(id);
    }
}

const std::shared_ptr<const render::DuiImage>& DuiTreeView::IconAt(int id) const
{
    static const std::shared_ptr<const render::DuiImage> empty;
    const auto it = tree_->icons.find(id);
    return it == tree_->icons.end() ? empty : it->second;
}

void DuiTreeView::SetIconMuted(int id, bool muted)
{
    if (!tree_->Find(id)) return;
    if (!muted) {
        tree_->mutedIconIds.erase(id);
        tree_->mutedIcons.erase(id);
        return;
    }
    tree_->mutedIconIds.insert(id);
    if (const auto& icon = IconAt(id))
        tree_->mutedIcons[id] = render::DuiImage::CreateGrayscale(*icon);
}

bool DuiTreeView::IconMuted(int id) const
{
    return tree_->mutedIconIds.find(id) != tree_->mutedIconIds.end();
}

void DuiTreeView::SetValue(int id, std::intptr_t value) { if (auto* node = tree_->Find(id)) node->value = value; }
std::intptr_t DuiTreeView::Value(int id) const { const auto* node = tree_->Find(id); return node ? node->value : 0; }

int DuiTreeView::AddColumn(DuiTreeColumn column)
{
    column.minimumWidth = (std::max)(detail::MinimumColumnWidth, column.minimumWidth);
    column.width = (std::max)(column.minimumWidth, column.width);
    tree_->columns.push_back(std::move(column));
    for (auto& node : tree_->nodes) {
        node.cells.resize(tree_->columns.size());
        if (tree_->columns.size() == 1)
            node.cells[0] = node.label;
    }
    return ColumnCount() - 1;
}

int DuiTreeView::ColumnCount() const { return static_cast<int>(tree_->columns.size()); }
DuiTreeColumn DuiTreeView::ColumnAt(int column) const { return column >= 0 && column < ColumnCount() ? tree_->columns[static_cast<std::size_t>(column)] : DuiTreeColumn{}; }
void DuiTreeView::SetColumnEditable(int column, bool editable) { if (column >= 0 && column < ColumnCount()) tree_->columns[static_cast<std::size_t>(column)].editable = editable; }
void DuiTreeView::SetSortIndicator(int column, int direction)
{
    tree_->sortColumn = column >= 0 && column < ColumnCount() ? column : -1;
    tree_->sortDirection = direction > 0 ? 1 : direction < 0 ? -1 : 0;
    if (tree_->sortColumn >= 0 && tree_->sortDirection != 0) {
        tree_->ReorderChildren(-1, tree_->sortColumn, tree_->sortDirection);
        tree_->RebuildVisible();
    }
}

int DuiTreeView::SortColumn() const { return tree_->sortColumn; }
int DuiTreeView::SortDirection() const { return tree_->sortDirection; }
void DuiTreeView::SetColumnClickedHandler(std::function<void(int, int)> handler) { tree_->columnClicked = std::move(handler); }
void DuiTreeView::SetContextMenuHandler(std::function<void(int, core::Point)> handler) { tree_->contextMenu = std::move(handler); }

void DuiTreeView::SetTextInput(ui::DuiTextInput* textInput)
{
    if (tree_->textInput == textInput) return;
    if (tree_->dragCandidate || tree_->dragging) {
        tree_->ClearDragState();
        SetCaptured(false);
    }
    CancelEdit();
    ui::DuiTextInput* previousInput = tree_->textInput;
    const bool previousInputAlive = previousInput != nullptr && !tree_->textInputToken.expired();
    if (previousInputAlive) {
        previousInput->SetChangedHandler({});
        previousInput->SetFocusLostHandler({});
        previousInput->SetSubmitHandler({});
        previousInput->SetVisible(false);
    }
    tree_->textInputLifetime.reset();
    tree_->textInputToken.reset();
    tree_->textInput = textInput;
    if (textInput) {
        tree_->textInputToken = textInput->LifetimeToken();
        tree_->textInputLifetime = std::make_shared<int>();
        const std::weak_ptr<int> lifetime = tree_->textInputLifetime;
        textInput->SetFocusLostHandler([lifetime, this] { if (lifetime.lock()) CommitEdit(); });
        textInput->SetSubmitHandler([lifetime, this] { if (lifetime.lock()) CommitEdit(); });
    }
}

bool DuiTreeView::BeginEdit(int id, int column)
{
    const bool implicitLabel = tree_->columns.empty() && column == 0;
    if (!tree_->textInput || !Editable() || !tree_->Find(id) || column < 0
        || (!implicitLabel && (column >= ColumnCount() || !tree_->columns[static_cast<std::size_t>(column)].editable))) return false;
    if (Editing()) CommitEdit();
    tree_->editId = id;
    tree_->editColumn = column;
    tree_->textInput->SetText(implicitLabel ? Label(id) : CellText(id, column));
    tree_->textInput->SetBorderVisible(false);
    tree_->textInput->SetEnabled(true);
    tree_->textInput->SetVisible(true);
    PlaceTextInput();
    tree_->textInput->Focus();
    return true;
}

void DuiTreeView::CommitEdit()
{
    if (!Editing()) return;
    const int id = tree_->editId;
    const int column = tree_->editColumn;
    const std::string text = tree_->textInput->Text();
    tree_->editId = -1;
    tree_->editColumn = -1;
    tree_->textInput->SetVisible(false);
    if (tree_->columns.empty()) SetLabel(id, text);
    else SetCellText(id, column, text);
    if (tree_->cellEdited) tree_->cellEdited(id, column, text);
}

void DuiTreeView::CancelEdit()
{
    if (tree_->editId < 0 || tree_->editColumn < 0) return;
    tree_->editId = -1;
    tree_->editColumn = -1;
    if (tree_->textInput && !tree_->textInputToken.expired()) tree_->textInput->SetVisible(false);
}

bool DuiTreeView::Editing() const
{
    return tree_->textInput && !tree_->textInputToken.expired() && tree_->editId >= 0 && tree_->editColumn >= 0;
}
void DuiTreeView::SetCellEditedHandler(std::function<void(int, int, std::string_view)> handler) { tree_->cellEdited = std::move(handler); }
void DuiTreeView::SetCellText(int id, int column, std::string text) { if (auto* node = tree_->Find(id); node && column >= 0 && column < ColumnCount()) { node->cells[static_cast<std::size_t>(column)] = std::move(text); if (column == 0) node->label = node->cells[0]; } }
std::string DuiTreeView::CellText(int id, int column) const { const auto* node = tree_->Find(id); return node && column >= 0 && column < static_cast<int>(node->cells.size()) ? node->cells[static_cast<std::size_t>(column)] : std::string{}; }
void DuiTreeView::SetCellChecked(int id, int column, bool checked)
{
    if (!tree_->Find(id) || column < 0 || column >= ColumnCount()) return;
    const auto key = Impl::CellKey(id, column);
    if (checked)
        tree_->checkedCells.insert(key);
    else
        tree_->checkedCells.erase(key);
}

bool DuiTreeView::CellChecked(int id, int column) const
{
    return tree_->checkedCells.find(Impl::CellKey(id, column)) != tree_->checkedCells.end();
}
void DuiTreeView::SetCellCheckedChangedHandler(std::function<void(int, int, bool)> handler)
{
    tree_->cellCheckedChanged = std::move(handler);
}
void DuiTreeView::SetCellProgress(int id, int column, int progress)
{
    if (!tree_->Find(id) || column < 0 || column >= ColumnCount()) return;
    tree_->cellProgress[Impl::CellKey(id, column)] = std::clamp(progress, 0, 100);
}

int DuiTreeView::CellProgress(int id, int column) const
{
    const auto it = tree_->cellProgress.find(Impl::CellKey(id, column));
    return it == tree_->cellProgress.end() ? 0 : it->second;
}
void DuiTreeView::SetCellProgressChangedHandler(std::function<void(int, int, int)> handler)
{
    tree_->cellProgressChanged = std::move(handler);
}

void DuiTreeView::SetCellLink(int id, int column, std::string text, std::string url)
{
    if (!tree_->Find(id) || column < 0 || column >= ColumnCount()) return;
    SetCellText(id, column, std::move(text));
    tree_->cellLinks[Impl::CellKey(id, column)] = std::move(url);
}

std::string DuiTreeView::CellLinkUrl(int id, int column) const
{
    const auto it = tree_->cellLinks.find(Impl::CellKey(id, column));
    return it == tree_->cellLinks.end() ? std::string{} : it->second;
}

void DuiTreeView::SetLinkClickedHandler(std::function<void(int, int, std::string_view)> handler)
{
    tree_->linkClicked = std::move(handler);
}

void DuiTreeView::SetCellImage(int id, int column, std::shared_ptr<const render::DuiImage> image)
{
    if (!tree_->Find(id) || column < 0 || column >= ColumnCount()) return;
    const auto key = Impl::CellKey(id, column);
    if (image)
        tree_->cellImages[key] = std::move(image);
    else
        tree_->cellImages.erase(key);
}

const std::shared_ptr<const render::DuiImage>& DuiTreeView::CellImageAt(int id, int column) const
{
    static const std::shared_ptr<const render::DuiImage> empty;
    const auto it = tree_->cellImages.find(Impl::CellKey(id, column));
    return it == tree_->cellImages.end() ? empty : it->second;
}

void DuiTreeView::SetNodeCheckState(int id, DuiTreeCheckState state, bool notify)
{
    if (!tree_->Find(id)) return;
    tree_->SetCheckRecursive(id, state);
    for (const DuiTreeNode* node = tree_->Find(tree_->Find(id)->parentId); node; node = tree_->Find(node->parentId)) {
        // 父节点状态由子节点实时聚合；清除旧的显式值，避免 Mixed 状态被缓存后阻断更新。
        tree_->nodeChecks.erase(node->id);
    }
    if (notify && tree_->nodeCheckChanged) tree_->nodeCheckChanged(id, state);
}

DuiTreeCheckState DuiTreeView::NodeCheckState(int id) const
{
    return tree_->Find(id) ? tree_->CheckState(id) : DuiTreeCheckState::Unchecked;
}

void DuiTreeView::SetNodeChecksVisible(bool visible) { tree_->nodeChecksVisible = visible; }
bool DuiTreeView::NodeChecksVisible() const { return tree_->nodeChecksVisible; }

void DuiTreeView::SetNodeCheckChangedHandler(std::function<void(int, DuiTreeCheckState)> handler)
{
    tree_->nodeCheckChanged = std::move(handler);
}

void DuiTreeView::SetNodeLoadState(int id, DuiTreeLoadState state)
{
    if (tree_->Find(id)) tree_->loadStates[id] = state;
}

DuiTreeLoadState DuiTreeView::NodeLoadState(int id) const
{
    const auto it = tree_->loadStates.find(id);
    return it == tree_->loadStates.end() ? DuiTreeLoadState::NotRequested : it->second;
}

void DuiTreeView::SetChildrenLoadRequestedHandler(std::function<void(int)> handler)
{
    tree_->childrenLoadRequested = std::move(handler);
}
void DuiTreeView::SetNodeDragEnabled(bool enabled)
{
    tree_->nodeDragEnabled = enabled;
    if (!enabled) {
        tree_->ClearDragState();
        SetCaptured(false);
    }
}
bool DuiTreeView::NodeDragEnabled() const { return tree_->nodeDragEnabled; }
void DuiTreeView::SetNodeDragHandler(std::function<void(const std::vector<int>&, int, DuiTreeDropPosition)> handler)
{
    tree_->nodeDragHandler = std::move(handler);
}
bool DuiTreeView::Dragging() const { return tree_->dragging; }
std::vector<int> DuiTreeView::DragSourceIds() const { return tree_->dragSourceIds; }
int DuiTreeView::DragTargetId() const { return tree_->dragTargetId; }
DuiTreeDropPosition DuiTreeView::DragTargetPosition() const { return tree_->dragTargetPosition; }
void DuiTreeView::SetHeaderHeight(int pixels) { tree_->headerHeight = (std::max)(detail::MinimumRowHeight, pixels); PlaceTextInput(); }
int DuiTreeView::HeaderHeight() const { return tree_->headerHeight; }
void DuiTreeView::SetEditable(bool editable) { tree_->editable = editable; if (!editable) CancelEdit(); }
bool DuiTreeView::Editable() const { return tree_->editable; }
void DuiTreeView::SetZebra(bool enabled) { tree_->zebra = enabled; }
bool DuiTreeView::Zebra() const { return tree_->zebra; }
void DuiTreeView::SetBorderVisible(bool visible) { tree_->borderVisible = visible; }
bool DuiTreeView::BorderVisible() const { return tree_->borderVisible; }
void DuiTreeView::SetHoverHighlight(bool enabled) { tree_->hoverHighlight = enabled; }
bool DuiTreeView::HoverHighlight() const { return tree_->hoverHighlight; }
void DuiTreeView::SetSelectionHighlight(bool enabled) { tree_->selectionHighlight = enabled; }
bool DuiTreeView::SelectionHighlight() const { return tree_->selectionHighlight; }
void DuiTreeView::SetFrozenColumns(int count) { tree_->frozenColumns = (std::clamp)(count, 0, ColumnCount()); }
int DuiTreeView::FrozenColumns() const { return tree_->frozenColumns; }
void DuiTreeView::SetFrozenRows(int count) { tree_->frozenRows = (std::max)(0, count); }
int DuiTreeView::FrozenRows() const { return tree_->frozenRows; }
int DuiTreeView::SelectedId() const { return tree_->selectedId; }

void DuiTreeView::SetSelectedId(int id, bool notify)
{
    if (id >= 0 && (!tree_->Find(id) || !tree_->Find(id)->selectable
        || std::find(tree_->visibleIds.begin(), tree_->visibleIds.end(), id) == tree_->visibleIds.end())) return;
    if (tree_->selectedId == id && tree_->selectedIds.size() == 1) return;
    const int previous = tree_->selectedId;
    tree_->selectedIds.clear();
    if (id >= 0) tree_->selectedIds.push_back(id);
    tree_->selectedId = id;
    tree_->selectionAnchorId = id;
    if (notify && previous != id && tree_->selectionChanged) tree_->selectionChanged(id);
}

void DuiTreeView::SetMultiSelect(bool enabled)
{
    if (tree_->multiSelect == enabled) return;
    tree_->multiSelect = enabled;
    if (!enabled && tree_->selectedIds.size() > 1)
    {
        const int selected = tree_->selectedId;
        tree_->selectedIds.clear();
        if (selected >= 0) tree_->selectedIds.push_back(selected);
    }
}

bool DuiTreeView::MultiSelect() const { return tree_->multiSelect; }

bool DuiTreeView::IsSelected(int id) const
{
    return std::find(tree_->selectedIds.begin(), tree_->selectedIds.end(), id) != tree_->selectedIds.end();
}

std::vector<int> DuiTreeView::SelectedIds() const { return tree_->selectedIds; }

void DuiTreeView::ClearSelection(bool notify)
{
    if (tree_->selectedIds.empty()) return;
    tree_->selectedIds.clear();
    tree_->selectedId = -1;
    tree_->selectionAnchorId = -1;
    if (notify && tree_->selectionChanged) tree_->selectionChanged(-1);
}

void DuiTreeView::SetSelectionChangedHandler(std::function<void(int)> handler) { tree_->selectionChanged = std::move(handler); }
void DuiTreeView::SetItemActivatedHandler(std::function<void(int)> handler) { tree_->itemActivated = std::move(handler); }
void DuiTreeView::SetHoverChangedHandler(std::function<void(int)> handler) { tree_->hoverChanged = std::move(handler); }
int DuiTreeView::HoveredId() const { return tree_->hoveredId; }
int DuiTreeView::VisibleCount() const { return static_cast<int>(tree_->visibleIds.size()); }
int DuiTreeView::IdAtVisibleRow(int row) const { return row >= 0 && row < VisibleCount() ? tree_->visibleIds[row] : -1; }
int DuiTreeView::VisibleRow(int id) const { const auto it = std::find(tree_->visibleIds.begin(), tree_->visibleIds.end(), id); return it == tree_->visibleIds.end() ? -1 : static_cast<int>(it - tree_->visibleIds.begin()); }
int DuiTreeView::ContentHeight() const { return VisibleCount() * tree_->rowHeight + (tree_->columns.empty() ? 0 : tree_->headerHeight); }

int DuiTreeView::HitTestId(core::Point point) const
{
    if (!Bounds().Contains(point)) return -1;
    const int header = tree_->columns.empty() ? 0 : tree_->headerHeight;
    return point.y < Bounds().top + header ? -1 : IdAtVisibleRow((point.y - Bounds().top - header) / tree_->rowHeight);
}

void DuiTreeView::SetRowHeight(int pixels) { tree_->rowHeight = (std::max)(detail::MinimumRowHeight, pixels); PlaceTextInput(); }
int DuiTreeView::RowHeight() const { return tree_->rowHeight; }
void DuiTreeView::SetIconSize(int pixels) { tree_->iconSize = (std::max)(detail::MinimumIconSize, pixels); }
int DuiTreeView::IconSize() const { return tree_->iconSize; }
void DuiTreeView::SetIndent(int pixels) { tree_->indent = (std::max)(detail::MinimumIndent, pixels); PlaceTextInput(); }
int DuiTreeView::Indent() const { return tree_->indent; }
void DuiTreeView::Layout(core::Rect bounds) { SetBounds(bounds); PlaceTextInput(); }

void DuiTreeView::PlaceTextInput()
{
    if (!Editing()) return;
    const int row = VisibleRow(tree_->editId);
    if (row < 0) {
        CancelEdit();
        return;
    }
    const bool implicitLabel = tree_->columns.empty() && tree_->editColumn == 0;
    int left = Bounds().left;
    for (int index = 0; index < tree_->editColumn && index < ColumnCount(); ++index)
        left += tree_->columns[static_cast<std::size_t>(index)].width;
    const int header = tree_->columns.empty() ? 0 : tree_->headerHeight;
    const int top = Bounds().top + header + row * tree_->rowHeight;
    const int right = implicitLabel ? Bounds().right : left + tree_->columns[static_cast<std::size_t>(tree_->editColumn)].width;
    tree_->textInput->SetBounds({left + 1, top + 1, (std::max)(left + 2, right - 1), (std::max)(top + 2, top + tree_->rowHeight - 1)});
}

core::DuiAccessibilityData DuiTreeView::CreateAccessibilityData() const
{
    const auto* selected = tree_->Find(tree_->selectedId);
    return {core::DuiAccessibilityRole::Tree,
            Name().empty() ? "Tree" : Name(),
            selected ? selected->label : std::string{},
            {},
            true,
            {},
            core::DuiAccessibilityPattern::ExpandCollapse | core::DuiAccessibilityPattern::Selection,
            true,
            selected && selected->expanded};
}

bool DuiTreeView::PerformAccessibilityAction(core::DuiAccessibilityAction action, std::string_view)
{
    if (!Enabled() || tree_->selectedId < 0 || !HasChildren(tree_->selectedId)) return false;
    if (action == core::DuiAccessibilityAction::Expand) {
        SetExpanded(tree_->selectedId, true);
        return true;
    }
    if (action == core::DuiAccessibilityAction::Collapse) {
        SetExpanded(tree_->selectedId, false);
        return true;
    }
    return false;
}

} // namespace ysDui::controls::list
