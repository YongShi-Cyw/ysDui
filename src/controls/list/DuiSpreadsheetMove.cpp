/**
 * 文件名：DuiSpreadsheetMove.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-10
 * 用途：实现工作表选区边框命中、整块移动与失败回滚。
 */
#include "DuiSpreadsheetInternal.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace ysDui::controls::list {

bool DuiSpreadsheet::Impl::ApplyCellChanges(const std::vector<CellChange>& changes, bool useBefore)
{
    if (model == nullptr)
        return false;
    const std::weak_ptr<int> lifetime = objectLifetime;
    IDuiWorkbookModel* const expectedModel = model;
    const DuiWorksheetId expectedSheet = sheet;
    for (const auto& change : changes)
    {
        if (change.kind != ChangeKind::Cell)
            return false;
        const bool readOnly = expectedModel->IsCellReadOnly(expectedSheet, change.address);
        if (lifetime.expired() || !HasModelContext(expectedModel, expectedSheet))
            return false;
        if (readOnly)
        {
            ReportWriteError(expectedSheet, change.address, ReadOnlyWriteResult());
            return false;
        }
    }

    std::size_t applied{};
    DuiWorkbookWriteResult failure;
    DuiCellAddress failedCell{};
    {
        ModelUpdateScope update(*this);
        for (const auto& change : changes)
        {
            DuiWorkbookWriteResult result = expectedModel->SetCellTextWithResult(
                expectedSheet, change.address, useBefore ? change.before : change.after);
            if (lifetime.expired() || !HasModelContext(expectedModel, expectedSheet))
                return false;
            if (!result)
            {
                failure = std::move(result);
                failedCell = change.address;
                break;
            }
            ++applied;
        }
    }
    if (failure)
        return true;

    DuiWorkbookWriteResult rollbackFailure;
    DuiCellAddress rollbackCell{};
    {
        ModelUpdateScope update(*this);
        while (applied > 0)
        {
            const auto& change = changes[--applied];
            DuiWorkbookWriteResult result = expectedModel->SetCellTextWithResult(
                expectedSheet, change.address, useBefore ? change.after : change.before);
            if (lifetime.expired() || !HasModelContext(expectedModel, expectedSheet))
                return false;
            if (!result && rollbackFailure)
            {
                rollbackFailure = std::move(result);
                rollbackCell = change.address;
            }
        }
    }
    if (lifetime.expired() || !HasModelContext(expectedModel, expectedSheet))
        return false;
    ReportWriteError(expectedSheet, failedCell, std::move(failure));
    if (lifetime.expired() || !HasModelContext(expectedModel, expectedSheet))
        return false;
    if (!rollbackFailure)
        ReportWriteError(expectedSheet, rollbackCell, std::move(rollbackFailure));
    return false;
}

