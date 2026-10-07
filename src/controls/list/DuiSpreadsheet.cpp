/**
 * 文件名：DuiSpreadsheet.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：实现工作表生命周期、模型、Sheet、选区与布局。
 */
#include "DuiSpreadsheetInternal.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace ysDui::controls::list {

DuiSpreadsheet::DuiSpreadsheet() : spreadsheet_(std::make_unique<Impl>())
{
    spreadsheet_->objectLifetime = std::make_shared<int>();
    spreadsheet_->requestedRanges.reserve(4);
    spreadsheet_->contextMenu = std::make_unique<DuiMenu>();
    spreadsheet_->contextMenu->SetItemInvokedHandler(
        [this](std::uint32_t command) { InvokeContextMenuCommand(command); });
    spreadsheet_->sheetMenu = std::make_unique<DuiMenu>();
    spreadsheet_->sheetMenu->SetItemInvokedHandler([this](std::uint32_t command)
    {
        auto& data = *spreadsheet_;
        const std::size_t index = command == 0 ? data.sheetMenuSheets.size()
                                                : static_cast<std::size_t>(command - 1);
        if (index < data.sheetMenuSheets.size())
            SetActiveWorksheet(data.sheetMenuSheets[index]);
    });
    spreadsheet_->sheetContextMenu = std::make_unique<DuiMenu>();
    spreadsheet_->sheetContextMenu->SetItemInvokedHandler(
        [this](std::uint32_t command) { InvokeSheetContextMenuCommand(command); });
    auto horizontal = std::make_unique<input::DuiScrollBar>(true);
    auto vertical = std::make_unique<input::DuiScrollBar>(false);
    spreadsheet_->horizontal = horizontal.get();
    spreadsheet_->vertical = vertical.get();
    horizontal->SetLineSize(kDefaultColumnWidth);
    vertical->SetLineSize(kDefaultRowHeight);
    const std::weak_ptr<int> lifetime = spreadsheet_->objectLifetime;
    horizontal->SetValueChangedHandler([this, lifetime](int)
    {
        PlaceTextInput();
        if (!lifetime.expired())
            RequestVisibleCellRanges();
    });
    vertical->SetValueChangedHandler([this, lifetime](int)
    {
        PlaceTextInput();
        if (!lifetime.expired())
            RequestVisibleCellRanges();
    });
    AddChild(std::move(horizontal));
    AddChild(std::move(vertical));
    SetAccessibilityName("Spreadsheet");
}

DuiSpreadsheet::~DuiSpreadsheet()
{
    if (spreadsheet_->contextMenu)
    {
        spreadsheet_->contextMenu->SetItemInvokedHandler({});
        spreadsheet_->contextMenu->Hide();
    }
    if (spreadsheet_->sheetMenu)
    {
        spreadsheet_->sheetMenu->SetItemInvokedHandler({});
        spreadsheet_->sheetMenu->Hide();
    }
    if (spreadsheet_->sheetContextMenu)
    {
        spreadsheet_->sheetContextMenu->SetItemInvokedHandler({});
        spreadsheet_->sheetContextMenu->Hide();
    }
    spreadsheet_->objectLifetime.reset();
    spreadsheet_->textInputLifetime.reset();
    if (spreadsheet_->HasTextInput())
    {
        ui::DuiTextInput* const input = spreadsheet_->textInput;
        const std::weak_ptr<void> inputLifetime = spreadsheet_->textInputObjectLifetime;
        input->SetChangedHandler({});
        if (!inputLifetime.expired())
            input->SetSubmitHandler({});
        if (!inputLifetime.expired())
            input->SetFocusLostHandler({});
        if (!inputLifetime.expired())
            input->SetVisible(false);
    }
    spreadsheet_->modelLifetime.reset();
    spreadsheet_->modelSubscription.Reset();
    spreadsheet_->textInputObjectLifetime.reset();
    spreadsheet_->textInput = nullptr;
}

