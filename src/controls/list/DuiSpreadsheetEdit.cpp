/**
 * 文件名：DuiSpreadsheetEdit.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：实现工作表编辑代理与撤销重做。
 */
#include "DuiSpreadsheetInternal.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace ysDui::controls::list {

void DuiSpreadsheet::SetTextInput(ui::DuiTextInput* input)
{
    if (spreadsheet_->textInput == input)
        return;
    const std::weak_ptr<int> lifetime = spreadsheet_->objectLifetime;
    CancelEdit();
    if (lifetime.expired())
        return;
    CancelWorksheetRename();
    if (lifetime.expired())
        return;
    auto& data = *spreadsheet_;
    ui::DuiTextInput* const expectedInput = data.textInput;
    const bool expectedInputAlive = data.HasTextInput();
    const std::weak_ptr<void> expectedInputObjectLifetime = data.textInputObjectLifetime;
    data.textInputLifetime.reset();
    data.textInputObjectLifetime.reset();
    if (expectedInputAlive)
    {
        expectedInput->SetChangedHandler({});
        if (lifetime.expired() || expectedInputObjectLifetime.expired() || data.textInput != expectedInput)
            return;
        expectedInput->SetSubmitHandler({});
        if (lifetime.expired() || expectedInputObjectLifetime.expired() || data.textInput != expectedInput)
            return;
        expectedInput->SetFocusLostHandler({});
        if (lifetime.expired() || expectedInputObjectLifetime.expired() || data.textInput != expectedInput)
            return;
        expectedInput->SetVisible(false);
        if (lifetime.expired() || expectedInputObjectLifetime.expired() || data.textInput != expectedInput)
            return;
    }
    data.textInput = input;
    if (input != nullptr)
    {
        data.textInputObjectLifetime = input->LifetimeToken();
        data.textInputLifetime = std::make_shared<int>();
        const std::weak_ptr<int> inputLifetime = data.textInputLifetime;
        input->SetSubmitHandler([inputLifetime, this]
        {
            if (!inputLifetime.lock())
                return;
            if (spreadsheet_->renamingSheet)
            {
                CommitWorksheetRename();
                return;
            }
            if (!Editing())
                return;
            core::Event event;
            event.type = core::EventType::KeyDown;
            event.key = core::key::Enter;
            OnKeyboardEvent(event);
        });
        if (lifetime.expired() || data.textInput != input || !data.HasTextInput())
            return;
        input->SetFocusLostHandler([inputLifetime, this]
        {
            if (inputLifetime.lock())
            {
                if (spreadsheet_->renamingSheet)
                    CommitWorksheetRename();
                else
                    CommitEdit();
            }
        });
    }
}

bool DuiSpreadsheet::Editing() const { return spreadsheet_->editing; }

bool DuiSpreadsheet::SetSelectionCursor(DuiCellAddress cell)
{
    auto& data = *spreadsheet_;
    if (!data.HasCells() || data.selectionMode != SelectionMode::Cells
        || !data.selection.Contains(cell))
        return false;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const bool changed = data.active != cell;
    data.active = cell;
    data.anchor = cell;
    if (changed && data.activeChanged)
    {
        const auto handler = data.activeChanged;
        handler(cell);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.active != cell)
            return false;
    }
    EnsureVisible(cell);
    return !lifetime.expired() && data.HasModelContext(expectedModel, expectedSheet)
        && data.active == cell;
}