bool DuiSpreadsheet::SelectionBorderContains(const Impl& data, core::Point point)
{
    if (!data.HasCells() || data.selectionMode != SelectionMode::Cells)
        return false;
    const DuiCellRange selection = data.selection.Normalized();
    std::array<std::pair<int, int>, 2> rowRanges{};
    std::array<std::pair<int, int>, 2> columnRanges{};
    std::size_t rowRangeCount{};
    std::size_t columnRangeCount{};
    const int frozenRows = data.FrozenRowCount();
    const int frozenColumns = data.FrozenColumnCount();
    // 用容量守卫写入，避免 MSVC /analyze 对 rowRangeCount++ 误报 C28020（_Param_(1)<2）。
    const auto pushRange = [](auto& ranges, std::size_t& count, std::pair<int, int> value)
    {
        if (count < ranges.size())
            ranges[count++] = value;
    };
    if (selection.first.row < frozenRows)
        pushRange(rowRanges, rowRangeCount,
                  {selection.first.row, std::min(selection.last.row, frozenRows - 1)});
    if (selection.last.row >= frozenRows)
        pushRange(rowRanges, rowRangeCount,
                  {std::max(selection.first.row, frozenRows), selection.last.row});
    if (selection.first.column < frozenColumns)
        pushRange(columnRanges, columnRangeCount,
                  {selection.first.column, std::min(selection.last.column, frozenColumns - 1)});
    if (selection.last.column >= frozenColumns)
        pushRange(columnRanges, columnRangeCount,
                  {std::max(selection.first.column, frozenColumns), selection.last.column});

    for (std::size_t rowIndex{}; rowIndex < rowRangeCount; ++rowIndex)
        for (std::size_t columnIndex{}; columnIndex < columnRangeCount; ++columnIndex)
        {
            const auto& rows = rowRanges[rowIndex];
            const auto& columns = columnRanges[columnIndex];
            core::Rect clip = data.viewport;
            if (rows.first < frozenRows)
                clip.bottom = std::min(clip.bottom, data.viewport.top + data.FrozenHeight());
            else
                clip.top = std::max(clip.top, data.viewport.top + data.FrozenHeight());
            if (columns.first < frozenColumns)
                clip.right = std::min(clip.right, data.viewport.left + data.FrozenWidth());
            else
                clip.left = std::max(clip.left, data.viewport.left + data.FrozenWidth());
            const core::Rect first = data.CellRect(rows.first, columns.first);
            const core::Rect last = data.CellRect(rows.second, columns.second);
            const core::Rect bounds = core::Rect::Intersect(
                {first.left, first.top, last.right, last.bottom}, clip);
            if (bounds.Empty())
                continue;
            if (rowIndex + 1 == rowRangeCount && columnIndex + 1 == columnRangeCount
                && core::Rect{bounds.right - 5, bounds.bottom - 5,
                              bounds.right + 3, bounds.bottom + 3}.Contains(point))
                return false;
            if (core::Rect{bounds.left - 2, bounds.top - 2, bounds.right + 1, bounds.top + 1}.Contains(point)
                || core::Rect{bounds.left - 2, bounds.bottom - 2,
                              bounds.right + 1, bounds.bottom + 1}.Contains(point)
                || core::Rect{bounds.left - 2, bounds.top - 2,
                              bounds.left + 1, bounds.bottom + 1}.Contains(point)
                || core::Rect{bounds.right - 2, bounds.top - 2,
                              bounds.right + 1, bounds.bottom + 1}.Contains(point))
                return true;
        }
    return false;
}

bool DuiSpreadsheet::CommitSelectionMove(DuiCellRange target)
{
    auto& data = *spreadsheet_;
    if (!data.HasCells())
        return false;
    const DuiCellRange source = data.moveSource.Normalized();
    target = target.Normalized();
    if (source == target)
        return true;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const auto key = [](DuiCellAddress cell)
    {
        return (static_cast<std::int64_t>(cell.row) << 32)
            | static_cast<std::uint32_t>(cell.column);
    };
    std::map<std::int64_t, DuiCellAddress> affected;
    const auto addRange = [&affected, &key](DuiCellRange range)
    {
        for (int row = range.first.row; row <= range.last.row; ++row)
            for (int column = range.first.column; column <= range.last.column; ++column)
            {
                const DuiCellAddress cell{row, column};
                affected.emplace(key(cell), cell);
            }
    };
    addRange(source);
    addRange(target);

    std::map<std::int64_t, std::string> before;
    for (const auto& [cellKey, cell] : affected)
    {
        before.emplace(cellKey, expectedModel->CellText(expectedSheet, cell));
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return false;
    }
    std::vector<CellChange> changes;
    changes.reserve(affected.size());
    for (const auto& [cellKey, cell] : affected)
    {
        std::string after;
        if (target.Contains(cell))
        {
            const DuiCellAddress sourceCell{
                source.first.row + cell.row - target.first.row,
                source.first.column + cell.column - target.first.column};
            after = before.at(key(sourceCell));
        }
        const std::string& original = before.at(cellKey);
        if (original != after)
            changes.push_back({ChangeKind::Cell, cell, original, std::move(after)});
    }
    if (!data.ApplyCellChanges(changes, false))
        return false;
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return false;

    const int rowDelta = target.first.row - source.first.row;
    const int columnDelta = target.first.column - source.first.column;
    const DuiCellAddress nextAnchor{data.anchor.row + rowDelta, data.anchor.column + columnDelta};
    const DuiCellAddress nextActive{data.active.row + rowDelta, data.active.column + columnDelta};
    const bool activeChanged = data.active != nextActive;
    data.anchor = nextAnchor;
    data.active = nextActive;
    data.selection = target;
    data.selectionMode = SelectionMode::Cells;
    data.RecordUndo(std::move(changes));
    if (activeChanged && data.activeChanged)
    {
        const auto handler = data.activeChanged;
        handler(nextActive);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.active != nextActive || data.selection != target)
            return true;
    }
    if (data.selectionChanged)
    {
        const auto handler = data.selectionChanged;
        handler(target);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.active != nextActive || data.selection != target)
            return true;
    }
    EnsureVisible(nextActive);
    return true;
}

} // namespace ysDui::controls::list