void DuiSpreadsheet::SetWorkbookModel(IDuiWorkbookModel* model)
{
    if (spreadsheet_->model == model)
        return;
    const std::weak_ptr<int> lifetime = spreadsheet_->objectLifetime;
    CancelEdit();
    if (lifetime.expired())
        return;
    CancelWorksheetRename();
    if (lifetime.expired())
        return;
    auto& data = *spreadsheet_;
    data.modelLifetime.reset();
    data.modelSubscription.Reset();
    // Unsubscribe 回调可能销毁本控件（见 spreadsheet_tests）；此后禁止再碰成员。
    if (lifetime.expired())
        return;
    if (data.sheetMenu)
        data.sheetMenu->Hide();
    if (data.sheetContextMenu)
        data.sheetContextMenu->Hide();
    data.sheetMenuSheets.clear();
    if (lifetime.expired())
        return;
    data.selecting = false;
    data.fillingSelection = false;
    data.fillAxis = FillAxis::None;
    data.movingSelection = false;
    data.resizingColumn = false;
    data.resizingRow = false;
    data.resizeIndex = -1;
    data.sortKeys.clear();
    data.filters.clear();
    SetCaptured(false);
    data.model = model;
    data.sheet = 0;
    data.requestedModel = nullptr;
    data.requestedSheet = 0;
    data.requestedRanges.clear();
    data.frozenRows = 1;
    data.frozenColumns = 1;
    data.firstVisibleSheet = 0;
    data.undo.clear();
    data.redo.clear();
    ReloadFromModel();
    if (lifetime.expired() || data.model != model)
        return;
    if (data.model != nullptr)
    {
        const std::shared_ptr<int> subscriptionLifetime = std::make_shared<int>();
        data.modelLifetime = subscriptionLifetime;
        const std::weak_ptr<int> modelLifetime = subscriptionLifetime;
        const std::weak_ptr<int> objectLifetime = data.objectLifetime;
        core::DuiSubscription subscription = model->SubscribeChanged([objectLifetime, modelLifetime, this]
        {
            if (objectLifetime.lock() && modelLifetime.lock() && spreadsheet_->modelUpdateDepth == 0)
                ReloadFromModel();
        });
        if (lifetime.expired())
            return;
        if (data.model != model || data.modelLifetime != subscriptionLifetime)
            return;
        data.modelSubscription = std::move(subscription);
    }
}

IDuiWorkbookModel* DuiSpreadsheet::WorkbookModel() const { return spreadsheet_->model; }

