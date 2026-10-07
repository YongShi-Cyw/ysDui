/**
 * 文件名：DuiTreeViewPaint.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：DuiTreeView 的绘制实现（行、表头、单元格、拖拽反馈），
 *       由 DuiTreeView.cpp 拆分而来，共享 DuiTreeViewInternal.hpp 中的 Impl 与布局常量。
 */
#include "ysDui/controls/list/DuiTreeView.hpp"

#include "DuiTreeViewInternal.hpp"

#include <algorithm>
#include <string>

#include "../input/DuiTextInputPaint.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::list {
namespace {
void PaintDisclosureGlyph(render::Canvas& canvas, core::Rect bounds, bool expanded,
                          core::Color color)
{
    const int centerX = (bounds.left + bounds.right) / 2;
    const int centerY = (bounds.top + bounds.bottom) / 2;
    render::DuiPath path;
    if (expanded)
    {
        path.MoveTo({centerX - 4, centerY - 2});
        path.LineTo({centerX + 4, centerY - 2});
        path.LineTo({centerX, centerY + 2});
    }
    else
    {
        path.MoveTo({centerX - 2, centerY - 4});
        path.LineTo({centerX + 2, centerY});
        path.LineTo({centerX - 2, centerY + 4});
    }
    path.Close();
    canvas.FillPath(path, color);
}

void PaintNodeCheck(render::Canvas& canvas, core::Rect bounds, DuiTreeCheckState state,
                    core::Color border, core::Color accent)
{
    canvas.StrokeRoundedRect(bounds, 1, border, 1.0F);
    if (state == DuiTreeCheckState::Checked)
        canvas.FillRoundedRect(bounds, 1, accent);
    else if (state == DuiTreeCheckState::Mixed)
        canvas.FillRect({bounds.left + 3, bounds.top + bounds.Height() / 2 - 1,
                         bounds.right - 3, bounds.top + bounds.Height() / 2 + 1}, accent);
}
}

