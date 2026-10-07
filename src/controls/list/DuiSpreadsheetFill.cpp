/**
 * 文件名：DuiSpreadsheetFill.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-10
 * 用途：实现工作表填充柄命中、预览和批量填充。
 */
#include "DuiSpreadsheetInternal.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ysDui::controls::list {

namespace {

bool ParseInteger(std::string_view text, std::int64_t& value)
{
    if (text.empty())
        return false;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool CheckedAdd(std::int64_t left, std::int64_t right, std::int64_t& result)
{
    constexpr std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left > maximum - right) || (right < 0 && left < minimum - right))
        return false;
    result = left + right;
    return true;
}

bool CheckedSubtract(std::int64_t left, std::int64_t right, std::int64_t& result)
{
    constexpr std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left < minimum + right) || (right < 0 && left > maximum + right))
        return false;
    result = left - right;
    return true;
}

std::optional<std::int64_t> IntegerStep(const std::vector<std::string>& source)
{
    if (source.size() < 2)
        return std::nullopt;
    std::int64_t previous{};
    std::int64_t current{};
    if (!ParseInteger(source[0], previous) || !ParseInteger(source[1], current))
        return std::nullopt;
    std::int64_t step{};
    if (!CheckedSubtract(current, previous, step))
        return std::nullopt;
    for (std::size_t index = 2; index < source.size(); ++index)
    {
        previous = current;
        if (!ParseInteger(source[index], current))
            return std::nullopt;
        std::int64_t candidate{};
        if (!CheckedSubtract(current, previous, candidate) || candidate != step)
            return std::nullopt;
    }
    return step;
}

std::size_t RepeatedIndex(int index, int sourceOffset, int sourceCount)
{
    int repeated = (index - sourceOffset) % sourceCount;
    if (repeated < 0)
        repeated += sourceCount;
    return static_cast<std::size_t>(repeated);
}

std::vector<std::string> BuildFilledLane(const std::vector<std::string>& source,
                                         int leadingCount, int trailingCount)
{
    const int sourceCount = static_cast<int>(source.size());
    const int totalCount = leadingCount + sourceCount + trailingCount;
    std::vector<std::string> values(static_cast<std::size_t>(totalCount));
    for (int index = 0; index < sourceCount; ++index)
        values[static_cast<std::size_t>(leadingCount + index)] = source[static_cast<std::size_t>(index)];

    const auto step = IntegerStep(source);
    bool sequenceComplete = step.has_value();
    if (sequenceComplete)
    {
        std::int64_t value{};
        ParseInteger(source.front(), value);
        for (int index = leadingCount - 1; index >= 0; --index)
        {
            std::int64_t next{};
            if (!CheckedSubtract(value, *step, next))
            {
                sequenceComplete = false;
                break;
            }
            value = next;
            values[static_cast<std::size_t>(index)] = std::to_string(value);
        }
        ParseInteger(source.back(), value);
        for (int index = leadingCount + sourceCount; sequenceComplete && index < totalCount; ++index)
        {
            std::int64_t next{};
            if (!CheckedAdd(value, *step, next))
            {
                sequenceComplete = false;
                break;
            }
            value = next;
            values[static_cast<std::size_t>(index)] = std::to_string(value);
        }
    }
    if (sequenceComplete)
        return values;

    for (int index = 0; index < totalCount; ++index)
        if (index < leadingCount || index >= leadingCount + sourceCount)
            values[static_cast<std::size_t>(index)] = source[RepeatedIndex(index, leadingCount, sourceCount)];
    return values;
}

} // namespace

bool DuiSpreadsheet::FillHandleContains(const Impl& data, core::Point point)
{
    if (!data.HasCells() || data.selectionMode != SelectionMode::Cells)
        return false;
    const DuiCellRange source = data.selection.Normalized();
    core::Rect clip = data.viewport;
    if (source.last.row >= data.FrozenRowCount())
        clip.top = std::max(clip.top, data.viewport.top + data.FrozenHeight());
    if (source.last.column >= data.FrozenColumnCount())
        clip.left = std::max(clip.left, data.viewport.left + data.FrozenWidth());
    const core::Rect cell = core::Rect::Intersect(
        data.CellRect(source.last.row, source.last.column), clip);
    if (cell.Empty())
        return false;
    return core::Rect{cell.right - 5, cell.bottom - 5, cell.right + 3, cell.bottom + 3}.Contains(point);
}

bool DuiSpreadsheet::BeginSelectionFill()
{
    auto& data = *spreadsheet_;
    if (!data.HasCells() || data.selectionMode != SelectionMode::Cells)
        return false;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    if (Editing())
        CommitEdit();
    if (lifetime.expired())
        return true;
    if (!data.HasModelContext(expectedModel, expectedSheet) || Editing())
        return true;
    data.fillingSelection = true;
    data.fillAxis = FillAxis::None;
    data.fillSource = data.selection.Normalized();
    data.fillTarget = data.fillSource;
    SetCaptured(true);
    return true;
}