bool DuiSpreadsheet::BeginEdit()
{
    auto& data = *spreadsheet_;
    if (!data.HasCells() || !data.HasTextInput() || data.editing || data.renamingSheet)
        return false;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    const DuiCellRange selected = data.selection.Normalized();
    if (data.selectionMode == SelectionMode::Cells && selected.first != selected.last
        && !SetSelectionCursor(data.anchor))
        return false;
    if (lifetime.expired())
        return false;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const DuiCellAddress cell = data.active;
    ui::DuiTextInput* const expectedInput = data.textInput;
    const auto clearEditState = [&data]
    {
        data.editing = false;
        data.editRow = data.editColumn = -1;
        data.editChanges.clear();
    };
    const auto clearEditStateIfAlive = [&]
    {
        if (!lifetime.expired())
            clearEditState();
    };
    const auto contextMatches = [&]
    {
        return !lifetime.expired() && data.HasModelContext(expectedModel, expectedSheet)
            && data.textInput == expectedInput && data.HasTextInput();
    };
    if (expectedModel->IsCellReadOnly(expectedSheet, cell))
        return false;
    if (!contextMatches())
        return false;
    const std::string text = expectedModel->CellText(expectedSheet, cell);
    if (!contextMatches())
        return false;
    data.editRow = cell.row;
    data.editColumn = cell.column;
    data.editChanges.clear();
    expectedInput->SetText(text);
    if (!contextMatches())
    {
        clearEditStateIfAlive();
        return false;
    }
    expectedInput->SetOptions({false, false, false, false, 0});
    if (!contextMatches())
    {
        clearEditStateIfAlive();
        return false;
    }
    expectedInput->SetBorderVisible(false);
    if (!contextMatches())
    {
        clearEditStateIfAlive();
        return false;
    }
    expectedInput->SetVisible(true);
    if (!contextMatches())
    {
        clearEditStateIfAlive();
        return false;
    }
    data.editing = true;
    PlaceTextInput();
    if (!contextMatches())
    {
        if (!lifetime.expired() && data.textInput == expectedInput && data.HasTextInput())
        {
            clearEditState();
            expectedInput->SetVisible(false);
        }
        return false;
    }
    expectedInput->Focus();
    if (!contextMatches())
    {
        if (!lifetime.expired() && data.textInput == expectedInput && data.HasTextInput())
        {
            clearEditState();
            expectedInput->SetVisible(false);
        }
        return false;
    }
    if (!data.editing || data.editRow != cell.row || data.editColumn != cell.column)
        return false;
    return true;
}

void DuiSpreadsheet::CommitEdit()
{
    auto& data = *spreadsheet_;
    if (!data.editing || data.model == nullptr)
        return;
    if (!data.HasTextInput())
    {
        data.editing = false;
        data.editRow = data.editColumn = -1;
        return;
    }
    const DuiCellAddress cell{data.editRow, data.editColumn};
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    ui::DuiTextInput* const expectedInput = data.textInput;
    const std::string before = expectedModel->CellText(expectedSheet, cell);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
        || data.textInput != expectedInput || !data.HasTextInput())
        return;
    const std::string after = expectedInput->Text();
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
        || data.textInput != expectedInput || !data.HasTextInput())
        return;
    data.editing = false;
    data.editRow = data.editColumn = -1;
    expectedInput->SetVisible(false);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
        || data.textInput != expectedInput || !data.HasTextInput())
        return;
    if (before == after)
        return;
    const bool declaredReadOnly = expectedModel->IsCellReadOnly(expectedSheet, cell);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
        || data.textInput != expectedInput || !data.HasTextInput())
        return;
    DuiWorkbookWriteResult result;
    if (declaredReadOnly)
        result = ReadOnlyWriteResult();
    else
    {
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.textInput != expectedInput || !data.HasTextInput())
            return;
        Impl::ModelUpdateScope update(data);
        result = data.model->SetCellTextWithResult(data.sheet, cell, after);
    }
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
        || data.textInput != expectedInput || !data.HasTextInput())
        return;
    if (!result)
    {
        data.ReportWriteError(expectedSheet, cell, std::move(result));
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.textInput != expectedInput || !data.HasTextInput())
            return;
        if (declaredReadOnly)
            return;
        data.editing = true;
        data.editRow = cell.row;
        data.editColumn = cell.column;
        expectedInput->SetVisible(true);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.textInput != expectedInput || !data.HasTextInput())
            return;
        PlaceTextInput();
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.textInput != expectedInput || !data.HasTextInput())
            return;
        expectedInput->Focus();
        return;
    }
    data.RecordUndo({CellChange{ChangeKind::Cell, cell, before, after}});
}

void DuiSpreadsheet::CancelEdit()
{
    auto& data = *spreadsheet_;
    if (!data.editing)
        return;
    data.editing = false;
    data.editRow = data.editColumn = -1;
    if (data.HasTextInput())
        data.textInput->SetVisible(false);
}

void DuiSpreadsheet::Undo()
{
    ApplyHistory(true);
}

void DuiSpreadsheet::Redo()
{
    ApplyHistory(false);
}