void DuiTreeView::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty()) return;
    const core::DuiTheme& theme = Theme();
    render::DuiTextStyle textStyle = tree_->textStyle;
    textStyle.color = theme.Get(core::ThemeSlot::TreeText);
    render::DuiTextStyle subtitleStyle = tree_->subtitleStyle;
    subtitleStyle.color = theme.Get(core::ThemeSlot::TreeSubtitle);
    canvas.FillRect(bounds, theme.Get(core::ThemeSlot::ListBackground));
    const int header = tree_->columns.empty() ? 0 : tree_->headerHeight;
    if (!tree_->columns.empty()) {
        const core::Rect headerBounds{Bounds().left, Bounds().top, Bounds().right, Bounds().top + header};
        canvas.FillRect(headerBounds, theme.Get(core::ThemeSlot::GridHeaderBackground));
        int left = Bounds().left;
        for (const auto& column : tree_->columns) {
            const int index = static_cast<int>(&column - tree_->columns.data());
            std::string title = column.title;
            if (index == SortColumn() && SortDirection() != 0)
                title += SortDirection() > 0 ? " ^" : " v";
            canvas.DrawText(title, {left + detail::TextPadding, headerBounds.top, left + column.width - detail::TextPadding, headerBounds.bottom}, textStyle, column.alignment, false);
            left += column.width;
            canvas.FillRect({left - 1, headerBounds.top + 2, left, headerBounds.bottom - 2}, theme.Get(core::ThemeSlot::GridLine));
        }
    }
    const int first = (std::max)(0, (bounds.top - Bounds().top - header) / tree_->rowHeight);
    const int last = (std::min)(VisibleCount(), (bounds.bottom - Bounds().top - header + tree_->rowHeight - 1) / tree_->rowHeight);
    for (int row = first; row < last; ++row) {
        const int id = IdAtVisibleRow(row);
        const DuiTreeNode* node = tree_->Find(id);
        if (!node) continue;
        const core::Rect rowBounds{Bounds().left, Bounds().top + header + row * tree_->rowHeight, Bounds().right, Bounds().top + header + (row + 1) * tree_->rowHeight};
        if (tree_->selectionHighlight && IsSelected(id))
            canvas.FillRect(rowBounds, theme.Get(core::ThemeSlot::TreeSelection));
        else if (tree_->hoverHighlight && id == tree_->hoveredId)
            canvas.FillRect(rowBounds, theme.Get(core::ThemeSlot::TreeHover));
        else if (tree_->zebra && row % 2 != 0) {
            canvas.FillRect(rowBounds, theme.Get(core::ThemeSlot::TreeZebra));
            // 交替行边框：浅灰描边，增强行分隔
            constexpr core::Color kZebraBorder{236, 236, 236, 255};
            canvas.StrokeRoundedRect(rowBounds, 0, kZebraBorder, 1.0F);
        }
        const int nodeDepth = tree_->Depth(id);
        if (tree_->columns.empty() && tree_->nodeChecksVisible) {
            const core::Rect nodeCheck{Bounds().left + nodeDepth * tree_->indent + detail::GlyphWidth + 2,
                                       rowBounds.top + (rowBounds.Height() - 14) / 2,
                                       Bounds().left + nodeDepth * tree_->indent + detail::GlyphWidth + 16,
                                       rowBounds.top + (rowBounds.Height() - 14) / 2 + 14};
            PaintNodeCheck(canvas, nodeCheck, NodeCheckState(id), theme.Get(core::ThemeSlot::ListCheckboxBorder),
                           theme.Get(core::ThemeSlot::BrandPrimary));
        }
        if (!tree_->columns.empty()) {
            int left = rowBounds.left;
            for (int column{}; column < ColumnCount(); ++column) {
                const auto& definition = tree_->columns[static_cast<std::size_t>(column)];
                core::Rect cell{left, rowBounds.top, left + definition.width, rowBounds.bottom};
                if (column == 0) {
                    const int glyphLeft = cell.left + tree_->Depth(id) * tree_->indent;
                    if (HasChildren(id))
                        PaintDisclosureGlyph(canvas, {glyphLeft, cell.top, glyphLeft + detail::GlyphWidth, cell.bottom},
                                             node->expanded, textStyle.color);
                    cell.left = glyphLeft + detail::GlyphWidth + detail::TextPadding;
                    if (const auto& icon = tree_->RenderIconAt(id)) {
                        const int iconPx = tree_->iconSize;
                        const int top = cell.top + (cell.Height() - iconPx) / 2;
                        canvas.DrawImage(*icon, {0, 0, icon->Size().width, icon->Size().height}, {cell.left, top, cell.left + iconPx, top + iconPx});
                        cell.left += iconPx + detail::TextPadding;
                    }
                }
                if (definition.kind == DuiTreeColumnKind::CheckBox) {
                    const int size = 14;
                    const int top = cell.top + (cell.Height() - size) / 2;
                    const int checkLeft = cell.left + (cell.Width() - size) / 2;
                    const core::Rect check{checkLeft, top, checkLeft + size, top + size};
                    canvas.StrokeRoundedRect(check, 2, theme.Get(core::ThemeSlot::ListCheckboxBorder), 1.0F);
                    if (CellChecked(id, column))
                        canvas.FillRoundedRect(check, 2, theme.Get(core::ThemeSlot::BrandPrimary));
                }
                else if (definition.kind == DuiTreeColumnKind::ProgressBar) {
                    constexpr int progressHeight = 12;
                    const int top = cell.top + (cell.Height() - progressHeight) / 2;
                    const core::Rect track{cell.left + detail::TextPadding, top, cell.right - detail::TextPadding, top + progressHeight};
                    canvas.FillRoundedRect(track, progressHeight / 2, theme.Get(core::ThemeSlot::TreeProgressTrack));
                    const int progressRight = track.left + track.Width() * CellProgress(id, column) / 100;
                    canvas.FillRoundedRect({track.left, track.top, progressRight, track.bottom}, progressHeight / 2,
                                           theme.Get(core::ThemeSlot::BrandPrimary));
                    const std::string label = CellText(id, column).empty()
                        ? std::to_string(CellProgress(id, column)) + "%"
                        : CellText(id, column);
                    canvas.DrawText(label, track, textStyle, render::DuiTextAlignment::Center, false);
                }
                else if (definition.kind == DuiTreeColumnKind::Hyperlink) {
                    auto linkStyle = textStyle;
                    linkStyle.color = theme.Get(core::ThemeSlot::TextLink);
                    canvas.DrawText(CellText(id, column), {cell.left + detail::TextPadding, cell.top, cell.right - detail::TextPadding, cell.bottom},
                                    linkStyle, definition.alignment, false);
                }
                else if ((definition.kind == DuiTreeColumnKind::Icon || definition.kind == DuiTreeColumnKind::Image)
                         && CellImageAt(id, column)) {
                    const auto& image = CellImageAt(id, column);
                    const auto imageSize = image->Size();
                    const int maxWidth = (std::max)(1, cell.Width() - detail::TextPadding * 2);
                    const int maxHeight = (std::max)(1, cell.Height() - 4);
                    const int width = definition.kind == DuiTreeColumnKind::Icon
                        ? (std::min)(18, maxWidth)
                        : (std::max)(1, (std::min)(maxWidth, imageSize.width * maxHeight / (std::max)(1, imageSize.height)));
                    const int height = definition.kind == DuiTreeColumnKind::Icon
                        ? (std::min)(18, maxHeight)
                        : (std::max)(1, (std::min)(maxHeight, imageSize.height * maxWidth / (std::max)(1, imageSize.width)));
                    const core::Rect destination{cell.left + (cell.Width() - width) / 2, cell.top + (cell.Height() - height) / 2,
                                                 cell.left + (cell.Width() + width) / 2, cell.top + (cell.Height() + height) / 2};
                    canvas.DrawImage(*image, {0, 0, imageSize.width, imageSize.height}, destination);
                }
                else {
                    const core::Rect textBounds{cell.left, cell.top, cell.right - detail::TextPadding, cell.bottom};
                    if (Editing() && tree_->editId == id && tree_->editColumn == column)
                        ysDui::controls::detail::PaintTextInput(canvas, tree_->textInput, {}, {}, textBounds,
                            textStyle, textStyle.color, definition.alignment, false, Focused(),
                            theme.Get(core::ThemeSlot::SelectionBackground), theme.Get(core::ThemeSlot::TextOnSelectedRow));
                    else
                        canvas.DrawText(CellText(id, column), textBounds, textStyle, definition.alignment, false);
                }
                left += definition.width;
            }
            continue;
        }
        const int left = rowBounds.left + nodeDepth * tree_->indent;
        if (HasChildren(id))
            PaintDisclosureGlyph(canvas, {left, rowBounds.top, left + detail::GlyphWidth, rowBounds.bottom},
                                 node->expanded, textStyle.color);
        core::Rect textBounds{left + detail::GlyphWidth + (tree_->nodeChecksVisible ? 20 : 0) + detail::TextPadding, rowBounds.top, rowBounds.right - detail::TextPadding, rowBounds.bottom};
        if (const auto& icon = tree_->RenderIconAt(id)) {
            const int iconPx = tree_->iconSize;
            const int top = rowBounds.top + (rowBounds.Height() - iconPx) / 2;
            canvas.DrawImage(*icon, {0, 0, icon->Size().width, icon->Size().height}, {textBounds.left, top, textBounds.left + iconPx, top + iconPx});
            textBounds.left += iconPx + detail::TextPadding;
        }
        if (!node->rightText.empty()) {
            const int rightWidth = static_cast<int>(node->rightText.size()) * textStyle.pointSize;
            canvas.DrawText(node->rightText, {rowBounds.right - rightWidth - detail::TextPadding, rowBounds.top, rowBounds.right - detail::TextPadding, rowBounds.bottom}, textStyle, render::DuiTextAlignment::End, false);
            textBounds.right -= rightWidth + detail::TextPadding;
        }
        if (tree_->statusColorIds.find(id) != tree_->statusColorIds.end()) {
            canvas.FillEllipse({textBounds.right - 10, rowBounds.top + (tree_->rowHeight - 8) / 2, textBounds.right - 2, rowBounds.top + (tree_->rowHeight + 8) / 2}, node->statusColor);
            textBounds.right -= 14;
        }
        const std::string subtitle = Subtitle(id);
        if (subtitle.empty()) {
            if (Editing() && tree_->editId == id)
                ysDui::controls::detail::PaintTextInput(canvas, tree_->textInput, {}, {}, textBounds,
                    textStyle, textStyle.color, render::DuiTextAlignment::Start, false, Focused(),
                    theme.Get(core::ThemeSlot::SelectionBackground), theme.Get(core::ThemeSlot::TextOnSelectedRow));
            else
                canvas.DrawText(node->label, textBounds, textStyle, render::DuiTextAlignment::Start, false);
            if (NodeLoadState(id) == DuiTreeLoadState::Loading)
                canvas.DrawText("Loading...", {rowBounds.right - 86, rowBounds.top, rowBounds.right - detail::TextPadding, rowBounds.bottom}, subtitleStyle, render::DuiTextAlignment::End, false);
            else if (NodeLoadState(id) == DuiTreeLoadState::Failed)
                canvas.DrawText("Load failed", {rowBounds.right - 86, rowBounds.top, rowBounds.right - detail::TextPadding, rowBounds.bottom}, subtitleStyle, render::DuiTextAlignment::End, false);
            continue;
        }
        const int middle = rowBounds.top + rowBounds.Height() / 2;
        canvas.DrawText(node->label, {textBounds.left, rowBounds.top + 2, textBounds.right, middle + 1},
                        textStyle, render::DuiTextAlignment::Start, false);
        canvas.DrawText(subtitle, {textBounds.left, middle - 1, textBounds.right, rowBounds.bottom - 2},
                        subtitleStyle, render::DuiTextAlignment::Start, false);
    }
    if (tree_->dragging) {
        core::Color preview = theme.Get(core::ThemeSlot::TreeSelection);
        preview.alpha = 90;
        for (const int sourceId : tree_->dragSourceIds) {
            const int sourceRow = VisibleRow(sourceId);
            if (sourceRow < first || sourceRow >= last) continue;
            const core::Rect sourceBounds{Bounds().left, Bounds().top + header + sourceRow * tree_->rowHeight,
                                          Bounds().right, Bounds().top + header + (sourceRow + 1) * tree_->rowHeight};
            canvas.FillRect(sourceBounds, preview);
        }
        if (tree_->dragTargetId >= 0) {
            const int targetRow = VisibleRow(tree_->dragTargetId);
            if (targetRow >= first && targetRow < last) {
                const core::Rect targetBounds{Bounds().left, Bounds().top + header + targetRow * tree_->rowHeight,
                                              Bounds().right, Bounds().top + header + (targetRow + 1) * tree_->rowHeight};
                if (!tree_->dragTargetValid) {
                    const core::Color invalid{150, 150, 150, 255};
                    constexpr int dash = 6;
                    for (int x = targetBounds.left; x < targetBounds.right; x += dash * 2)
                        canvas.FillRect({x, targetBounds.top, (std::min)(x + dash, targetBounds.right), targetBounds.top + 2}, invalid);
                    for (int x = targetBounds.left; x < targetBounds.right; x += dash * 2)
                        canvas.FillRect({x, targetBounds.bottom - 2, (std::min)(x + dash, targetBounds.right), targetBounds.bottom}, invalid);
                    for (int y = targetBounds.top + dash * 2; y < targetBounds.bottom - dash; y += dash * 2) {
                        canvas.FillRect({targetBounds.left, y, targetBounds.left + 2,
                                         (std::min)(y + dash, targetBounds.bottom)}, invalid);
                        canvas.FillRect({targetBounds.right - 2, y, targetBounds.right,
                                         (std::min)(y + dash, targetBounds.bottom)}, invalid);
                    }
                }
                else if (tree_->dragTargetPosition == DuiTreeDropPosition::Inside) {
                    core::Color inside = theme.Get(core::ThemeSlot::TreeSelection);
                    inside.alpha = 70;
                    canvas.FillRect(targetBounds, inside);
                    canvas.StrokeRoundedRect({targetBounds.left + 1, targetBounds.top + 1,
                                              targetBounds.right - 1, targetBounds.bottom - 1}, 0,
                                             theme.Get(core::ThemeSlot::BrandPrimary), 2.0F);
                }
                else {
                    const int y = tree_->dragTargetPosition == DuiTreeDropPosition::Before
                        ? targetBounds.top : targetBounds.bottom - 2;
                    canvas.FillRect({targetBounds.left, y, targetBounds.right, y + 2},
                                    theme.Get(core::ThemeSlot::BrandPrimary));
                }
            }
        }
    }
    if (tree_->borderVisible)
        canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::GridBorder), 1.0F);
}

} // namespace ysDui::controls::list
