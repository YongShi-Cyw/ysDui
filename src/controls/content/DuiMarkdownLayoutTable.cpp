/**
 * 文件名：DuiMarkdownLayoutTable.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：Markdown 表格列宽分配与单元格对齐布局。
 */
#include "DuiMarkdownLayoutInternals.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace ysDui::controls::content::detail {

void LayoutTableBlock(LayoutContext& ctx, const BlockNode& block, int left, int right, int& y)
{
    std::vector<const BlockNode*> rows;
    for (const BlockNode& row : block.children)
    {
        if (row.kind == BlockKind::TableRow)
            rows.push_back(&row);
    }
    if (rows.empty())
        return;
    std::size_t colCount = 0;
    for (const BlockNode* row : rows)
        colCount = (std::max)(colCount, row->children.size());
    if (colCount == 0)
        return;

    const int tableTop = y;
    const int tableWidth = (std::max)(1, right - left);
    // 按单元格纯文本偏好宽度分配列（过窄均分余量；过宽不压缩，交由横滚）
    std::vector<int> preferred(colCount, kTableMinColWidth);
    for (const BlockNode* row : rows)
    {
        for (std::size_t col = 0; col < row->children.size(); ++col)
        {
            std::string plain;
            for (const InlineNode& inlineNode : row->children[col].inlines)
                AppendPlainInline(inlineNode, plain);
            render::DuiTextStyle measureStyle = ctx.baseStyle;
            if (row->children[col].headerCell)
                measureStyle.bold = true;
            const int textW = plain.empty()
                ? 0
                : ctx.measurer->MeasureText(plain, measureStyle, {}).size.width;
            preferred[col] = (std::max)(preferred[col], textW + kTableCellPadX * 2);
        }
    }
    int preferredSum = 0;
    for (const int width : preferred)
        preferredSum += width;
    std::vector<int> colWidths(colCount, tableWidth / static_cast<int>(colCount));
    if (preferredSum > 0)
    {
        if (preferredSum <= tableWidth)
        {
            const int extra = tableWidth - preferredSum;
            const int baseExtra = extra / static_cast<int>(colCount);
            int rem = extra % static_cast<int>(colCount);
            for (std::size_t col = 0; col < colCount; ++col)
            {
                colWidths[col] = preferred[col] + baseExtra + (rem > 0 ? 1 : 0);
                if (rem > 0)
                    --rem;
            }
        }
        else
        {
            colWidths = preferred;
            ctx.NoteContentRight(left + preferredSum);
        }
    }
    std::vector<int> colLefts(colCount + 1, left);
    for (std::size_t col = 0; col < colCount; ++col)
        colLefts[col + 1] = colLefts[col] + colWidths[col];
    const int tableRight = colLefts.back();
    ctx.NoteContentRight(tableRight);

    struct CellSpan final {
        std::size_t runBegin{};
        std::size_t runEnd{};
        int contentBottom{};
        int cellLeft{};
        int cellRight{};
        render::DuiTextAlignment align{render::DuiTextAlignment::Start};
    };
    std::vector<int> rowTops;
    std::vector<int> rowBottoms;
    std::vector<bool> rowIsHeader;
    rowTops.reserve(rows.size());
    rowBottoms.reserve(rows.size());
    rowIsHeader.reserve(rows.size());

    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const BlockNode* row = rows[rowIndex];
        const bool isHeader = !row->children.empty() && row->children.front().headerCell;
        const int rowTop = y;
        int rowBottom = y + ctx.baseStyle.pointSize + kTableCellPadY * 2;
        std::vector<CellSpan> spans;
        spans.reserve(colCount);
        for (std::size_t col = 0; col < colCount; ++col)
        {
            const int padLeft = colLefts[col] + kTableCellPadX;
            const int padRight = colLefts[col + 1] - kTableCellPadX;
            CellSpan span;
            span.cellLeft = padLeft;
            span.cellRight = (std::max)(padLeft + 1, padRight);
            span.runBegin = ctx.layout.runs.size();
            span.contentBottom = rowTop + kTableCellPadY;
            if (col < row->children.size())
            {
                const BlockNode& cell = row->children[col];
                span.align = cell.cellAlign;
                int cellY = rowTop + kTableCellPadY;
                render::DuiTextStyle old = ctx.baseStyle;
                if (cell.headerCell || isHeader)
                    ctx.baseStyle.bold = true;
                LayoutInlines(ctx, cell.inlines, span.cellLeft, span.cellRight, cellY, {});
                ctx.baseStyle = old;
                span.contentBottom = cellY;
                rowBottom = (std::max)(rowBottom, cellY + kTableCellPadY);
            }
            span.runEnd = ctx.layout.runs.size();
            spans.push_back(span);
        }

        // 行高确定后：垂直居中 + 水平对齐（左/中/右）
        for (const CellSpan& span : spans)
        {
            const int contentTop = rowTop + kTableCellPadY;
            const int availableH = rowBottom - kTableCellPadY - contentTop;
            const int usedH = (std::max)(0, span.contentBottom - contentTop);
            const int shiftY = (std::max)(0, (availableH - usedH) / 2);

            int minLeft = (std::numeric_limits<int>::max)();
            int maxRight = (std::numeric_limits<int>::min)();
            for (std::size_t index = span.runBegin; index < span.runEnd; ++index)
            {
                minLeft = (std::min)(minLeft, ctx.layout.runs[index].bounds.left);
                maxRight = (std::max)(maxRight, ctx.layout.runs[index].bounds.right);
            }
            int shiftX = 0;
            if (minLeft <= maxRight)
            {
                const int contentW = maxRight - minLeft;
                const int availW = (std::max)(0, span.cellRight - span.cellLeft);
                if (span.align == render::DuiTextAlignment::Center)
                    shiftX = span.cellLeft + (availW - contentW) / 2 - minLeft;
                else if (span.align == render::DuiTextAlignment::End)
                    shiftX = span.cellRight - contentW - minLeft;
                else
                    shiftX = span.cellLeft - minLeft;
            }
            if (shiftX == 0 && shiftY == 0)
                continue;
            for (std::size_t index = span.runBegin; index < span.runEnd; ++index)
            {
                ctx.layout.runs[index].bounds.left += shiftX;
                ctx.layout.runs[index].bounds.right += shiftX;
                ctx.layout.runs[index].bounds.top += shiftY;
                ctx.layout.runs[index].bounds.bottom += shiftY;
            }
        }

        rowTops.push_back(rowTop);
        rowBottoms.push_back(rowBottom);
        rowIsHeader.push_back(isHeader);
        y = rowBottom;
    }

    const int tableBottom = y;
    // 表头底色（无斑马纹）
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        if (!rowIsHeader[rowIndex])
            continue;
        ctx.AddDecor({left, rowTops[rowIndex], tableRight, rowBottoms[rowIndex]},
                     ctx.palette.tableHeaderFill);
    }
    // 完整网格：横线 + 竖线（直角，无外框圆角）
    const core::Color grid = ctx.palette.tableBorder;
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        ctx.AddDecor({left, rowTops[rowIndex], tableRight, rowTops[rowIndex] + kTableGridLine},
                     grid);
        if (rowIndex + 1 == rows.size())
        {
            ctx.AddDecor(
                {left, tableBottom - kTableGridLine, tableRight, tableBottom}, grid);
        }
    }
    for (std::size_t col = 0; col <= colCount; ++col)
    {
        const int x = colLefts[col];
        const int lineLeft = (col == colCount) ? tableRight - kTableGridLine : x;
        ctx.AddDecor({lineLeft, tableTop, lineLeft + kTableGridLine, tableBottom}, grid);
    }
    y += 12;

}

} // namespace ysDui::controls::content::detail