void DuiSpreadsheet::ApplyHistory(bool undoOperation)
{
    auto& data = *spreadsheet_;
    auto& source = undoOperation ? data.undo : data.redo;
    auto& destination = undoOperation ? data.redo : data.undo;
    if (source.empty() || data.model == nullptr)
        return;
    auto changes = std::move(source.back());
    source.pop_back();
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    if (std::all_of(changes.begin(), changes.end(), [](const CellChange& change)
        { return change.kind == ChangeKind::Cell; }))
    {
        const bool succeeded = data.ApplyCellChanges(changes, undoOperation);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return;
        if (!succeeded)
        {
            source.push_back(std::move(changes));
            return;
        }
        destination.push_back(std::move(changes));
        return;
    }
    bool succeeded = true;
    bool metricsChanged{};
    {
        Impl::ModelUpdateScope update(data);
        for (const auto& change : changes)
        {
            bool applied{};
            if (change.kind == ChangeKind::Cell)
            {
                const bool readOnly = expectedModel->IsCellReadOnly(expectedSheet, change.address);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return;
                DuiWorkbookWriteResult result = readOnly
                    ? ReadOnlyWriteResult()
                    : expectedModel->SetCellTextWithResult(
                        expectedSheet, change.address, undoOperation ? change.before : change.after);
                applied = static_cast<bool>(result);
                if (!result)
                    data.ReportWriteError(expectedSheet, change.address, std::move(result));
            }
            else if (change.kind == ChangeKind::RowHeight)
            {
                DuiWorkbookWriteResult result = expectedModel->SetRowHeightWithResult(
                    expectedSheet, change.address.row,
                    undoOperation ? change.beforeSize : change.afterSize);
                applied = static_cast<bool>(result);
                if (!result)
                    data.ReportWriteError(expectedSheet, change.address, std::move(result),
                                          DuiSpreadsheetWriteOperation::RowHeight);
                if (applied)
                {
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return;
                    const int height = expectedModel->RowHeight(expectedSheet, change.address.row);
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return;
                    data.rows.SetSize(change.address.row, height);
                    metricsChanged = true;
                }
            }
            else
            {
                DuiWorkbookWriteResult result = expectedModel->SetColumnWidthWithResult(
                    expectedSheet, change.address.column,
                    undoOperation ? change.beforeSize : change.afterSize);
                applied = static_cast<bool>(result);
                if (!result)
                    data.ReportWriteError(expectedSheet, change.address, std::move(result),
                                          DuiSpreadsheetWriteOperation::ColumnWidth);
                if (applied)
                {
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return;
                    const int width = expectedModel->ColumnWidth(expectedSheet, change.address.column);
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return;
                    data.columns.SetSize(change.address.column, width);
                    metricsChanged = true;
                }
            }
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return;
            succeeded = applied && succeeded;
        }
    }
    if (metricsChanged)
    {
        Layout(Bounds());
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return;
    }
    if (!succeeded)
    {
        source.push_back(std::move(changes));
        return;
    }
    destination.push_back(std::move(changes));
}

