/**
 * 文件名：DuiSpreadsheetViewport.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：计算工作表冻结区与滚动区的可见数据请求范围。
 */
#include "DuiSpreadsheetInternal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace ysDui::controls::list {

void DuiSpreadsheet::RequestVisibleCellRanges()
{
    auto& data = *spreadsheet_;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    if (!data.HasCells())
    {
        data.requestedModel = expectedModel;
        data.requestedSheet = expectedSheet;
        data.requestedRanges.clear();
        return;
    }
    if (data.viewport.Empty())
        return;

    struct AxisRange final
    {
        int first{};
        int last{};
    };
    const int rowCount = data.Rows();
    const int columnCount = data.Columns();
    const int frozenRows = data.FrozenRowCount();
    const int frozenColumns = data.FrozenColumnCount();
    const int frozenHeight = data.rows.OffsetAt(frozenRows);
    const int frozenWidth = data.columns.OffsetAt(frozenColumns);

    int frozenRowEnd{};
    while (frozenRowEnd < frozenRows && data.CellRect(frozenRowEnd, 0).top < data.viewport.bottom)
        ++frozenRowEnd;
    int frozenColumnEnd{};
    while (frozenColumnEnd < frozenColumns && data.CellRect(0, frozenColumnEnd).left < data.viewport.right)
        ++frozenColumnEnd;

    const int firstRow = std::clamp(
        std::max(frozenRows, data.rows.IndexAt(frozenHeight + data.vertical->Position())), frozenRows, rowCount);
    const int firstColumn = std::clamp(
        std::max(frozenColumns, data.columns.IndexAt(frozenWidth + data.horizontal->Position())),
        frozenColumns, columnCount);
    int lastRow = firstRow;
    while (lastRow < rowCount && data.CellRect(lastRow, 0).top < data.viewport.bottom)
        ++lastRow;
    int lastColumn = firstColumn;
    while (lastColumn < columnCount && data.CellRect(0, lastColumn).left < data.viewport.right)
        ++lastColumn;

    std::array<AxisRange, 2> rowRanges;
    std::array<AxisRange, 2> columnRanges;
    std::size_t rowRangeCount{};
    std::size_t columnRangeCount{};
    const auto appendRange = [](auto& ranges, std::size_t& count, int first, int last)
    {
        if (first >= last)
            return;
        if (count != 0 && first <= ranges[count - 1].last)
            ranges[count - 1].last = std::max(ranges[count - 1].last, last);
        else
            ranges[count++] = {first, last};
    };
    appendRange(rowRanges, rowRangeCount, 0, frozenRowEnd);
    appendRange(rowRanges, rowRangeCount, firstRow, lastRow);
    appendRange(columnRanges, columnRangeCount, 0, frozenColumnEnd);
    appendRange(columnRanges, columnRangeCount, firstColumn,
                std::min(columnCount, lastColumn + kViewportPrefetchColumns));

    std::array<DuiCellRange, 4> ranges;
    std::size_t rangeCount{};
    for (std::size_t row = 0; row < rowRangeCount; ++row)
        for (std::size_t column = 0; column < columnRangeCount; ++column)
            ranges[rangeCount++] = {{rowRanges[row].first, columnRanges[column].first},
                                    {rowRanges[row].last - 1, columnRanges[column].last - 1}};

    if (data.requestedModel == expectedModel && data.requestedSheet == expectedSheet
        && data.requestedRanges.size() == rangeCount
        && std::equal(data.requestedRanges.begin(), data.requestedRanges.end(), ranges.begin()))
        return;
    data.requestedModel = expectedModel;
    data.requestedSheet = expectedSheet;
    data.requestedRanges.assign(ranges.begin(), ranges.begin() + static_cast<std::ptrdiff_t>(rangeCount));
    expectedModel->RequestCellRanges(expectedSheet, data.requestedRanges);
}

} // namespace ysDui::controls::list