void DuiSpreadsheet::ReloadFromModel()
{
    const std::weak_ptr<int> lifetime = spreadsheet_->objectLifetime;
    CancelEdit();
    if (lifetime.expired())
        return;
    CancelWorksheetRename();
    if (lifetime.expired())
        return;
    auto& data = *spreadsheet_;
    if (data.sheetMenu)
        data.sheetMenu->Hide();
    if (data.sheetContextMenu)
        data.sheetContextMenu->Hide();
    data.sheetMenuSheets.clear();
    data.selecting = false;
    data.fillingSelection = false;
    data.fillAxis = FillAxis::None;
    data.movingSelection = false;
    data.resizingColumn = false;
    data.resizingRow = false;
    data.resizeIndex = -1;
    SetCaptured(false);
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const auto contextMatches = [&]
    {
        return !lifetime.expired() && data.HasModelContext(expectedModel, expectedSheet);
    };
    if (expectedModel == nullptr)
    {
        data.sheet = 0;
        data.rows = {};
        data.columns = {};
        data.selection = {{}, {}};
        Layout(Bounds());
        return;
    }
    const int worksheetCount = expectedModel->WorksheetCount();
    if (!contextMatches())
        return;
    if (worksheetCount <= 0)
    {
        data.sheet = 0;
        data.rows = {};
        data.columns = {};
        data.selection = {{}, {}};
        Layout(Bounds());
        return;
    }
    bool found = false;
    DuiWorksheetId targetSheet = expectedSheet;
    for (int index = 0; index < worksheetCount; ++index)
    {
        found = found || expectedModel->WorksheetIdAt(index) == targetSheet;
        if (!contextMatches())
            return;
    }
    if (!found)
    {
        const DuiWorksheetId firstSheet = expectedModel->WorksheetIdAt(0);
        if (!contextMatches())
            return;
        targetSheet = firstSheet;
    }
    const int rows = std::max(0, expectedModel->RowCount(targetSheet));
    if (!contextMatches())
        return;
    const int columns = std::max(0, expectedModel->ColumnCount(targetSheet));
    if (!contextMatches())
        return;

    std::optional<DuiSparseAxisSizes> sparseRows = expectedModel->SparseRowSizes(targetSheet);
    if (!contextMatches())
        return;
    SparseAxis nextRows;
    if (sparseRows)
        nextRows = SparseAxis(rows, sparseRows->defaultPixels, std::move(sparseRows->overrides));
    else
    {
        std::vector<DuiAxisSizeOverride> overrides;
        for (int row = 0; row < rows; ++row)
        {
            const int height = expectedModel->RowHeight(targetSheet, row);
            if (!contextMatches())
                return;
            if (ClampDimension(height) != kDefaultRowHeight)
                overrides.push_back({row, height});
        }
        nextRows = SparseAxis(rows, kDefaultRowHeight, std::move(overrides));
    }

    std::optional<DuiSparseAxisSizes> sparseColumns = expectedModel->SparseColumnSizes(targetSheet);
    if (!contextMatches())
        return;
    SparseAxis nextColumns;
    if (sparseColumns)
        nextColumns = SparseAxis(columns, sparseColumns->defaultPixels, std::move(sparseColumns->overrides));
    else
    {
        std::vector<DuiAxisSizeOverride> overrides;
        for (int column = 0; column < columns; ++column)
        {
            const int width = expectedModel->ColumnWidth(targetSheet, column);
            if (!contextMatches())
                return;
            if (ClampDimension(width) != kDefaultColumnWidth)
                overrides.push_back({column, width});
        }
        nextColumns = SparseAxis(columns, kDefaultColumnWidth, std::move(overrides));
    }

    if (!contextMatches())
        return;
    data.sheet = targetSheet;
    data.rows = std::move(nextRows);
    data.columns = std::move(nextColumns);
    data.vertical->SetLineSize(data.rows.DefaultSize());
    data.horizontal->SetLineSize(data.columns.DefaultSize());
    if (rows == 0 || columns == 0)
    {
        data.active = {-1, -1};
        data.anchor = data.active;
        data.selection = {data.active, data.active};
        data.undo.clear();
        data.redo.clear();
        Layout(Bounds());
        return;
    }
    data.active.row = std::clamp(data.active.row, 0, std::max(0, rows - 1));
    data.active.column = std::clamp(data.active.column, 0, std::max(0, columns - 1));
    data.anchor = data.active;
    data.selection = {data.active, data.active};
    data.undo.clear();
    data.redo.clear();
    Layout(Bounds());
}

void DuiSpreadsheet::SetClipboard(ui::DuiClipboard* clipboard) { spreadsheet_->clipboard = clipboard; }
void DuiSpreadsheet::SetWorkbookWriteErrorHandler(
    std::function<void(const DuiSpreadsheetWriteError&)> handler)
{
    spreadsheet_->workbookWriteError = std::move(handler);
}
DuiWorksheetId DuiSpreadsheet::ActiveWorksheet() const { return spreadsheet_->sheet; }