bool DuiSpreadsheet::OnKeyboardEvent(const core::Event& event)
{
    auto& data = *spreadsheet_;
    if (event.type == core::EventType::TextInput && !Editing())
    {
        if (BeginEdit())
        {
            if (data.HasTextInput())
                data.textInput->SetText(event.text);
            return true;
        }
    }
    if (event.type != core::EventType::KeyDown)
        return false;
    const bool control = (event.modifiers & core::modifier::Control) != 0;
    const bool shift = (event.modifiers & core::modifier::Shift) != 0;
    if (control && event.key == 'F' && data.findRequested)
    {
        const auto handler = data.findRequested;
        handler();
        return true;
    }
    if (control && event.key == 'H' && data.replaceRequested)
    {
        const auto handler = data.replaceRequested;
        handler();
        return true;
    }
    if (control && event.key == 'C' && data.clipboard != nullptr)
    {
        const std::weak_ptr<int> lifetime = data.objectLifetime;
        IDuiWorkbookModel* const expectedModel = data.model;
        const DuiWorksheetId expectedSheet = data.sheet;
        ui::DuiClipboard* const expectedClipboard = data.clipboard;
        const DuiCellRange range = data.selection.Normalized();
        std::string text;
        for (int row = range.first.row; row <= range.last.row; ++row)
        {
            if (row != range.first.row)
                text += "\r\n";
            for (int column = range.first.column; column <= range.last.column; ++column)
            {
                if (column != range.first.column)
                    text.push_back('\t');
                const std::string cellText = expectedModel->CellText(expectedSheet, {row, column});
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
                    || data.clipboard != expectedClipboard)
                    return true;
                text += cellText;
            }
        }
        return expectedClipboard->SetText(std::move(text));
    }
    if (control && event.key == 'X' && data.clipboard != nullptr)
    {
        const std::weak_ptr<int> lifetime = data.objectLifetime;
        IDuiWorkbookModel* const expectedModel = data.model;
        const DuiWorksheetId expectedSheet = data.sheet;
        core::Event copyEvent;
        copyEvent.type = core::EventType::KeyDown;
        copyEvent.key = 'C';
        copyEvent.modifiers = event.modifiers;
        const bool copied = OnEvent(copyEvent);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return true;
        if (copied)
        {
            const DuiCellRange range = data.selection.Normalized();
            std::vector<CellChange> changes;
            bool succeeded = true;
            {
                Impl::ModelUpdateScope update(data);
                for (int row = range.first.row; row <= range.last.row; ++row)
                    for (int column = range.first.column; column <= range.last.column; ++column)
                    {
                        const DuiCellAddress cell{row, column};
                        const std::string before = expectedModel->CellText(expectedSheet, cell);
                        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                            return true;
                        if (!before.empty())
                        {
                            const bool readOnly = expectedModel->IsCellReadOnly(expectedSheet, cell);
                            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                                return true;
                            DuiWorkbookWriteResult result = readOnly
                                ? ReadOnlyWriteResult()
                                : expectedModel->SetCellTextWithResult(expectedSheet, cell, {});
                            const bool cleared = static_cast<bool>(result);
                            if (!result)
                                data.ReportWriteError(expectedSheet, cell, std::move(result));
                            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                                return true;
                            if (cleared)
                                changes.push_back({ChangeKind::Cell, cell, before, {}});
                            else
                                succeeded = false;
                        }
                    }
            }
            data.RecordUndo(std::move(changes));
            return succeeded;
        }
    }
    if (control && event.key == 'V' && data.clipboard != nullptr)
    {
        const std::weak_ptr<int> lifetime = data.objectLifetime;
        IDuiWorkbookModel* const expectedModel = data.model;
        const DuiWorksheetId expectedSheet = data.sheet;
        ui::DuiClipboard* const expectedClipboard = data.clipboard;
        const auto value = expectedClipboard->GetText();
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.clipboard != expectedClipboard)
            return true;
        if (!value)
            return false;
        const int rowCount = std::max(0, expectedModel->RowCount(expectedSheet));
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return true;
        const int columnCount = std::max(0, expectedModel->ColumnCount(expectedSheet));
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return true;
        std::vector<CellChange> changes;
        bool succeeded = true;
        std::stringstream rows(*value);
        std::string line;
        int row = data.selection.first.row;
        {
            Impl::ModelUpdateScope update(data);
            while (std::getline(rows, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                std::stringstream columns(line);
                std::string cellText;
                int column = data.selection.first.column;
                while (std::getline(columns, cellText, '\t'))
                {
                    if (row < rowCount && column < columnCount)
                    {
                        const DuiCellAddress cell{row, column};
                        const std::string before = expectedModel->CellText(expectedSheet, cell);
                        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                            return true;
                        if (before != cellText)
                        {
                            const bool readOnly = expectedModel->IsCellReadOnly(expectedSheet, cell);
                            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                                return true;
                            DuiWorkbookWriteResult result = readOnly
                                ? ReadOnlyWriteResult()
                                : expectedModel->SetCellTextWithResult(expectedSheet, cell, cellText);
                            const bool changed = static_cast<bool>(result);
                            if (!result)
                                data.ReportWriteError(expectedSheet, cell, std::move(result));
                            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                                return true;
                            if (changed)
                                changes.push_back({ChangeKind::Cell, cell, before, cellText});
                            else
                                succeeded = false;
                        }
                    }
                    ++column;
                }
                ++row;
            }
        }
        data.RecordUndo(std::move(changes));
        return succeeded;
    }
    if (control && event.key == 'Z') { Undo(); return true; }
    if (control && event.key == 'Y') { Redo(); return true; }
    if (event.key == core::key::Function2)
        return BeginEdit();
    if (Editing())
    {
        if (event.key == core::key::Enter)
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            const DuiCellAddress editedCell{data.editRow, data.editColumn};
            const DuiCellRange selected = data.selection.Normalized();
            const bool advanceWithinSelection = data.selectionMode == SelectionMode::Cells
                && selected.first != selected.last && selected.Contains(editedCell);
            CommitEdit();
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet) || Editing())
                return true;
            if (advanceWithinSelection && data.selectionMode == SelectionMode::Cells
                && data.selection.Normalized() == selected)
            {
                DuiCellAddress next = editedCell;
                if (next.row < selected.last.row)
                    ++next.row;
                else if (next.column < selected.last.column)
                    next = {selected.first.row, next.column + 1};
                else
                    next = selected.first;
                SetSelectionCursor(next);
                return true;
            }
            const int rows = expectedModel->RowCount(expectedSheet);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            SetActiveCell({std::min(rows - 1, data.active.row + 1), data.active.column});
            return true;
        }
        if (event.key == core::key::Escape) { CancelEdit(); return true; }
        if (event.key == core::key::Tab)
        {
            const std::weak_ptr<int> lifetime = data.objectLifetime;
            IDuiWorkbookModel* const expectedModel = data.model;
            const DuiWorksheetId expectedSheet = data.sheet;
            CommitEdit();
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet) || Editing())
                return true;
            const int columns = expectedModel->ColumnCount(expectedSheet);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return true;
            SetActiveCell({data.active.row, std::min(columns - 1, data.active.column + (shift ? -1 : 1))});
            return true;
        }
    }
    const std::weak_ptr<int> navigationLifetime = data.objectLifetime;
    IDuiWorkbookModel* const navigationModel = data.model;
    const DuiWorksheetId navigationSheet = data.sheet;
    DuiCellAddress next = data.active;
    if (event.key == core::key::Left) next.column = std::max(0, next.column - 1);
    else if (event.key == core::key::Right)
    {
        const int columns = navigationModel->ColumnCount(navigationSheet);
        if (navigationLifetime.expired() || !data.HasModelContext(navigationModel, navigationSheet))
            return true;
        next.column = std::min(columns - 1, next.column + 1);
    }
    else if (event.key == core::key::Up) next.row = std::max(0, next.row - 1);
    else if (event.key == core::key::Down)
    {
        const int rows = navigationModel->RowCount(navigationSheet);
        if (navigationLifetime.expired() || !data.HasModelContext(navigationModel, navigationSheet))
            return true;
        next.row = std::min(rows - 1, next.row + 1);
    }
    else if (event.key == core::key::Home) next.column = 0;
    else if (event.key == core::key::End)
    {
        const int columns = navigationModel->ColumnCount(navigationSheet);
        if (navigationLifetime.expired() || !data.HasModelContext(navigationModel, navigationSheet))
            return true;
        next.column = columns - 1;
    }
    else if (event.key == core::key::PageUp) next.row = std::max(0, next.row - std::max(1, data.viewport.Height() / kDefaultRowHeight));
    else if (event.key == core::key::PageDown)
    {
        const int rows = navigationModel->RowCount(navigationSheet);
        if (navigationLifetime.expired() || !data.HasModelContext(navigationModel, navigationSheet))
            return true;
        next.row = std::min(rows - 1, next.row + std::max(1, data.viewport.Height() / kDefaultRowHeight));
    }
    else if (event.key == core::key::Delete)
    {
        const DuiCellRange range = data.selection.Normalized();
        std::vector<CellChange> changes;
        bool succeeded = true;
        const std::weak_ptr<int> lifetime = data.objectLifetime;
        IDuiWorkbookModel* const expectedModel = data.model;
        const DuiWorksheetId expectedSheet = data.sheet;
        {
            Impl::ModelUpdateScope update(data);
            for (int row = range.first.row; row <= range.last.row; ++row)
                for (int column = range.first.column; column <= range.last.column; ++column)
                {
                    const DuiCellAddress cell{row, column};
                    const std::string before = expectedModel->CellText(expectedSheet, cell);
                    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                        return true;
                    if (!before.empty())
                    {
                        const bool readOnly = expectedModel->IsCellReadOnly(expectedSheet, cell);
                        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                            return true;
                        DuiWorkbookWriteResult result = readOnly
                            ? ReadOnlyWriteResult()
                            : expectedModel->SetCellTextWithResult(expectedSheet, cell, {});
                        const bool cleared = static_cast<bool>(result);
                        if (!result)
                            data.ReportWriteError(expectedSheet, cell, std::move(result));
                        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                            return true;
                        if (cleared)
                            changes.push_back({ChangeKind::Cell, cell, before, {}});
                        else
                            succeeded = false;
                    }
                }
        }
        data.RecordUndo(std::move(changes));
        return succeeded;
    }
    else return false;
    SetActiveCell(next, shift);
    return true;
}

} // namespace ysDui::controls::list
