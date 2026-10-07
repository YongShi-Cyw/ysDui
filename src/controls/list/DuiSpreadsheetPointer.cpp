/**
 * 文件名：DuiSpreadsheetPointer.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：实现工作表命中测试、指针、键盘和剪贴板交互。
 */
#include "DuiSpreadsheetInternal.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

namespace ysDui::controls::list {

int DuiSpreadsheet::ColumnFromPoint(const Impl& data, core::Point point)
{
    if (!data.columnHeader.Contains(point) && !(point.x >= data.viewport.left && point.x < data.viewport.right))
        return -1;
    const int frozenColumnCount = data.FrozenColumnCount();
    const int frozenWidth = data.columns.OffsetAt(frozenColumnCount);
    const int offset = point.x < data.viewport.left + frozenWidth
        ? point.x - data.viewport.left
        : point.x - data.viewport.left + data.horizontal->Position();
    return data.columns.IndexAt(offset);
}

int DuiSpreadsheet::RowFromPoint(const Impl& data, core::Point point)
{
    if (!data.rowHeader.Contains(point) && !(point.y >= data.viewport.top && point.y < data.viewport.bottom))
        return -1;
    const int frozenRowCount = data.FrozenRowCount();
    const int frozenHeight = data.rows.OffsetAt(frozenRowCount);
    const int offset = point.y < data.viewport.top + frozenHeight
        ? point.y - data.viewport.top
        : point.y - data.viewport.top + data.vertical->Position();
    return data.rows.IndexAt(offset);
}

int DuiSpreadsheet::ColumnResizeIndex(const Impl& data, core::Point point)
{
    if (!data.columnHeader.Contains(point))
        return -1;
    const int column = ColumnFromPoint(data, point);
    const int frozenColumnCount = data.FrozenColumnCount();
    for (const int candidate : {column, column - 1})
        if (candidate >= 0 && candidate < data.Columns())
        {
            const int right = data.viewport.left + data.columns.OffsetAt(candidate + 1)
                - (candidate < frozenColumnCount ? 0 : data.horizontal->Position());
            if (std::abs(point.x - right) <= 3)
                return candidate;
        }
    return -1;
}

int DuiSpreadsheet::RowResizeIndex(const Impl& data, core::Point point)
{
    if (!data.rowHeader.Contains(point))
        return -1;
    const int row = RowFromPoint(data, point);
    const int frozenRowCount = data.FrozenRowCount();
    for (const int candidate : {row, row - 1})
        if (candidate >= 0 && candidate < data.Rows())
        {
            const int bottom = data.viewport.top + data.rows.OffsetAt(candidate + 1)
                - (candidate < frozenRowCount ? 0 : data.vertical->Position());
            if (std::abs(point.y - bottom) <= 3)
                return candidate;
        }
    return -1;
}

core::Control* DuiSpreadsheet::HitTest(core::Point point)
{
    core::Control* hit = core::Control::HitTest(point);
    if (hit != this)
        return hit;
    if (spreadsheet_->resizingColumn || ColumnResizeIndex(*spreadsheet_, point) >= 0)
        SetPointerCursor(core::DuiPointerCursor::ResizeHorizontal);
    else if (spreadsheet_->resizingRow || RowResizeIndex(*spreadsheet_, point) >= 0)
        SetPointerCursor(core::DuiPointerCursor::ResizeVertical);
    else if (spreadsheet_->movingSelection || SelectionBorderContains(*spreadsheet_, point))
        SetPointerCursor(core::DuiPointerCursor::Move);
    else
        SetPointerCursor(core::DuiPointerCursor::Arrow);
    return this;
}

bool DuiSpreadsheet::OnEvent(const core::Event& event)
{
    auto& data = *spreadsheet_;
    if (!Enabled() || !data.HasSheet())
        return false;
    if (event.type == core::EventType::PointerMove || event.type == core::EventType::PointerLeave)
    {
        DuiWorksheetId hoveredSheet{};
        if (event.type == core::EventType::PointerMove && data.sheetBar.Contains(event.position))
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            const int worksheetCount = expectedModel->WorksheetCount();
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            int left = data.sheetTabArea.left;
            for (int index = data.firstVisibleSheet; index < worksheetCount; ++index)
            {
                if (left + kSheetTabWidth > data.sheetTabArea.right)
                    break;
                const DuiWorksheetId sheet = expectedModel->WorksheetIdAt(index);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return true;
                const core::Rect tab{left, data.sheetBar.top + 2, left + kSheetTabWidth,
                                     data.sheetBar.bottom - 2};
                if (tab.Contains(event.position))
                {
                    if (sheet != data.sheet)
                        hoveredSheet = sheet;
                    break;
                }
                left += kSheetTabWidth + kSheetTabGap;
            }
        }
        data.hoveredSheet = hoveredSheet;
    }
    if (event.type == core::EventType::PointerDown && event.button == core::PointerButton::Secondary)
    {
        if (data.sheetBar.Contains(event.position))
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            const int worksheetCount = expectedModel->WorksheetCount();
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            int left = data.sheetTabArea.left;
            for (int index = data.firstVisibleSheet; index < worksheetCount; ++index)
            {
                if (left + kSheetTabWidth > data.sheetTabArea.right)
                    break;
                const DuiWorksheetId sheet = expectedModel->WorksheetIdAt(index);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return true;
                const core::Rect tab{left, data.sheetBar.top + 2, left + kSheetTabWidth,
                                     data.sheetBar.bottom - 2};
                if (tab.Contains(event.position))
                {
                    if (sheet != data.sheet && !SetActiveWorksheet(sheet))
                        return false;
                    if (lifetime.expired() || data.model != expectedModel || data.sheet != sheet)
                        return true;
                    return ShowSheetContextMenu(sheet, tab);
                }
                left += kSheetTabWidth + kSheetTabGap;
            }
            return false;
        }
        return ShowContextMenu(event.position);
    }
    if (event.type == core::EventType::PointerDown && event.button != core::PointerButton::Primary)
        return false;
    const auto notifySelectionChanged = [&data]()
    {
        if (!data.selectionChanged)
            return true;
        const auto handler = data.selectionChanged;
        const std::weak_ptr<int> lifetime = data.objectLifetime;
        IDuiWorkbookModel* const expectedModel = data.model;
        const DuiWorksheetId expectedSheet = data.sheet;
        const DuiCellRange selection = data.selection;
        handler(selection);
        return !lifetime.expired() && data.HasModelContext(expectedModel, expectedSheet);
    };
    if (event.type == core::EventType::PointerWheel && Bounds().Contains(event.position))
    {
        const int delta = event.wheelDelta > 0 ? -data.vertical->LineSize() * 3 : data.vertical->LineSize() * 3;
        (event.modifiers & core::modifier::Shift) != 0 ? data.horizontal->SetPosition(data.horizontal->Position() + delta)
                                                       : data.vertical->SetPosition(data.vertical->Position() + delta);
        return event.wheelDelta != 0;
    }
    if (event.type == core::EventType::PointerDown)
    {
        if (data.sheetBar.Contains(event.position))
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            const int worksheetCount = expectedModel->WorksheetCount();
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            const int visibleTabCount = std::max(1, data.sheetTabArea.Width() / (kSheetTabWidth + kSheetTabGap));
            const int lastFirstVisibleSheet = std::max(0, worksheetCount - visibleTabCount);
            int firstVisibleSheet = data.firstVisibleSheet;
            if (data.firstSheetButton.Contains(event.position))
                firstVisibleSheet = 0;
            else if (data.previousSheetButton.Contains(event.position))
                firstVisibleSheet = std::max(0, firstVisibleSheet - 1);
            else if (data.nextSheetButton.Contains(event.position))
                firstVisibleSheet = std::min(lastFirstVisibleSheet, firstVisibleSheet + 1);
            else if (data.lastSheetButton.Contains(event.position))
                firstVisibleSheet = lastFirstVisibleSheet;
            if (firstVisibleSheet != data.firstVisibleSheet)
            {
                data.firstVisibleSheet = firstVisibleSheet;
                Layout(Bounds());
                return true;
            }
            if (data.sheetListButton.Contains(event.position))
            {
                if (data.popupFactory == nullptr || !data.sheetMenu)
                    return true;
                data.sheetMenu->ClearItems();
                data.sheetMenuSheets.clear();
                data.sheetMenuSheets.reserve(static_cast<std::size_t>(worksheetCount));
                for (int index = 0; index < worksheetCount; ++index)
                {
                    const DuiWorksheetId sheet = expectedModel->WorksheetIdAt(index);
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return true;
                    const std::string name = expectedModel->WorksheetName(sheet);
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return true;
                    data.sheetMenuSheets.push_back(sheet);
                    data.sheetMenu->AddCheckItem(static_cast<std::uint32_t>(index + 1), name, sheet == data.sheet);
                }
                return data.sheetMenu->Show(*data.popupFactory, data.popupOwner, data.sheetListButton);
            }
            int left = data.sheetTabArea.left;
            for (int index = data.firstVisibleSheet; index < worksheetCount; ++index)
            {
                if (left + kSheetTabWidth > data.sheetTabArea.right)
                    break;
                const DuiWorksheetId sheet = expectedModel->WorksheetIdAt(index);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return true;
                const core::Rect tab{left, data.sheetBar.top + 2, left + kSheetTabWidth, data.sheetBar.bottom - 2};
                if (tab.Contains(event.position))
                    return SetActiveWorksheet(sheet);
                left += kSheetTabWidth + kSheetTabGap;
            }
            if (data.addSheetButton.Contains(event.position))
                return AddWorksheet() != 0;
        }
        const core::Rect selectAllButton{data.rowHeader.left, data.columnHeader.top,
                                         data.rowHeader.right, data.columnHeader.bottom};
        if (selectAllButton.Contains(event.position))
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            const int rows = expectedModel->RowCount(expectedSheet);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            const int columns = expectedModel->ColumnCount(expectedSheet);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            data.anchor = {};
            data.active = {std::max(0, rows - 1), std::max(0, columns - 1)};
            data.selection = {{}, data.active};
            data.selectionMode = SelectionMode::Cells;
            if (!notifySelectionChanged())
                return true;
            return true;
        }
        const int columnResizeIndex = ColumnResizeIndex(data, event.position);
        if (columnResizeIndex >= 0)
        {
            data.resizingColumn = true;
            data.resizeIndex = columnResizeIndex;
            data.resizeStart = event.position.x;
            data.resizeOriginal = data.ColumnSize(columnResizeIndex);
            SetCaptured(true);
            return true;
        }
        if (data.columnHeader.Contains(event.position))
        {
            const int column = ColumnFromPoint(data, event.position);
            if (column >= 0)
            {
                const std::weak_ptr<int> lifetime = data.objectLifetime;
                IDuiWorkbookModel* const expectedModel = data.model;
                const DuiWorksheetId expectedSheet = data.sheet;
                const int rows = expectedModel->RowCount(expectedSheet);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return true;
                data.anchor = {0, column};
                data.active = {std::max(0, rows - 1), column};
                data.selection = {data.anchor, data.active};
                data.selectionMode = SelectionMode::Columns;
                data.selecting = true;
                if (!notifySelectionChanged())
                    return true;
                if (!data.selecting || data.selectionMode != SelectionMode::Columns)
                    return true;
                SetCaptured(true);
            }
            return true;
        }
        const int rowResizeIndex = RowResizeIndex(data, event.position);
        if (rowResizeIndex >= 0)
        {
            data.resizingRow = true;
            data.resizeIndex = rowResizeIndex;
            data.resizeStart = event.position.y;
            data.resizeOriginal = data.RowSize(rowResizeIndex);
            SetCaptured(true);
            return true;
        }
        if (data.rowHeader.Contains(event.position))
        {
            const int row = RowFromPoint(data, event.position);
            if (row >= 0)
            {
                const std::weak_ptr<int> lifetime = data.objectLifetime;
                IDuiWorkbookModel* const expectedModel = data.model;
                const DuiWorksheetId expectedSheet = data.sheet;
                const int columns = expectedModel->ColumnCount(expectedSheet);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return true;
                data.anchor = {row, 0};
                data.active = {row, std::max(0, columns - 1)};
                data.selection = {data.anchor, data.active};
                data.selectionMode = SelectionMode::Rows;
                data.selecting = true;
                if (!notifySelectionChanged())
                    return true;
                if (!data.selecting || data.selectionMode != SelectionMode::Rows)
                    return true;
                SetCaptured(true);
            }
            return true;
        }
        if (FillHandleContains(data, event.position))
            return BeginSelectionFill();
        if (SelectionBorderContains(data, event.position))
        {
            if (Editing())
            {
                const std::weak_ptr<int> lifetime = data.objectLifetime;
                IDuiWorkbookModel* const expectedModel = data.model;
                const DuiWorksheetId expectedSheet = data.sheet;
                CommitEdit();
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
                    || Editing())
                    return true;
            }
            const DuiCellRange source = data.selection.Normalized();
            const int row = std::clamp(RowFromPoint(data, event.position),
                                       source.first.row, source.last.row);
            const int column = std::clamp(ColumnFromPoint(data, event.position),
                                          source.first.column, source.last.column);
            data.movingSelection = true;
            data.movePointerCell = {row, column};
            data.moveSource = source;
            data.moveTarget = source;
            SetPointerCursor(core::DuiPointerCursor::Move);
            SetCaptured(true);
            return true;
        }
        const int row = RowFromPoint(data, event.position);
        const int column = ColumnFromPoint(data, event.position);
        if (row >= 0 && column >= 0)
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            if (Editing())
                CommitEdit();
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet) || Editing())
                return true;
            SetActiveCell({row, column}, (event.modifiers & core::modifier::Shift) != 0);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
                || data.active != DuiCellAddress{row, column})
                return true;
            data.selecting = true;
            data.selectionMode = SelectionMode::Cells;
            SetCaptured(true);
            return true;
        }
    }
    if (!data.HasCells())
        return false;
    if (event.type == core::EventType::PointerMove && data.fillingSelection)
        return UpdateSelectionFill(event.position);
    if (event.type == core::EventType::PointerMove && data.movingSelection)
    {
        int row = RowFromPoint(data, event.position);
        int column = ColumnFromPoint(data, event.position);
        row = std::clamp(row < 0 ? (event.position.y < data.viewport.top ? 0 : data.Rows() - 1) : row,
                         0, data.Rows() - 1);
        column = std::clamp(column < 0 ? (event.position.x < data.viewport.left ? 0 : data.Columns() - 1) : column,
                            0, data.Columns() - 1);
        const DuiCellRange source = data.moveSource.Normalized();
        const int rowDelta = std::clamp(row - data.movePointerCell.row,
                                        -source.first.row, data.Rows() - 1 - source.last.row);
        const int columnDelta = std::clamp(column - data.movePointerCell.column,
                                           -source.first.column, data.Columns() - 1 - source.last.column);
        data.moveTarget = {{source.first.row + rowDelta, source.first.column + columnDelta},
                           {source.last.row + rowDelta, source.last.column + columnDelta}};
        return true;
    }
    if (event.type == core::EventType::PointerMove && (data.resizingColumn || data.resizingRow))
    {
        const int pixels = ClampDimension(data.resizeOriginal + ((data.resizingColumn ? event.position.x : event.position.y) - data.resizeStart));
        const std::weak_ptr<int> lifetime = data.objectLifetime;
        IDuiWorkbookModel* const expectedModel = data.model;
        const DuiWorksheetId expectedSheet = data.sheet;
        Impl::ModelUpdateScope update(data);
        if (data.resizingColumn)
        {
            DuiWorkbookWriteResult result = data.model->SetColumnWidthWithResult(data.sheet, data.resizeIndex, pixels);
            const bool changed = static_cast<bool>(result);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            if (!changed)
            {
                data.ReportWriteError(expectedSheet, {0, data.resizeIndex}, std::move(result),
                                      DuiSpreadsheetWriteOperation::ColumnWidth);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return true;
                return false;
            }
            const int width = expectedModel->ColumnWidth(expectedSheet, data.resizeIndex);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            data.columns.SetSize(data.resizeIndex, width);
        }
        else
        {
            DuiWorkbookWriteResult result = data.model->SetRowHeightWithResult(data.sheet, data.resizeIndex, pixels);
            const bool changed = static_cast<bool>(result);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            if (!changed)
            {
                data.ReportWriteError(expectedSheet, {data.resizeIndex, 0}, std::move(result),
                                      DuiSpreadsheetWriteOperation::RowHeight);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return true;
                return false;
            }
            const int height = expectedModel->RowHeight(expectedSheet, data.resizeIndex);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            data.rows.SetSize(data.resizeIndex, height);
        }
        Layout(Bounds());
        return true;
    }
    if (event.type == core::EventType::PointerMove && data.selecting)
    {
        int row = RowFromPoint(data, event.position);
        int column = ColumnFromPoint(data, event.position);
        if (data.selectionMode == SelectionMode::Rows)
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            const int rows = expectedModel->RowCount(expectedSheet);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            const int columns = expectedModel->ColumnCount(expectedSheet);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            if (row < 0)
                row = event.position.y < data.viewport.top ? 0 : rows - 1;
            row = std::clamp(row, 0, std::max(0, rows - 1));
            const int first = std::min(data.anchor.row, row);
            const int last = std::max(data.anchor.row, row);
            data.active = {row, std::max(0, columns - 1)};
            data.selection = {{first, 0}, {last, std::max(0, columns - 1)}};
            if (!notifySelectionChanged())
                return true;
        }
        else if (data.selectionMode == SelectionMode::Columns)
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            const int columns = expectedModel->ColumnCount(expectedSheet);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            const int rows = expectedModel->RowCount(expectedSheet);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            if (column < 0)
                column = event.position.x < data.viewport.left ? 0 : columns - 1;
            column = std::clamp(column, 0, std::max(0, columns - 1));
            const int first = std::min(data.anchor.column, column);
            const int last = std::max(data.anchor.column, column);
            data.active = {std::max(0, rows - 1), column};
            data.selection = {{0, first}, {std::max(0, rows - 1), last}};
            if (!notifySelectionChanged())
                return true;
        }
        else if (row >= 0 && column >= 0)
            SetActiveCell({row, column}, true);
        return true;
    }
    if (event.type == core::EventType::PointerUp && (data.resizingColumn || data.resizingRow))
    {
        const bool row = data.resizingRow;
        const int resizeIndex = data.resizeIndex;
        const int pixels = row ? data.RowSize(resizeIndex) : data.ColumnSize(resizeIndex);
        if (pixels != data.resizeOriginal)
        {
            const DuiCellAddress address{row ? resizeIndex : 0, row ? 0 : resizeIndex};
            data.RecordUndo({CellChange{row ? ChangeKind::RowHeight : ChangeKind::ColumnWidth,
                                         address, {}, {}, data.resizeOriginal, pixels}});
        }
        data.resizingColumn = false;
        data.resizingRow = false;
        data.resizeIndex = -1;
        SetCaptured(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && data.fillingSelection)
        return EndSelectionFill();
    if (event.type == core::EventType::PointerUp && data.movingSelection)
    {
        const DuiCellRange target = data.moveTarget;
        const std::weak_ptr<int> lifetime = data.objectLifetime;
        data.movingSelection = false;
        SetCaptured(false);
        const bool moved = CommitSelectionMove(target);
        if (lifetime.expired())
            return true;
        SetPointerCursor(core::DuiPointerCursor::Arrow);
        return moved;
    }
    if (event.type == core::EventType::PointerUp && data.selecting)
    {
        data.selecting = false;
        SetCaptured(false);
        return true;
    }
    if (event.type == core::EventType::PointerCancel
        && (data.selecting || data.fillingSelection || data.movingSelection || data.resizingColumn
            || data.resizingRow))
    {
        bool restoreFailed{};
        DuiWorkbookWriteResult restoreResult;
        if ((data.resizingColumn || data.resizingRow) && data.resizeIndex >= 0)
        {
            const bool column = data.resizingColumn;
            const int resizeIndex = data.resizeIndex;
            const int resizeOriginal = data.resizeOriginal;
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            const int current = column ? data.ColumnSize(resizeIndex) : data.RowSize(resizeIndex);
            if (current != resizeOriginal)
            {
                {
                    Impl::ModelUpdateScope update(data);
                    if (column)
                        restoreResult = expectedModel->SetColumnWidthWithResult(expectedSheet, resizeIndex, resizeOriginal);
                    else
                        restoreResult = expectedModel->SetRowHeightWithResult(expectedSheet, resizeIndex, resizeOriginal);
                    restoreFailed = !restoreResult;
                }
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return true;
                if (restoreFailed)
                {
                    data.ReportWriteError(expectedSheet, {column ? 0 : resizeIndex, column ? resizeIndex : 0},
                                          std::move(restoreResult), column ? DuiSpreadsheetWriteOperation::ColumnWidth
                                                                           : DuiSpreadsheetWriteOperation::RowHeight);
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return true;
                }
                if (!restoreFailed && column)
                {
                    const int width = expectedModel->ColumnWidth(expectedSheet, resizeIndex);
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return true;
                    data.columns.SetSize(resizeIndex, width);
                }
                else if (!restoreFailed)
                {
                    const int height = expectedModel->RowHeight(expectedSheet, resizeIndex);
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return true;
                    data.rows.SetSize(resizeIndex, height);
                }
                if (!restoreFailed)
                {
                    Layout(Bounds());
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return true;
                }
            }
        }
        data.selecting = false;
        data.fillingSelection = false;
        data.fillAxis = FillAxis::None;
        data.movingSelection = false;
        data.resizingColumn = false;
        data.resizingRow = false;
        data.resizeIndex = -1;
        SetCaptured(false);
        return !restoreFailed;
    }
    if (event.type == core::EventType::PointerDoubleClick)
    {
        if (event.button != core::PointerButton::Primary)
            return false;
        if (data.sheetBar.Contains(event.position))
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            const int worksheetCount = expectedModel->WorksheetCount();
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            int left = data.sheetTabArea.left;
            for (int index = data.firstVisibleSheet; index < worksheetCount; ++index)
            {
                if (left + kSheetTabWidth > data.sheetTabArea.right)
                    break;
                const DuiWorksheetId sheet = expectedModel->WorksheetIdAt(index);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return true;
                const core::Rect tab{left, data.sheetBar.top + 2, left + kSheetTabWidth,
                                     data.sheetBar.bottom - 2};
                if (tab.Contains(event.position))
                {
                    if (sheet != data.sheet && !SetActiveWorksheet(sheet))
                        return false;
                    if (lifetime.expired() || data.model != expectedModel || data.sheet != sheet)
                        return true;
                    return BeginWorksheetRename(sheet, tab);
                }
                left += kSheetTabWidth + kSheetTabGap;
            }
            return false;
        }
        const int columnResizeIndex = ColumnResizeIndex(data, event.position);
        if (columnResizeIndex >= 0)
            return static_cast<bool>(AutoFitColumn(columnResizeIndex));
        const int rowResizeIndex = RowResizeIndex(data, event.position);
        if (rowResizeIndex >= 0)
            return static_cast<bool>(AutoFitRow(rowResizeIndex));
        const int row = RowFromPoint(data, event.position);
        const int column = ColumnFromPoint(data, event.position);
        if (row >= 0 && column >= 0)
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            SetActiveCell({row, column});
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
                || data.active != DuiCellAddress{row, column})
                return true;
            return BeginEdit();
        }
    }
    return OnKeyboardEvent(event);
}

} // namespace ysDui::controls::list
