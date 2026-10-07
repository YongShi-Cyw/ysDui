#include "ysDui/render/DuiNinePatch.hpp"

#include <algorithm>

namespace ysDui::render {
namespace {
DuiNinePatchInsets ClampForSize(core::Size size, DuiNinePatchInsets insets)
{
    insets.left = (std::max)(0, insets.left);
    insets.top = (std::max)(0, insets.top);
    insets.right = (std::max)(0, insets.right);
    insets.bottom = (std::max)(0, insets.bottom);
    if (size.width > 0 && insets.left + insets.right > size.width) {
        const int total = insets.left + insets.right;
        insets.left = insets.left * size.width / total;
        insets.right = size.width - insets.left;
    }
    if (size.height > 0 && insets.top + insets.bottom > size.height) {
        const int total = insets.top + insets.bottom;
        insets.top = insets.top * size.height / total;
        insets.bottom = size.height - insets.top;
    }
    return insets;
}

void SetCells(DuiNinePatchCells& cells, core::Size source, core::Rect destination,
              DuiNinePatchInsets sourceInsets, DuiNinePatchInsets destinationInsets)
{
    const int sourceX[] = {0, sourceInsets.left, source.width - sourceInsets.right, source.width};
    const int sourceY[] = {0, sourceInsets.top, source.height - sourceInsets.bottom, source.height};
    const int destinationX[] = {destination.left, destination.left + destinationInsets.left,
                                destination.right - destinationInsets.right, destination.right};
    const int destinationY[] = {destination.top, destination.top + destinationInsets.top,
                                destination.bottom - destinationInsets.bottom, destination.bottom};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            cells[static_cast<std::size_t>(row * 3 + column)] = {
                {sourceX[column], sourceY[row], sourceX[column + 1], sourceY[row + 1]},
                {destinationX[column], destinationY[row], destinationX[column + 1], destinationY[row + 1]}};
        }
    }
}
} // namespace

DuiNinePatchInsets ClampNinePatchInsets(core::Size source, DuiNinePatchInsets insets)
{
    return ClampForSize(source, insets);
}

DuiNinePatchCells ComputeNinePatchCells(core::Size source, core::Rect destination, DuiNinePatchInsets insets)
{
    DuiNinePatchCells cells{};
    if (source.Empty() || destination.Empty()) return cells;
    const auto sourceInsets = ClampForSize(source, insets);
    const auto destinationInsets = ClampForSize({destination.Width(), destination.Height()}, sourceInsets);
    SetCells(cells, source, destination, sourceInsets, destinationInsets);
    return cells;
}

DuiNinePatchCells ComputeNinePatchCells(core::Size source, core::Rect destination,
                                        DuiNinePatchInsets sourceInsets, DuiNinePatchInsets destinationInsets)
{
    DuiNinePatchCells cells{};
    if (source.Empty() || destination.Empty()) return cells;
    SetCells(cells, source, destination, ClampForSize(source, sourceInsets),
             ClampForSize({destination.Width(), destination.Height()}, destinationInsets));
    return cells;
}

void DrawNinePatch(Canvas& canvas, const DuiImage& image, core::Rect destination, DuiNinePatchInsets insets)
{
    if (image.Empty()) return;
    for (const auto& cell : ComputeNinePatchCells(image.Size(), destination, insets))
        if (!cell.source.Empty() && !cell.destination.Empty()) canvas.DrawImage(image, cell.source, cell.destination);
}

void DrawNinePatch(Canvas& canvas, const DuiImage& image, core::Rect destination,
                   DuiNinePatchInsets sourceInsets, DuiNinePatchInsets destinationInsets)
{
    if (image.Empty()) return;
    for (const auto& cell : ComputeNinePatchCells(image.Size(), destination, sourceInsets, destinationInsets))
        if (!cell.source.Empty() && !cell.destination.Empty()) canvas.DrawImage(image, cell.source, cell.destination);
}

} // namespace ysDui::render