bool DuiSpreadsheet::SetActiveWorksheet(DuiWorksheetId sheet)
{
    auto& data = *spreadsheet_;
    if (data.model == nullptr)
        return false;
    const std::weak_ptr<int> renameLifetime = data.objectLifetime;
    IDuiWorkbookModel* const renameModel = data.model;
    CommitWorksheetRename();
    if (renameLifetime.expired())
        return true;
    if (data.model != renameModel)
        return true;
    if (data.renamingSheet)
        return false;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const int worksheetCount = expectedModel->WorksheetCount();
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return true;
    for (int index = 0; index < worksheetCount; ++index)
    {
        if (expectedModel->WorksheetIdAt(index) != sheet)
        {
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            continue;
        }
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return true;
        CommitEdit();
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return true;
        if (data.editing)
            return false;
        bool targetExists{};
        const int targetCount = expectedModel->WorksheetCount();
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return true;
        for (int current = 0; current < targetCount; ++current)
        {
            targetExists = targetExists || expectedModel->WorksheetIdAt(current) == sheet;
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
        }
        if (!targetExists)
            return false;
        data.sheet = sheet;
        const int visibleTabCount = std::max(1, data.sheetTabArea.Width() / (kSheetTabWidth + kSheetTabGap));
        if (index < data.firstVisibleSheet)
            data.firstVisibleSheet = index;
        else if (index >= data.firstVisibleSheet + visibleTabCount)
            data.firstVisibleSheet = index - visibleTabCount + 1;
        data.sortKeys.clear();
        data.filters.clear();
        ReloadFromModel();
        if (lifetime.expired())
            return true;
        if (!data.HasModelContext(expectedModel, sheet))
            return false;
        if (data.sheetChanged)
        {
            const auto handler = data.sheetChanged;
            const std::weak_ptr<int> handlerLifetime = data.objectLifetime;
            handler(data.sheet);
            if (handlerLifetime.expired())
                return true;
        }
        return true;
    }
    return false;
}

DuiWorksheetId DuiSpreadsheet::AddWorksheet(std::string name)
{
    if (spreadsheet_->model == nullptr)
        return 0;
    const std::weak_ptr<int> lifetime = spreadsheet_->objectLifetime;
    IDuiWorkbookModel* const expectedModel = spreadsheet_->model;
    const DuiWorksheetId expectedSheet = spreadsheet_->sheet;
    CommitEdit();
    if (lifetime.expired() || !spreadsheet_->HasModelContext(expectedModel, expectedSheet)
        || spreadsheet_->editing)
        return 0;
    if (name.empty())
    {
        const int worksheetCount = expectedModel->WorksheetCount();
        if (lifetime.expired() || !spreadsheet_->HasModelContext(expectedModel, expectedSheet))
            return 0;
        name = "Sheet" + std::to_string(worksheetCount + 1);
    }
    const auto uniqueName = UniqueWorksheetName(
        *spreadsheet_, expectedModel, expectedSheet, std::move(name));
    if (!uniqueName)
        return 0;
    DuiWorksheetId sheet{};
    {
        Impl::ModelUpdateScope update(*spreadsheet_);
        sheet = spreadsheet_->model->AddWorksheet(*uniqueName);
    }
    if (lifetime.expired() || !spreadsheet_->HasModelContext(expectedModel, expectedSheet))
        return 0;
    if (sheet != 0 && !SetActiveWorksheet(sheet))
        return 0;
    return sheet;
}

bool DuiSpreadsheet::RenameActiveWorksheet(std::string name)
{
    auto& data = *spreadsheet_;
    if (data.model == nullptr || data.sheet == 0)
        return false;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const auto nameExists = WorksheetNameExists(
        data, expectedModel, expectedSheet, name, expectedSheet);
    if (!nameExists || *nameExists)
        return false;
    bool renamed{};
    {
        Impl::ModelUpdateScope update(data);
        renamed = data.model->RenameWorksheet(data.sheet, std::move(name));
    }
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet) || !renamed)
        return false;
    bool activeExists{};
    const int worksheetCount = expectedModel->WorksheetCount();
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return false;
    for (int index = 0; index < worksheetCount; ++index)
    {
        activeExists = activeExists || expectedModel->WorksheetIdAt(index) == expectedSheet;
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return false;
    }
    if (activeExists)
        return true;
    data.sheet = 0;
    ReloadFromModel();
    if (lifetime.expired() || data.model != expectedModel)
        return false;
    if (data.sheetChanged)
    {
        const auto handler = data.sheetChanged;
        const std::weak_ptr<int> handlerLifetime = data.objectLifetime;
        handler(data.sheet);
        if (handlerLifetime.expired())
            return false;
    }
    return false;
}

