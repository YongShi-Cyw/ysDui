/**
 * 文件名：DuiSpreadsheetPaint.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：实现工作表网格、选区、冻结区域和 Sheet 栏绘制。
 */
#include "DuiSpreadsheetInternal.hpp"

#include <algorithm>
#include <array>

#include "../input/DuiTextInputPaint.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::list {

void DuiSpreadsheet::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const auto& data = *spreadsheet_;
    if (!EffectivelyVisible() || !data.HasSheet())
        return;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    const auto contextMatches = [&]
    {
        return !lifetime.expired() && data.HasModelContext(expectedModel, expectedSheet);
    };
    const int rowCount = data.Rows();
    const int columnCount = data.Columns();
    const core::DuiTheme& theme = Theme();
    canvas.FillRect(Bounds(), theme.Get(core::ThemeSlot::SpreadsheetBackground));
    canvas.FillRect(data.columnHeader, theme.Get(core::ThemeSlot::SpreadsheetHeaderBackground));
    canvas.FillRect(data.rowHeader, theme.Get(core::ThemeSlot::SpreadsheetHeaderBackground));
    canvas.FillRect({data.rowHeader.left, data.columnHeader.top, data.rowHeader.right, data.columnHeader.bottom}, theme.Get(core::ThemeSlot::SpreadsheetHeaderBackground));
    const int frozenRows = std::clamp(data.frozenRows, 0, rowCount);
    const int frozenColumns = std::clamp(data.frozenColumns, 0, columnCount);
    const int frozenWidth = data.columns.OffsetAt(frozenColumns);
    const int frozenHeight = data.rows.OffsetAt(frozenRows);
    const auto cellRect = [&data](int row, int column) { return data.CellRect(row, column); };
    const auto visibleCellRect = [&data, frozenRows, frozenColumns, frozenWidth, frozenHeight](int row, int column)
    {
        core::Rect clip = data.viewport;
        if (column >= frozenColumns)
            clip.left = data.viewport.left + frozenWidth;
        if (row >= frozenRows)
            clip.top = data.viewport.top + frozenHeight;
        return core::Rect::Intersect(data.CellRect(row, column), clip);
    };
    int frozenRowEnd = 0;
    while (frozenRowEnd < frozenRows && cellRect(frozenRowEnd, 0).top < data.viewport.bottom)
        ++frozenRowEnd;
    int frozenColumnEnd = 0;
    while (frozenColumnEnd < frozenColumns && cellRect(0, frozenColumnEnd).left < data.viewport.right)
        ++frozenColumnEnd;
    const int firstRow = std::clamp(std::max(frozenRows, data.rows.IndexAt(frozenHeight + data.vertical->Position())),
                                    frozenRows, rowCount);
    const int firstColumn = std::clamp(std::max(frozenColumns, data.columns.IndexAt(frozenWidth + data.horizontal->Position())),
                                       frozenColumns, columnCount);
    int lastRow = firstRow;
    while (lastRow < rowCount && cellRect(lastRow, 0).top < data.viewport.bottom)
        ++lastRow;
    int lastColumn = firstColumn;
    while (lastColumn < columnCount && cellRect(0, lastColumn).left < data.viewport.right)
        ++lastColumn;
    const core::Color anchorFill{255, 255, 255, 255};
    const core::Color selectedGridLine{208, 208, 208, 255};
    const core::Color normalGridLine = theme.Get(core::ThemeSlot::SpreadsheetGridLine);
    const auto paintCell = [&](int row, int column)
    {
        const auto rect = visibleCellRect(row, column);
        if (!rect.Empty())
        {
            const DuiCellAddress cell{row, column};
            const bool selected = data.selection.Contains(cell);
            if (selected)
                canvas.FillRect(rect, cell == data.anchor ? anchorFill : theme.Get(core::ThemeSlot::SpreadsheetSelection));
            const core::Color lineColor = selected ? selectedGridLine : normalGridLine;
            canvas.FillRect({rect.right - 1, rect.top, rect.right, rect.bottom}, lineColor);
            canvas.FillRect({rect.left, rect.bottom - 1, rect.right, rect.bottom}, lineColor);
            const render::DuiTextStyle style{{30, 30, 30, 255}, theme.DefaultFontFamily(), theme.DefaultFontPointSize(), false, false};
            if (rect.Width() > 8)
            {
                const core::Rect textBounds{rect.left + 4, rect.top, rect.right - 4, rect.bottom};
                if (data.editing && row == data.editRow && column == data.editColumn
                    && data.HasTextInput())
                {
                    ysDui::controls::detail::PaintTextInput(
                        canvas, data.textInput, {}, {}, textBounds, style, style.color,
                        render::DuiTextAlignment::Start, false, Focused(),
                        theme.Get(core::ThemeSlot::SelectionBackground),
                        theme.Get(core::ThemeSlot::TextOnSelectedRow));
                }
                else
                {
                    const DuiCellPresentation presentation = expectedModel->CellPresentation(expectedSheet, cell);
                    if (!contextMatches())
                        return false;
                    canvas.DrawText(presentation.displayText, textBounds, style,
                                    render::DuiTextAlignment::Start, false);
                }
            }
        }
        return true;
    };
    bool paintAborted{};
    for (int row = 0; row < frozenRowEnd && !paintAborted; ++row)
        for (int column = 0; column < columnCount && !paintAborted; ++column)
            if (column < frozenColumnEnd || (column >= firstColumn && column < lastColumn))
                paintAborted = !paintCell(row, column);
    for (int row = firstRow; row < lastRow && !paintAborted; ++row)
        for (int column = 0; column < frozenColumnEnd && !paintAborted; ++column)
            paintAborted = !paintCell(row, column);
    for (int row = firstRow; row < lastRow && !paintAborted; ++row)
        for (int column = firstColumn; column < lastColumn && !paintAborted; ++column)
            paintAborted = !paintCell(row, column);
    if (paintAborted)
        return;

    const DuiCellRange selected = data.selection.Normalized();
    std::array<std::pair<int, int>, 2> visibleRows;
    std::array<std::pair<int, int>, 2> visibleColumns;
    std::size_t visibleRowCount{};
    std::size_t visibleColumnCount{};
    if (frozenRowEnd > 0)
        visibleRows[visibleRowCount++] = {0, frozenRowEnd - 1};
    if (lastRow > firstRow)
        visibleRows[visibleRowCount++] = {firstRow, lastRow - 1};
    if (frozenColumnEnd > 0)
        visibleColumns[visibleColumnCount++] = {0, frozenColumnEnd - 1};
    if (lastColumn > firstColumn)
        visibleColumns[visibleColumnCount++] = {firstColumn, lastColumn - 1};
    std::array<std::pair<int, int>, 2> selectedRows;
    std::array<std::pair<int, int>, 2> selectedColumns;
    std::size_t selectedRowCount{};
    std::size_t selectedColumnCount{};
    for (std::size_t index{}; index < visibleRowCount; ++index)
    {
        const auto& range = visibleRows[index];
        const int first = std::max(range.first, selected.first.row);
        const int last = std::min(range.second, selected.last.row);
        if (first <= last)
            selectedRows[selectedRowCount++] = {first, last};
    }
    for (std::size_t index{}; index < visibleColumnCount; ++index)
    {
        const auto& range = visibleColumns[index];
        const int first = std::max(range.first, selected.first.column);
        const int last = std::min(range.second, selected.last.column);
        if (first <= last)
            selectedColumns[selectedColumnCount++] = {first, last};
    }
    const auto gridLine = theme.Get(core::ThemeSlot::SpreadsheetGridLine);
    const auto freezeLine = theme.Get(core::ThemeSlot::SpreadsheetActiveCell);
    const render::DuiTextStyle headerStyle{{50, 50, 50, 255}, theme.DefaultFontFamily(), theme.DefaultFontPointSize(), false, false};
    const render::DuiTextStyle selectedHeaderStyle{freezeLine, theme.DefaultFontFamily(), theme.DefaultFontPointSize(), false, false};
    const auto selectedHeader = theme.Get(core::ThemeSlot::SpreadsheetSelection);
    const core::Color selectedHeaderGridLine{219, 219, 219, 255};
    const auto drawColumnHeader = [&](int column)
    {
        const core::Rect cell = cellRect(0, column);
        core::Rect clip = data.columnHeader;
        if (column >= frozenColumns)
            clip.left = data.columnHeader.left + frozenWidth;
        const core::Rect header = core::Rect::Intersect({cell.left, data.columnHeader.top, cell.right, data.columnHeader.bottom}, clip);
        if (header.Empty())
            return;
        const bool selectedColumn = selected.first.column <= column && column <= selected.last.column;
        if (selectedColumn)
            canvas.FillRect(header, selectedHeader);
        const auto headerGridLine = selectedColumn ? selectedHeaderGridLine : gridLine;
        canvas.FillRect({header.right - 1, header.top, header.right, header.bottom}, headerGridLine);
        canvas.FillRect({header.left, header.bottom - 1, header.right, header.bottom}, headerGridLine);
        if (selectedColumn)
            canvas.FillRect({header.left, header.bottom - 1, header.right, header.bottom + 1}, freezeLine);
        canvas.DrawText(ColumnName(column), header, selectedColumn ? selectedHeaderStyle : headerStyle,
                        render::DuiTextAlignment::Center, false);
        const auto sort = std::find_if(data.sortKeys.begin(), data.sortKeys.end(),
                                       [column](const DuiWorkbookSortKey& key) { return key.column == column; });
        const bool filtered = std::any_of(data.filters.begin(), data.filters.end(),
                                          [column](const DuiWorkbookFilter& filter)
                                          { return filter.column == column; });
        if (sort != data.sortKeys.end())
        {
            const int centerX = header.right - (filtered ? 17 : 8);
            const int centerY = (header.top + header.bottom) / 2;
            render::DuiPath arrow;
            if (sort->direction == DuiWorkbookSortDirection::Ascending)
            {
                arrow.MoveTo({centerX - 3, centerY + 2});
                arrow.LineTo({centerX, centerY - 2});
                arrow.LineTo({centerX + 3, centerY + 2});
            }
            else
            {
                arrow.MoveTo({centerX - 3, centerY - 2});
                arrow.LineTo({centerX, centerY + 2});
                arrow.LineTo({centerX + 3, centerY - 2});
            }
            arrow.Close();
            canvas.FillPath(arrow, freezeLine);
        }
        if (filtered)
        {
            const int right = header.right - 4;
            const int centerY = (header.top + header.bottom) / 2;
            render::DuiPath funnel;
            funnel.MoveTo({right - 7, centerY - 3});
            funnel.LineTo({right, centerY - 3});
            funnel.LineTo({right - 3, centerY});
            funnel.LineTo({right - 3, centerY + 4});
            funnel.LineTo({right - 5, centerY + 3});
            funnel.LineTo({right - 5, centerY});
            funnel.Close();
            canvas.FillPath(funnel, freezeLine);
        }
    };
    for (std::size_t index{}; index < visibleColumnCount; ++index)
    {
        const auto& range = visibleColumns[index];
        for (int column = range.first; column <= range.second; ++column)
            drawColumnHeader(column);
    }
    const auto drawRowHeader = [&](int row)
    {
        const core::Rect cell = cellRect(row, 0);
        core::Rect clip = data.rowHeader;
        if (row >= frozenRows)
            clip.top = data.rowHeader.top + frozenHeight;
        const core::Rect header = core::Rect::Intersect({data.rowHeader.left, cell.top, data.rowHeader.right, cell.bottom}, clip);
        if (header.Empty())
            return;
        const bool selectedRow = selected.first.row <= row && row <= selected.last.row;
        if (selectedRow)
            canvas.FillRect(header, selectedHeader);
        const auto headerGridLine = selectedRow ? selectedHeaderGridLine : gridLine;
        canvas.FillRect({header.left, header.bottom - 1, header.right, header.bottom}, headerGridLine);
        canvas.FillRect({header.right - 1, header.top, header.right, header.bottom}, headerGridLine);
        if (selectedRow)
            canvas.FillRect({header.right - 1, header.top, header.right + 1, header.bottom}, freezeLine);
        canvas.DrawText(std::to_string(row + 1), header, selectedRow ? selectedHeaderStyle : headerStyle,
                        render::DuiTextAlignment::Center, false);
    };
    for (std::size_t index{}; index < visibleRowCount; ++index)
    {
        const auto& range = visibleRows[index];
        for (int row = range.first; row <= range.second; ++row)
            drawRowHeader(row);
    }
    canvas.FillRect({data.rowHeader.left, data.columnHeader.top, data.rowHeader.right, data.columnHeader.bottom},
                    theme.Get(core::ThemeSlot::SpreadsheetHeaderBackground));
    canvas.FillRect({data.columnHeader.right, data.columnHeader.top, Bounds().right, data.columnHeader.bottom},
                    theme.Get(core::ThemeSlot::ScrollTrack));
    canvas.FillPath(data.selectAllTriangle, {128, 128, 128, 255});
    if (frozenColumns > 0)
        canvas.FillRect({data.viewport.left + frozenWidth - 1, data.columnHeader.top,
                         data.viewport.left + frozenWidth, data.viewport.bottom}, freezeLine);
    if (frozenRows > 0)
        canvas.FillRect({data.rowHeader.left, data.viewport.top + frozenHeight - 1,
                         data.viewport.right, data.viewport.top + frozenHeight}, freezeLine);
    canvas.FillRect(data.sheetBar, theme.Get(core::ThemeSlot::ScrollTrack));
    const int worksheetCount = expectedModel->WorksheetCount();
    if (!contextMatches())
        return;
    const int visibleTabCount = std::max(1, data.sheetTabArea.Width() / (kSheetTabWidth + kSheetTabGap));
    const int lastFirstVisibleSheet = std::max(0, worksheetCount - visibleTabCount);
    const auto drawSheetNavigation = [&](core::Rect bounds, std::string_view text, bool enabled)
    {
        render::DuiTextStyle style = headerStyle;
        style.color = theme.Get(enabled ? core::ThemeSlot::TabScrollArrow : core::ThemeSlot::TabScrollArrowDisabled);
        canvas.DrawText(text, bounds, style, render::DuiTextAlignment::Center, false);
    };
    drawSheetNavigation(data.firstSheetButton, "|<", data.firstVisibleSheet > 0);
    drawSheetNavigation(data.previousSheetButton, "<", data.firstVisibleSheet > 0);
    drawSheetNavigation(data.nextSheetButton, ">", data.firstVisibleSheet < lastFirstVisibleSheet);
    drawSheetNavigation(data.lastSheetButton, ">|", data.firstVisibleSheet < lastFirstVisibleSheet);
    drawSheetNavigation(data.sheetListButton, "...", worksheetCount > 0);

    int left = data.sheetTabArea.left;
    for (int index = data.firstVisibleSheet; index < worksheetCount; ++index)
    {
        if (left + kSheetTabWidth > data.sheetTabArea.right)
            break;
        const DuiWorksheetId sheet = expectedModel->WorksheetIdAt(index);
        if (!contextMatches())
            return;
        const bool selectedSheet = sheet == data.sheet;
        const core::Color fill = selectedSheet
            ? theme.Get(core::ThemeSlot::TabSelected)
            : sheet == data.hoveredSheet ? kSheetTabHoverColor : theme.Get(core::ThemeSlot::TabIdle);
        canvas.FillRect({left, data.sheetBar.top + 2, left + kSheetTabWidth, data.sheetBar.bottom - 2}, fill);
        const std::string name = expectedModel->WorksheetName(sheet);
        if (!contextMatches())
            return;
        render::DuiTextStyle tabStyle = headerStyle;
        if (selectedSheet)
            tabStyle.color = theme.Get(core::ThemeSlot::BrandPrimary);
        const core::Rect textBounds{left + 8, data.sheetBar.top, left + kSheetTabWidth - 8, data.sheetBar.bottom};
        if (data.renamingSheet && data.renameSheet == sheet && data.HasTextInput())
            ysDui::controls::detail::PaintTextInput(
                canvas, data.textInput, {}, {}, textBounds, tabStyle, tabStyle.color,
                render::DuiTextAlignment::Center, false, true,
                theme.Get(core::ThemeSlot::SelectionBackground),
                theme.Get(core::ThemeSlot::TextOnSelectedRow));
        else
            canvas.DrawText(name, textBounds, tabStyle, render::DuiTextAlignment::Center, false);
        left += kSheetTabWidth + kSheetTabGap;
    }
    canvas.DrawText("+", data.addSheetButton, headerStyle, render::DuiTextAlignment::Center, false);
    if (data.horizontal->Visible())
        data.horizontal->Paint(canvas, dirty);
    if (data.vertical->Visible())
        data.vertical->Paint(canvas, dirty);
    const core::Rect contentBounds{Bounds().left, Bounds().top, Bounds().right, data.sheetBar.top};
    if (!contentBounds.Empty())
    {
        canvas.FillRect({contentBounds.left, contentBounds.top, contentBounds.right, contentBounds.top + 1}, gridLine);
        canvas.FillRect({contentBounds.left, contentBounds.top, contentBounds.left + 1, contentBounds.bottom}, gridLine);
        canvas.FillRect({contentBounds.right - 1, contentBounds.top, contentBounds.right, contentBounds.bottom}, gridLine);
        canvas.FillRect({contentBounds.left, contentBounds.bottom - 1, contentBounds.right, contentBounds.bottom}, gridLine);
    }
    const bool movingPreview = data.movingSelection && data.moveTarget.Normalized() != selected;
    const bool fillingPreview = data.fillingSelection && data.fillTarget.Normalized() != selected;
    if (movingPreview || fillingPreview)
    {
        const DuiCellRange target = (fillingPreview ? data.fillTarget : data.moveTarget).Normalized();
        std::array<std::pair<int, int>, 2> targetRows;
        std::array<std::pair<int, int>, 2> targetColumns;
        std::size_t targetRowCount{};
        std::size_t targetColumnCount{};
        for (std::size_t index{}; index < visibleRowCount; ++index)
        {
            const int first = std::max(visibleRows[index].first, target.first.row);
            const int last = std::min(visibleRows[index].second, target.last.row);
            if (first <= last)
                targetRows[targetRowCount++] = {first, last};
        }
        for (std::size_t index{}; index < visibleColumnCount; ++index)
        {
            const int first = std::max(visibleColumns[index].first, target.first.column);
            const int last = std::min(visibleColumns[index].second, target.last.column);
            if (first <= last)
                targetColumns[targetColumnCount++] = {first, last};
        }
        const core::Color previewColor{128, 128, 128, 255};
        const auto drawHorizontal = [&canvas, &data, previewColor](int left, int right, int y)
        {
            for (int x = left; x < right; x += 4)
            {
                const core::Rect segment = core::Rect::Intersect(
                    {x, y - 2, std::min(x + 2, right), y + 1}, data.viewport);
                if (!segment.Empty())
                    canvas.FillRect(segment, previewColor);
            }
        };
        const auto drawVertical = [&canvas, &data, previewColor](int x, int top, int bottom)
        {
            for (int y = top; y < bottom; y += 4)
            {
                const core::Rect segment = core::Rect::Intersect(
                    {x - 2, y, x + 1, std::min(y + 2, bottom)}, data.viewport);
                if (!segment.Empty())
                    canvas.FillRect(segment, previewColor);
            }
        };
        for (std::size_t rowIndex{}; rowIndex < targetRowCount; ++rowIndex)
            for (std::size_t columnIndex{}; columnIndex < targetColumnCount; ++columnIndex)
            {
                const auto& rows = targetRows[rowIndex];
                const auto& columns = targetColumns[columnIndex];
                const core::Rect first = visibleCellRect(rows.first, columns.first);
                const core::Rect last = visibleCellRect(rows.second, columns.second);
                const core::Rect bounds = core::Rect::Intersect(
                    {first.left, first.top, last.right, last.bottom}, data.viewport);
                if (bounds.Empty())
                    continue;
                drawHorizontal(bounds.left, bounds.right, bounds.top);
                drawHorizontal(bounds.left, bounds.right, bounds.bottom);
                drawVertical(bounds.left, bounds.top, bounds.bottom);
                drawVertical(bounds.right, bounds.top, bounds.bottom);
            }
    }
    if (selectedRowCount != 0 && selectedColumnCount != 0)
    {
        const auto activeColor = theme.Get(core::ThemeSlot::SpreadsheetActiveCell);
        const bool columnSelection = data.selectionMode == SelectionMode::Columns;
        const bool rowSelection = data.selectionMode == SelectionMode::Rows;
        const auto fillBorder = [&canvas, &data, activeColor](core::Rect border)
        {
            border = core::Rect::Intersect(border, data.viewport);
            if (!border.Empty())
                canvas.FillRect(border, activeColor);
        };
        for (std::size_t rowIndex = 0; rowIndex < selectedRowCount; ++rowIndex)
            for (std::size_t columnIndex = 0; columnIndex < selectedColumnCount; ++columnIndex)
            {
                const auto& rows = selectedRows[rowIndex];
                const auto& columns = selectedColumns[columnIndex];
                const core::Rect first = visibleCellRect(rows.first, columns.first);
                const core::Rect last = visibleCellRect(rows.second, columns.second);
                const core::Rect bounds = core::Rect::Intersect({first.left, first.top, last.right, last.bottom}, data.viewport);
                if (bounds.Empty())
                    continue;
                if (rowIndex == 0)
                    fillBorder({bounds.left - 2, bounds.top - 2, bounds.right + 1, bounds.top + 1});
                if (!columnSelection && rowIndex + 1 == selectedRowCount)
                    fillBorder({bounds.left - 2, bounds.bottom - 2, bounds.right + 1, bounds.bottom + 1});
                if (columnIndex == 0)
                    fillBorder({bounds.left - 2, bounds.top - 2, bounds.left + 1, bounds.bottom + 1});
                if (!rowSelection && columnIndex + 1 == selectedColumnCount)
                    fillBorder({bounds.right - 2, bounds.top - 2, bounds.right + 1, bounds.bottom + 1});
                const bool isHandleCell = columnSelection
                    ? rowIndex == 0 && columnIndex + 1 == selectedColumnCount
                    : rowSelection
                        ? rowIndex + 1 == selectedRowCount && columnIndex == 0
                        : rowIndex + 1 == selectedRowCount && columnIndex + 1 == selectedColumnCount;
                if (isHandleCell)
                {
                    const core::Color handleBackground{255, 255, 255, 255};
                    const core::Rect handleClip = (columnSelection || rowSelection)
                        ? core::Rect{data.rowHeader.left, data.columnHeader.top, data.viewport.right, data.viewport.bottom}
                        : data.viewport;
                    const core::Rect handleOuter = core::Rect::Intersect(
                        columnSelection
                            ? core::Rect{bounds.right - 5, bounds.top - 5, bounds.right + 3, bounds.top + 3}
                            : rowSelection
                                ? core::Rect{bounds.left - 5, bounds.bottom - 5, bounds.left + 3, bounds.bottom + 3}
                                : core::Rect{bounds.right - 5, bounds.bottom - 5, bounds.right + 3, bounds.bottom + 3},
                        handleClip);
                    const core::Rect handle = core::Rect::Intersect(
                        columnSelection
                            ? core::Rect{bounds.right - 4, bounds.top - 4, bounds.right + 2, bounds.top + 2}
                            : rowSelection
                                ? core::Rect{bounds.left - 4, bounds.bottom - 4, bounds.left + 2, bounds.bottom + 2}
                                : core::Rect{bounds.right - 4, bounds.bottom - 4, bounds.right + 2, bounds.bottom + 2},
                        handleClip);
                    canvas.FillRect(handleOuter, handleBackground);
                    canvas.FillRect(handle, activeColor);
                }
            }
    }
}

} // namespace ysDui::controls::list