bool DuiSpreadsheet::UpdateSelectionFill(core::Point point)
{
    auto& data = *spreadsheet_;
    if (!data.fillingSelection || data.viewport.Empty())
        return true;
    point.x = std::clamp(point.x, data.viewport.left, data.viewport.right - 1);
    point.y = std::clamp(point.y, data.viewport.top, data.viewport.bottom - 1);
    const int row = std::clamp(RowFromPoint(data, point), 0, data.Rows() - 1);
    const int column = std::clamp(ColumnFromPoint(data, point), 0, data.Columns() - 1);
    const DuiCellRange source = data.fillSource.Normalized();
    const int rowExtension = row < source.first.row ? source.first.row - row
        : row > source.last.row ? row - source.last.row : 0;
    const int columnExtension = column < source.first.column ? source.first.column - column
        : column > source.last.column ? column - source.last.column : 0;
    if (rowExtension == 0 && columnExtension == 0)
    {
        data.fillAxis = FillAxis::None;
        data.fillTarget = source;
    }
    else if (rowExtension >= columnExtension)
    {
        data.fillAxis = FillAxis::Rows;
        data.fillTarget = {{std::min(row, source.first.row), source.first.column},
                           {std::max(row, source.last.row), source.last.column}};
    }
    else
    {
        data.fillAxis = FillAxis::Columns;
        data.fillTarget = {{source.first.row, std::min(column, source.first.column)},
                           {source.last.row, std::max(column, source.last.column)}};
    }
    return true;
}

bool DuiSpreadsheet::EndSelectionFill()
{
    auto& data = *spreadsheet_;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    data.fillingSelection = false;
    SetCaptured(false);
    const bool filled = CommitSelectionFill();
    if (!lifetime.expired())
        data.fillAxis = FillAxis::None;
    return filled;
}

bool DuiSpreadsheet::CommitSelectionFill()
{
    auto& data = *spreadsheet_;
    if (!data.HasCells())
        return false;
    const DuiCellRange source = data.fillSource.Normalized();
    const DuiCellRange target = data.fillTarget.Normalized();
    if (data.fillAxis == FillAxis::None || source == target)
        return true;
    const bool vertical = data.fillAxis == FillAxis::Rows;
    const int laneCount = vertical ? source.last.column - source.first.column + 1
                                   : source.last.row - source.first.row + 1;
    const int sourceCount = vertical ? source.last.row - source.first.row + 1
                                     : source.last.column - source.first.column + 1;
    const int leadingCount = vertical ? source.first.row - target.first.row
                                      : source.first.column - target.first.column;
    const int trailingCount = vertical ? target.last.row - source.last.row
                                       : target.last.column - source.last.column;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    std::vector<std::vector<std::string>> lanes(static_cast<std::size_t>(laneCount));
    for (int lane = 0; lane < laneCount; ++lane)
    {
        auto& values = lanes[static_cast<std::size_t>(lane)];
        values.reserve(static_cast<std::size_t>(sourceCount));
        for (int index = 0; index < sourceCount; ++index)
        {
            const DuiCellAddress cell = vertical
                ? DuiCellAddress{source.first.row + index, source.first.column + lane}
                : DuiCellAddress{source.first.row + lane, source.first.column + index};
            values.push_back(expectedModel->CellText(expectedSheet, cell));
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return false;
        }
        values = BuildFilledLane(values, leadingCount, trailingCount);
    }

    std::vector<CellChange> changes;
    changes.reserve(static_cast<std::size_t>(laneCount * (leadingCount + trailingCount)));
    const int targetCount = leadingCount + sourceCount + trailingCount;
    for (int lane = 0; lane < laneCount; ++lane)
        for (int index = 0; index < targetCount; ++index)
        {
            if (index >= leadingCount && index < leadingCount + sourceCount)
                continue;
            const DuiCellAddress cell = vertical
                ? DuiCellAddress{target.first.row + index, source.first.column + lane}
                : DuiCellAddress{source.first.row + lane, target.first.column + index};
            std::string before = expectedModel->CellText(expectedSheet, cell);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return false;
            const std::string& after = lanes[static_cast<std::size_t>(lane)][static_cast<std::size_t>(index)];
            if (before != after)
                changes.push_back({ChangeKind::Cell, cell, std::move(before), after});
        }
    if (!data.ApplyCellChanges(changes, false))
        return false;
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return false;

    const bool activeChanged = data.active != target.last;
    data.anchor = target.first;
    data.active = target.last;
    data.selection = target;
    data.selectionMode = SelectionMode::Cells;
    data.RecordUndo(std::move(changes));
    if (activeChanged && data.activeChanged)
    {
        const auto handler = data.activeChanged;
        handler(data.active);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.active != target.last || data.selection != target)
            return true;
    }
    if (data.selectionChanged)
    {
        const auto handler = data.selectionChanged;
        handler(target);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet)
            || data.active != target.last || data.selection != target)
            return true;
    }
    EnsureVisible(target.last);
    return true;
}

} // namespace ysDui::controls::list