bool DuiSpreadsheet::RemoveActiveWorksheet()
{
    auto& data = *spreadsheet_;
    if (data.model == nullptr || data.sheet == 0)
        return false;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const int worksheetCount = expectedModel->WorksheetCount();
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return true;
    if (worksheetCount <= 1)
        return false;
    CommitEdit();
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return true;
    if (data.editing)
        return false;
    const DuiWorksheetId removed = data.sheet;
    bool removedSheet{};
    {
        Impl::ModelUpdateScope update(data);
        removedSheet = data.model->RemoveWorksheet(removed);
    }
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return removedSheet;
    if (!removedSheet)
        return false;
    data.sheet = 0;
    ReloadFromModel();
    if (lifetime.expired() || data.model != expectedModel)
        return true;
    if (data.sheetChanged)
    {
        const auto handler = data.sheetChanged;
        const std::weak_ptr<int> handlerLifetime = data.objectLifetime;
        handler(data.sheet);
        if (handlerLifetime.expired())
            return true;
    }
    return true;
}

DuiCellAddress DuiSpreadsheet::ActiveCell() const { return spreadsheet_->active; }
DuiCellRange DuiSpreadsheet::Selection() const { return spreadsheet_->selection; }
std::optional<core::Rect> DuiSpreadsheet::VisibleCellBounds(DuiCellAddress cell) const
{
    const auto& data = *spreadsheet_;
    if (!data.HasSheet() || !cell.Valid() || cell.row >= data.Rows() || cell.column >= data.Columns())
        return std::nullopt;

    core::Rect clip = data.viewport;
    const int frozenHeight = data.FrozenHeight();
    const int frozenWidth = data.FrozenWidth();
    if (cell.row < data.FrozenRowCount())
        clip.bottom = std::min(clip.bottom, data.viewport.top + frozenHeight);
    else
        clip.top = std::max(clip.top, data.viewport.top + frozenHeight);
    if (cell.column < data.FrozenColumnCount())
        clip.right = std::min(clip.right, data.viewport.left + frozenWidth);
    else
        clip.left = std::max(clip.left, data.viewport.left + frozenWidth);

    const core::Rect visible = core::Rect::Intersect(data.CellRect(cell.row, cell.column), clip);
    if (visible.right <= visible.left || visible.bottom <= visible.top)
        return std::nullopt;
    return visible;
}
int DuiSpreadsheet::FrozenRowCount() const { return spreadsheet_->FrozenRowCount(); }
int DuiSpreadsheet::FrozenColumnCount() const { return spreadsheet_->FrozenColumnCount(); }

void DuiSpreadsheet::SetFrozenPanes(int rows, int columns)
{
    auto& data = *spreadsheet_;
    if (!data.HasSheet())
        return;
    data.frozenRows = std::clamp(rows, 0, data.Rows());
    data.frozenColumns = std::clamp(columns, 0, data.Columns());
    Layout(Bounds());
}

void DuiSpreadsheet::ClearFrozenPanes()
{
    SetFrozenPanes(0, 0);
}

void DuiSpreadsheet::SetActiveCell(DuiCellAddress cell, bool extendSelection)
{
    auto& data = *spreadsheet_;
    if (!data.HasCells() || cell.row < 0 || cell.column < 0)
        return;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const int rows = expectedModel->RowCount(expectedSheet);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return;
    const int columns = expectedModel->ColumnCount(expectedSheet);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return;
    if (cell.row >= rows || cell.column >= columns)
        return;
    const bool changed = data.active != cell;
    data.active = cell;
    if (extendSelection)
        data.selection = {data.anchor, cell};
    else
    {
        data.anchor = cell;
        data.selection = {cell, cell};
    }
    if (changed && data.activeChanged)
    {
        const auto handler = data.activeChanged;
        handler(cell);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.active != cell)
            return;
    }
    if (data.selectionChanged)
    {
        const auto handler = data.selectionChanged;
        const DuiCellRange selection = data.selection;
        handler(selection);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.active != cell)
            return;
    }
    EnsureVisible(cell);
}

