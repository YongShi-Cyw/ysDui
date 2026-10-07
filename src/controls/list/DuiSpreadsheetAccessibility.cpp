/**
 * 文件名：DuiSpreadsheetAccessibility.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：实现工作表可访问性语义与操作。
 */
#include "DuiSpreadsheetInternal.hpp"

#include <string>
#include <utility>

namespace ysDui::controls::list {

core::DuiAccessibilityData DuiSpreadsheet::CreateAccessibilityData() const
{
    auto accessibility = core::Control::CreateAccessibilityData();
    accessibility.role = core::DuiAccessibilityRole::Table;
    accessibility.keyboardFocusable = true;
    accessibility.name = Name().empty() ? "Spreadsheet" : Name();
    if (!spreadsheet_->HasSheet())
        return accessibility;

    accessibility.patterns = core::DuiAccessibilityPattern::Grid
        | core::DuiAccessibilityPattern::Table;
    if (!spreadsheet_->HasCells())
        return accessibility;

    const auto& data = *spreadsheet_;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const DuiCellAddress expectedActive = data.active;
    const DuiCellRange selection = data.selection.Normalized();
    const auto addressName = [](DuiCellAddress cell)
    {
        return ColumnName(cell.column) + std::to_string(cell.row + 1);
    };
    const DuiCellPresentation presentation = expectedModel->CellPresentation(expectedSheet, expectedActive);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
        || data.active != expectedActive)
        return accessibility;
    const std::string worksheetName = expectedModel->WorksheetName(expectedSheet);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
        || data.active != expectedActive)
        return accessibility;
    accessibility.value = presentation.displayText;
    accessibility.readOnly = expectedModel->IsCellReadOnly(expectedSheet, expectedActive);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
        || data.active != expectedActive)
        return accessibility;
    accessibility.description = worksheetName + ", active cell "
        + addressName(expectedActive) + ", selected " + addressName(selection.first) + ":"
        + addressName(selection.last);
    accessibility.patterns = accessibility.patterns
        | core::DuiAccessibilityPattern::Value
        | core::DuiAccessibilityPattern::Selection;
    return accessibility;
}

bool DuiSpreadsheet::PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                                std::string_view value)
{
    if (action != core::DuiAccessibilityAction::SetValue || !Enabled() || !spreadsheet_->HasCells())
        return false;

    const std::weak_ptr<int> lifetime = spreadsheet_->objectLifetime;
    CancelEdit();
    if (lifetime.expired())
        return false;
    auto& data = *spreadsheet_;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const DuiCellAddress expectedCell = data.active;
    const std::string before = expectedModel->CellText(expectedSheet, expectedCell);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
        || data.active != expectedCell)
        return false;
    const std::string after(value);
    if (before == after)
        return true;
    if (expectedModel->IsCellReadOnly(expectedSheet, expectedCell))
        return false;
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
        || data.active != expectedCell)
        return false;
    DuiWorkbookWriteResult result;
    {
        Impl::ModelUpdateScope update(data);
        result = data.model->SetCellTextWithResult(data.sheet, expectedCell, after);
    }
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return true;
    if (!result)
    {
        data.ReportWriteError(expectedSheet, expectedCell, std::move(result));
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return true;
        return false;
    }
    data.RecordUndo({CellChange{ChangeKind::Cell, expectedCell, before, after}});
    return true;
}

} // namespace ysDui::controls::list
