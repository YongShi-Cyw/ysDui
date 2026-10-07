/**
 * 文件名：DuiSpreadsheetOperations.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：实现工作表查找、替换、排序、筛选、自动尺寸和右键菜单。
 */
#include "DuiSpreadsheetInternal.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"

namespace ysDui::controls::list {
namespace {

[[nodiscard]] DuiWorkbookOperationResult InvalidOperation(
    DuiWorkbookOperationErrorCode code, std::string message)
{
    return {code, std::move(message)};
}

[[nodiscard]] std::optional<DuiCellRange> ResolveRange(
    int rows, int columns, std::optional<DuiCellRange> requested)
{
    if (rows <= 0 || columns <= 0)
        return std::nullopt;
    DuiCellRange range = requested.value_or(DuiCellRange{{0, 0}, {rows - 1, columns - 1}})
        .Normalized();
    if (!range.first.Valid() || range.last.row >= rows || range.last.column >= columns)
        return std::nullopt;
    return range;
}

[[nodiscard]] DuiWorkbookOperationResult FromWriteResult(const DuiWorkbookWriteResult& result)
{
    if (result)
        return {};
    const DuiWorkbookOperationErrorCode code = result.code == DuiWorkbookWriteErrorCode::InvalidWorksheet
        ? DuiWorkbookOperationErrorCode::InvalidWorksheet
        : result.code == DuiWorkbookWriteErrorCode::InvalidCell
            ? DuiWorkbookOperationErrorCode::InvalidArgument
            : DuiWorkbookOperationErrorCode::Rejected;
    return {code, result.message};
}

[[nodiscard]] core::Event CommandEvent(unsigned int key)
{
    core::Event event;
    event.type = core::EventType::KeyDown;
    event.key = key;
    event.modifiers = core::modifier::Control;
    return event;
}

} // namespace

void DuiSpreadsheet::SetPopupContext(ui::IUiHostFactory& factory, ui::HostRef owner)
{
    spreadsheet_->popupFactory = &factory;
    spreadsheet_->popupOwner = std::move(owner);
}

void DuiSpreadsheet::SetTextMeasurer(render::DuiTextMeasurer* measurer)
{
    spreadsheet_->textMeasurer = measurer;
    if (spreadsheet_->contextMenu)
        spreadsheet_->contextMenu->SetTextMeasurer(measurer);
    if (spreadsheet_->sheetMenu)
        spreadsheet_->sheetMenu->SetTextMeasurer(measurer);
    if (spreadsheet_->sheetContextMenu)
        spreadsheet_->sheetContextMenu->SetTextMeasurer(measurer);
}

void DuiSpreadsheet::SetFindRequestedHandler(std::function<void()> handler)
{
    spreadsheet_->findRequested = std::move(handler);
}

void DuiSpreadsheet::SetReplaceRequestedHandler(std::function<void()> handler)
{
    spreadsheet_->replaceRequested = std::move(handler);
}

void DuiSpreadsheet::SetWorkbookOperationErrorHandler(
    std::function<void(const DuiWorkbookOperationResult&)> handler)
{
    spreadsheet_->workbookOperationError = std::move(handler);
}

DuiWorkbookFindResult DuiSpreadsheet::FindNext(
    std::string_view query, DuiWorkbookFindOptions options, std::optional<DuiCellRange> range)
{
    auto& data = *spreadsheet_;
    if (query.empty())
        return {InvalidOperation(DuiWorkbookOperationErrorCode::InvalidArgument,
                                 "Search text must not be empty"), std::nullopt};
    if (!data.HasCells())
        return {InvalidOperation(DuiWorkbookOperationErrorCode::InvalidWorksheet,
                                 "Spreadsheet has no active worksheet cells"), std::nullopt};
    if (!HasCapability(data.model->Capabilities(), DuiWorkbookCapability::Find))
    {
        DuiWorkbookFindResult result{UnsupportedWorkbookOperation(), std::nullopt};
        data.ReportOperationError(result.operation);
        return result;
    }
    const auto resolvedRange = ResolveRange(data.Rows(), data.Columns(), range);
    if (!resolvedRange)
        return {InvalidOperation(DuiWorkbookOperationErrorCode::InvalidRange,
                                 "Search range is outside the active worksheet"), std::nullopt};

    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    DuiWorkbookFindResult result = expectedModel->FindCell(
        expectedSheet, query, data.active.Valid() ? std::optional<DuiCellAddress>{data.active} : std::nullopt,
        resolvedRange, options);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return result;
    if (!result.operation)
    {
        data.ReportOperationError(result.operation);
        return result;
    }
    if (result.cell && resolvedRange->Contains(*result.cell))
        SetActiveCell(*result.cell);
    else if (result.cell)
    {
        result.operation = InvalidOperation(DuiWorkbookOperationErrorCode::InvalidRange,
                                            "Workbook model returned a cell outside the search range");
        result.cell.reset();
        data.ReportOperationError(result.operation);
    }
    return result;
}

DuiWorkbookReplaceResult DuiSpreadsheet::ReplaceText(
    std::string_view query, std::string_view replacement, bool replaceAll,
    DuiWorkbookFindOptions options, std::optional<DuiCellRange> range)
{
    auto& data = *spreadsheet_;
    if (query.empty())
        return {InvalidOperation(DuiWorkbookOperationErrorCode::InvalidArgument,
                                 "Search text must not be empty"), 0, std::nullopt};
    if (!data.HasCells())
        return {InvalidOperation(DuiWorkbookOperationErrorCode::InvalidWorksheet,
                                 "Spreadsheet has no active worksheet cells"), 0, std::nullopt};
    if (!HasCapability(data.model->Capabilities(), DuiWorkbookCapability::Replace))
    {
        DuiWorkbookReplaceResult result{UnsupportedWorkbookOperation(), 0, std::nullopt};
        data.ReportOperationError(result.operation);
        return result;
    }
    const auto resolvedRange = ResolveRange(data.Rows(), data.Columns(), range);
    if (!resolvedRange)
        return {InvalidOperation(DuiWorkbookOperationErrorCode::InvalidRange,
                                 "Replace range is outside the active worksheet"), 0, std::nullopt};

    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    DuiWorkbookReplaceResult result;
    {
        Impl::ModelUpdateScope update(data);
        result = expectedModel->ReplaceText(expectedSheet, query, replacement, resolvedRange, options, replaceAll);
    }
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return result;
    if (!result.operation)
    {
        data.ReportOperationError(result.operation);
        return result;
    }
    if (result.replacedCount == 0)
        return result;
    const std::optional<DuiCellAddress> firstChanged = result.firstChanged;
    ReloadFromModel();
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return result;
    if (firstChanged && firstChanged->row < data.Rows() && firstChanged->column < data.Columns())
        SetActiveCell(*firstChanged);
    return result;
}

DuiWorkbookOperationResult DuiSpreadsheet::ApplySort(
    std::vector<DuiWorkbookSortKey> keys, std::optional<DuiCellRange> range)
{
    auto& data = *spreadsheet_;
    if (!data.HasCells())
        return InvalidOperation(DuiWorkbookOperationErrorCode::InvalidWorksheet,
                                "Spreadsheet has no active worksheet cells");
    if (!HasCapability(data.model->Capabilities(), DuiWorkbookCapability::Sort))
    {
        DuiWorkbookOperationResult result = UnsupportedWorkbookOperation();
        data.ReportOperationError(result);
        return result;
    }
    const auto resolvedRange = ResolveRange(data.Rows(), data.Columns(), range);
    if (!resolvedRange)
        return InvalidOperation(DuiWorkbookOperationErrorCode::InvalidRange,
                                "Sort range is outside the active worksheet");
    if (std::any_of(keys.begin(), keys.end(), [&data](const DuiWorkbookSortKey& key)
        { return key.column < 0 || key.column >= data.Columns(); }))
        return InvalidOperation(DuiWorkbookOperationErrorCode::InvalidArgument,
                                "Sort key column is outside the active worksheet");

    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    DuiWorkbookOperationResult result;
    {
        Impl::ModelUpdateScope update(data);
        result = expectedModel->ApplySort(expectedSheet, *resolvedRange, keys);
    }
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return result;
    if (!result)
    {
        data.ReportOperationError(result);
        return result;
    }
    data.sortKeys = std::move(keys);
    ReloadFromModel();
    return result;
}

DuiWorkbookOperationResult DuiSpreadsheet::SetFilters(std::vector<DuiWorkbookFilter> filters)
{
    auto& data = *spreadsheet_;
    if (!data.HasSheet())
        return InvalidOperation(DuiWorkbookOperationErrorCode::InvalidWorksheet,
                                "Spreadsheet has no active worksheet");
    if (!HasCapability(data.model->Capabilities(), DuiWorkbookCapability::Filter))
    {
        DuiWorkbookOperationResult result = UnsupportedWorkbookOperation();
        data.ReportOperationError(result);
        return result;
    }
    if (std::any_of(filters.begin(), filters.end(), [&data](const DuiWorkbookFilter& filter)
        { return filter.column < 0 || filter.column >= data.Columns(); }))
        return InvalidOperation(DuiWorkbookOperationErrorCode::InvalidArgument,
                                "Filter column is outside the active worksheet");

    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    DuiWorkbookOperationResult result;
    {
        Impl::ModelUpdateScope update(data);
        result = expectedModel->SetFilters(expectedSheet, filters);
    }
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return result;
    if (!result)
    {
        data.ReportOperationError(result);
        return result;
    }
    data.filters = std::move(filters);
    ReloadFromModel();
    return result;
}

DuiWorkbookOperationResult DuiSpreadsheet::ClearFilters()
{
    return SetFilters({});
}

DuiWorkbookOperationResult DuiSpreadsheet::AutoFitRow(int row)
{
    auto& data = *spreadsheet_;
    if (!data.HasCells() || row < 0 || row >= data.Rows())
        return InvalidOperation(DuiWorkbookOperationErrorCode::InvalidArgument,
                                "Row is outside the active worksheet");
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    std::optional<int> preferred = expectedModel->PreferredRowHeight(expectedSheet, row);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return InvalidOperation(DuiWorkbookOperationErrorCode::Rejected, "Workbook context changed");
    int pixels = preferred.value_or(kDefaultRowHeight);
    if (!preferred && data.textMeasurer)
    {
        const render::DuiTextStyle style{{30, 30, 30, 255}, Theme().DefaultFontFamily(),
                                         Theme().DefaultFontPointSize(), false, false};
        for (int column = 0; column < data.Columns(); ++column)
        {
            const std::string text = expectedModel->CellPresentation(expectedSheet, {row, column}).displayText;
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return InvalidOperation(DuiWorkbookOperationErrorCode::Rejected, "Workbook context changed");
            const int height = data.textMeasurer->MeasureText(
                text.empty() ? "Mg" : text, style, {}).size.height + 4;
            pixels = std::max(pixels, height);
        }
    }
    pixels = ClampDimension(pixels);
    const int before = data.RowSize(row);
    DuiWorkbookWriteResult writeResult;
    {
        Impl::ModelUpdateScope update(data);
        writeResult = expectedModel->SetRowHeightWithResult(expectedSheet, row, pixels);
    }
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return FromWriteResult(writeResult);
    if (!writeResult)
    {
        data.ReportWriteError(expectedSheet, {row, 0}, writeResult, DuiSpreadsheetWriteOperation::RowHeight);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return FromWriteResult(writeResult);
    }
    DuiWorkbookOperationResult result = FromWriteResult(writeResult);
    if (!result)
    {
        data.ReportOperationError(result);
        return result;
    }
    pixels = ClampDimension(expectedModel->RowHeight(expectedSheet, row));
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return result;
    data.rows.SetSize(row, pixels);
    if (before != pixels)
        data.RecordUndo({CellChange{ChangeKind::RowHeight, {row, 0}, {}, {}, before, pixels}});
    Layout(Bounds());
    return result;
}

DuiWorkbookOperationResult DuiSpreadsheet::AutoFitColumn(int column)
{
    auto& data = *spreadsheet_;
    if (!data.HasCells() || column < 0 || column >= data.Columns())
        return InvalidOperation(DuiWorkbookOperationErrorCode::InvalidArgument,
                                "Column is outside the active worksheet");
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    std::optional<int> preferred = expectedModel->PreferredColumnWidth(expectedSheet, column);
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return InvalidOperation(DuiWorkbookOperationErrorCode::Rejected, "Workbook context changed");
    const render::DuiTextStyle style{{30, 30, 30, 255}, Theme().DefaultFontFamily(),
                                     Theme().DefaultFontPointSize(), false, false};
    const auto textWidth = [&data, &style](std::string_view text)
    {
        return data.textMeasurer
            ? data.textMeasurer->MeasureText(text, style, {}).size.width
            : static_cast<int>(text.size()) * std::max(1, style.pointSize);
    };
    int pixels = preferred.value_or(textWidth(ColumnName(column)) + 12);
    if (!preferred)
    {
        for (int row = 0; row < data.Rows(); ++row)
        {
            const std::string text = expectedModel->CellPresentation(expectedSheet, {row, column}).displayText;
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return InvalidOperation(DuiWorkbookOperationErrorCode::Rejected, "Workbook context changed");
            pixels = std::max(pixels, textWidth(text) + 12);
        }
    }
    pixels = ClampDimension(pixels);
    const int before = data.ColumnSize(column);
    DuiWorkbookWriteResult writeResult;
    {
        Impl::ModelUpdateScope update(data);
        writeResult = expectedModel->SetColumnWidthWithResult(expectedSheet, column, pixels);
    }
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return FromWriteResult(writeResult);
    if (!writeResult)
    {
        data.ReportWriteError(expectedSheet, {0, column}, writeResult, DuiSpreadsheetWriteOperation::ColumnWidth);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return FromWriteResult(writeResult);
    }
    DuiWorkbookOperationResult result = FromWriteResult(writeResult);
    if (!result)
    {
        data.ReportOperationError(result);
        return result;
    }
    pixels = ClampDimension(expectedModel->ColumnWidth(expectedSheet, column));
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return result;
    data.columns.SetSize(column, pixels);
    if (before != pixels)
        data.RecordUndo({CellChange{ChangeKind::ColumnWidth, {0, column}, {}, {}, before, pixels}});
    Layout(Bounds());
    return result;
}

std::vector<DuiWorkbookSortKey> DuiSpreadsheet::SortKeys() const
{
    return spreadsheet_->sortKeys;
}

std::vector<DuiWorkbookFilter> DuiSpreadsheet::Filters() const
{
    return spreadsheet_->filters;
}

bool DuiSpreadsheet::ShowContextMenu(core::Point point)
{
    auto& data = *spreadsheet_;
    if (!data.popupFactory || !data.contextMenu || !data.HasCells())
        return false;
    const int row = RowFromPoint(data, point);
    const int column = ColumnFromPoint(data, point);
    if (row >= 0 && column >= 0 && !data.selection.Contains({row, column}))
        SetActiveCell({row, column});

    const DuiWorkbookCapability capabilities = data.model->Capabilities();
    const bool canSort = HasCapability(capabilities, DuiWorkbookCapability::Sort);
    const bool canFilter = HasCapability(capabilities, DuiWorkbookCapability::Filter);
    DuiMenu& menu = *data.contextMenu;
    menu.ClearItems();
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::Cut), "Cut");
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::Copy), "Copy");
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::Paste), "Paste");
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::ClearContents), "Clear contents");
    if (data.findRequested || data.replaceRequested)
    {
        menu.AddSeparator();
        if (data.findRequested)
            menu.AddItem(static_cast<std::uint32_t>(ContextCommand::Find), "Find...");
        if (data.replaceRequested)
            menu.AddItem(static_cast<std::uint32_t>(ContextCommand::Replace), "Replace...");
    }
    if (canSort || canFilter)
    {
        menu.AddSeparator();
        if (canSort)
        {
            menu.AddItem(static_cast<std::uint32_t>(ContextCommand::SortAscending), "Sort ascending");
            menu.AddItem(static_cast<std::uint32_t>(ContextCommand::SortDescending), "Sort descending");
            if (!data.sortKeys.empty())
                menu.AddItem(static_cast<std::uint32_t>(ContextCommand::ClearSort), "Clear sort");
        }
        if (canFilter)
        {
            menu.AddItem(static_cast<std::uint32_t>(ContextCommand::FilterByCell), "Filter by cell value");
            if (!data.filters.empty())
                menu.AddItem(static_cast<std::uint32_t>(ContextCommand::ClearFilters), "Clear filters");
        }
    }
    menu.AddSeparator();
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::FreezePanes), "Freeze panes");
    if (data.FrozenRowCount() > 0 || data.FrozenColumnCount() > 0)
        menu.AddItem(static_cast<std::uint32_t>(ContextCommand::UnfreezePanes), "Unfreeze panes");
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::AutoFitColumn), "Auto fit column");
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::AutoFitRow), "Auto fit row");
    return menu.Show(*data.popupFactory, data.popupOwner, {point.x, point.y, point.x + 1, point.y + 1});
}