void DuiSpreadsheet::SetSelection(DuiCellRange range)
{
    auto& data = *spreadsheet_;
    if (!data.HasCells())
        return;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const int rows = expectedModel->RowCount(expectedSheet);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return;
    const int columns = expectedModel->ColumnCount(expectedSheet);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return;
    range = range.Normalized();
    range.first.row = std::clamp(range.first.row, 0, std::max(0, rows - 1));
    range.last.row = std::clamp(range.last.row, range.first.row, std::max(0, rows - 1));
    range.first.column = std::clamp(range.first.column, 0, std::max(0, columns - 1));
    range.last.column = std::clamp(range.last.column, range.first.column, std::max(0, columns - 1));
    data.selection = range;
    data.anchor = range.first;
    data.active = range.last;
    data.selectionMode = SelectionMode::Cells;
    if (data.selectionChanged)
    {
        const auto handler = data.selectionChanged;
        handler(data.selection);
        if (lifetime.expired())
            return;
    }
}

void DuiSpreadsheet::SetActiveCellChangedHandler(std::function<void(DuiCellAddress)> handler) { spreadsheet_->activeChanged = std::move(handler); }
void DuiSpreadsheet::SetSelectionChangedHandler(std::function<void(DuiCellRange)> handler) { spreadsheet_->selectionChanged = std::move(handler); }
void DuiSpreadsheet::SetWorksheetChangedHandler(std::function<void(DuiWorksheetId)> handler) { spreadsheet_->sheetChanged = std::move(handler); }

void DuiSpreadsheet::Layout(core::Rect bounds)
{
    auto& data = *spreadsheet_;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    SetBounds(bounds);
    data.sheetBar = {bounds.left, bounds.bottom - kSheetBarHeight, bounds.right, bounds.bottom};
    const int worksheetCount = expectedModel == nullptr ? 0 : expectedModel->WorksheetCount();
    if (lifetime.expired() || data.model != expectedModel)
        return;
    data.firstVisibleSheet = std::clamp(data.firstVisibleSheet, 0, std::max(0, worksheetCount - 1));
    const int navigationTop = data.sheetBar.top + 4;
    const int navigationBottom = data.sheetBar.bottom - 4;
    int navigationLeft = data.sheetBar.left + 2;
    const auto placeNavigationButton = [&]
    {
        const core::Rect result{navigationLeft, navigationTop, navigationLeft + kSheetNavigationButtonWidth,
                                navigationBottom};
        navigationLeft += kSheetNavigationButtonWidth;
        return result;
    };
    data.firstSheetButton = placeNavigationButton();
    data.previousSheetButton = placeNavigationButton();
    data.nextSheetButton = placeNavigationButton();
    data.lastSheetButton = placeNavigationButton();
    data.sheetListButton = placeNavigationButton();
    const int tabLeft = navigationLeft + 4;
    const int tabWidth = std::clamp(bounds.Width() / 2 - (tabLeft - bounds.left), 0, 360);
    data.sheetTabArea = {tabLeft, data.sheetBar.top, tabLeft + tabWidth, data.sheetBar.bottom};
    const int visibleTabCount = std::max(1, data.sheetTabArea.Width() / (kSheetTabWidth + kSheetTabGap));
    const int displayedTabs = std::min(visibleTabCount, std::max(0, worksheetCount - data.firstVisibleSheet));
    const int addSheetLeft = data.sheetTabArea.left + displayedTabs * (kSheetTabWidth + kSheetTabGap);
    data.addSheetButton = {addSheetLeft, data.sheetBar.top, addSheetLeft + kAddSheetButtonWidth,
                           data.sheetBar.bottom};
    const core::Rect content{bounds.left, bounds.top, bounds.right, data.sheetBar.top};
    data.columnHeader = {content.left + kRowHeaderWidth, content.top, content.right - kScrollBarSize, content.top + kColumnHeaderHeight};
    data.rowHeader = {content.left, content.top + kColumnHeaderHeight, content.left + kRowHeaderWidth, content.bottom};
    data.viewport = {data.columnHeader.left, data.columnHeader.bottom, data.columnHeader.right, data.rowHeader.bottom};
    data.selectAllTriangle = {};
    data.selectAllTriangle.MoveTo({data.rowHeader.right - 9, data.columnHeader.bottom - 4});
    data.selectAllTriangle.LineTo({data.rowHeader.right - 4, data.columnHeader.bottom - 9});
    data.selectAllTriangle.LineTo({data.rowHeader.right - 4, data.columnHeader.bottom - 4});
    data.selectAllTriangle.Close();
    const int horizontalLeft = std::min(data.sheetBar.right, data.addSheetButton.right + 8);
    const int horizontalTop = data.sheetBar.top + (kSheetBarHeight - kScrollBarSize) / 2;
    data.horizontal->SetBounds({horizontalLeft, horizontalTop, data.sheetBar.right, horizontalTop + kScrollBarSize});
    data.vertical->SetBounds({data.viewport.right, data.viewport.top, content.right, content.bottom});
    const int frozenWidth = data.FrozenWidth();
    const int frozenHeight = data.FrozenHeight();
    const int visibleWidth = std::max(1, data.viewport.Width() - frozenWidth);
    const int visibleHeight = std::max(1, data.viewport.Height() - frozenHeight);
    const int totalWidth = data.columns.TotalSize();
    const int totalHeight = data.rows.TotalSize();
    data.horizontal->SetRange(0, std::max(0, totalWidth - frozenWidth - visibleWidth));
    data.horizontal->SetPageSize(visibleWidth);
    data.horizontal->SetVisible(totalWidth > data.viewport.Width());
    data.vertical->SetRange(0, std::max(0, totalHeight - frozenHeight - visibleHeight));
    data.vertical->SetPageSize(visibleHeight);
    data.vertical->SetVisible(totalHeight > data.viewport.Height());
    PlaceTextInput();
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return;
    RequestVisibleCellRanges();
}