void DuiSpreadsheet::InvokeContextMenuCommand(std::uint32_t command)
{
    auto& data = *spreadsheet_;
    switch (static_cast<ContextCommand>(command))
    {
    case ContextCommand::Cut: (void)OnKeyboardEvent(CommandEvent('X')); break;
    case ContextCommand::Copy: (void)OnKeyboardEvent(CommandEvent('C')); break;
    case ContextCommand::Paste: (void)OnKeyboardEvent(CommandEvent('V')); break;
    case ContextCommand::ClearContents:
    {
        core::Event event;
        event.type = core::EventType::KeyDown;
        event.key = core::key::Delete;
        (void)OnKeyboardEvent(event);
        break;
    }
    case ContextCommand::Find:
        if (data.findRequested) data.findRequested();
        break;
    case ContextCommand::Replace:
        if (data.replaceRequested) data.replaceRequested();
        break;
    case ContextCommand::SortAscending:
        (void)ApplySort({{data.active.column, DuiWorkbookSortDirection::Ascending}});
        break;
    case ContextCommand::SortDescending:
        (void)ApplySort({{data.active.column, DuiWorkbookSortDirection::Descending}});
        break;
    case ContextCommand::ClearSort: (void)ApplySort({}); break;
    case ContextCommand::FilterByCell:
    {
        const std::weak_ptr<int> lifetime = data.objectLifetime;
        IDuiWorkbookModel* const expectedModel = data.model;
        const DuiWorksheetId expectedSheet = data.sheet;
        const std::string value = data.model->CellText(data.sheet, data.active);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return;
        const DuiWorkbookFilterOperator operation = value.empty()
            ? DuiWorkbookFilterOperator::IsEmpty
            : DuiWorkbookFilterOperator::Equals;
        (void)SetFilters({{data.active.column, operation, value, false}});
        break;
    }
    case ContextCommand::ClearFilters: (void)ClearFilters(); break;
    case ContextCommand::AutoFitColumn: (void)AutoFitColumn(data.active.column); break;
    case ContextCommand::AutoFitRow: (void)AutoFitRow(data.active.row); break;
    case ContextCommand::FreezePanes:
    {
        const DuiCellAddress cell = data.selection.Normalized().first;
        SetFrozenPanes(cell.row, cell.column);
        break;
    }
    case ContextCommand::UnfreezePanes: ClearFrozenPanes(); break;
    }
}

} // namespace ysDui::controls::list