void DuiSpreadsheet::EnsureVisible(DuiCellAddress cell)
{
    auto& data = *spreadsheet_;
    if (!data.HasSheet() || data.rows.Empty() || data.columns.Empty())
        return;
    const int frozenWidth = data.FrozenWidth();
    const int frozenHeight = data.FrozenHeight();
    const int x = data.columns.OffsetAt(cell.column) - frozenWidth;
    const int y = data.rows.OffsetAt(cell.row) - frozenHeight;
    const int viewWidth = std::max(1, data.viewport.Width() - frozenWidth);
    const int viewHeight = std::max(1, data.viewport.Height() - frozenHeight);
    if (cell.column >= data.FrozenColumnCount() && x < data.horizontal->Position())
        data.horizontal->SetPosition(x);
    else if (cell.column >= data.FrozenColumnCount() && x + data.ColumnSize(cell.column) > data.horizontal->Position() + viewWidth)
        data.horizontal->SetPosition(x + data.ColumnSize(cell.column) - viewWidth);
    if (cell.row >= data.FrozenRowCount() && y < data.vertical->Position())
        data.vertical->SetPosition(y);
    else if (cell.row >= data.FrozenRowCount() && y + data.RowSize(cell.row) > data.vertical->Position() + viewHeight)
        data.vertical->SetPosition(y + data.RowSize(cell.row) - viewHeight);
}

void DuiSpreadsheet::PlaceTextInput()
{
    auto& data = *spreadsheet_;
    if (!data.HasTextInput())
        return;
    if (data.renamingSheet)
    {
        data.textInput->SetBounds(data.renameSheetBounds);
        return;
    }
    if (!data.editing || data.rows.Empty())
        return;
    const int row = data.editRow;
    const int column = data.editColumn;
    const core::Rect cell = data.CellRect(row, column);
    const int left = cell.left;
    const int top = cell.top;
    const core::Rect bounds{left + 1, top + 1, left + data.ColumnSize(column) - 1, top + data.RowSize(row) - 1};
    data.textInput->SetBounds(bounds);
}

} // namespace ysDui::controls::list
