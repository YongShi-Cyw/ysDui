#include "ysDui/controls/layout/DuiUniformGrid.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/render/DuiPaintChildren.hpp"

namespace ysDui::controls::layout {
namespace {
int nonNegative(int value)
{
    return (std::max)(0, value);
}
} // namespace

class DuiUniformGrid::Impl {
public:
    int columns{1};
    int rows{}; // 0 = 不限行数
    int gap{};
    int paddingLeft{};
    int paddingTop{};
    int paddingRight{};
    int paddingBottom{};
};

DuiUniformGrid::DuiUniformGrid() : grid_(std::make_unique<Impl>()) {}
DuiUniformGrid::~DuiUniformGrid() = default;
DuiUniformGrid::DuiUniformGrid(DuiUniformGrid&&) noexcept = default;
DuiUniformGrid& DuiUniformGrid::operator=(DuiUniformGrid&&) noexcept = default;

void DuiUniformGrid::SetGrid(int columns, int rows)
{
    grid_->columns = (std::max)(1, columns);
    grid_->rows = nonNegative(rows);
}
int DuiUniformGrid::Columns() const { return grid_->columns; }
int DuiUniformGrid::Rows() const { return grid_->rows; }
void DuiUniformGrid::SetGap(int pixels) { grid_->gap = nonNegative(pixels); }
int DuiUniformGrid::Gap() const { return grid_->gap; }
void DuiUniformGrid::SetPadding(int left, int top, int right, int bottom)
{
    grid_->paddingLeft = nonNegative(left);
    grid_->paddingTop = nonNegative(top);
    grid_->paddingRight = nonNegative(right);
    grid_->paddingBottom = nonNegative(bottom);
}
void DuiUniformGrid::SetPadding(int all) { SetPadding(all, all, all, all); }
core::Rect DuiUniformGrid::Padding() const
{
    return {grid_->paddingLeft, grid_->paddingTop, grid_->paddingRight, grid_->paddingBottom};
}

void DuiUniformGrid::AddChild(std::unique_ptr<core::Control> child)
{
    if (child == nullptr)
        return;
    core::Control::AddChild(std::move(child));
}

int DuiUniformGrid::VisibleCount() const
{
    int count{};
    for (const auto& child : Children())
    {
        if (child->Visible())
            ++count;
    }
    return count;
}

void DuiUniformGrid::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    const int columns = grid_->columns;
    // 每个单元格的步长 = 单元格尺寸 + gap；先按可用宽度定单元宽，再均分给每列
    const int innerWidth = nonNegative(bounds.Width() - grid_->paddingLeft - grid_->paddingRight);
    const int innerHeight = nonNegative(bounds.Height() - grid_->paddingTop - grid_->paddingBottom);
    const int totalGapX = (columns > 0 ? columns - 1 : 0) * grid_->gap;
    const int cellWidth = columns > 0 ? nonNegative(innerWidth - totalGapX) / columns : 0;

    const int visible = VisibleCount();
    const int rows = grid_->rows > 0
        ? grid_->rows
        : columns > 0 ? (visible + columns - 1) / columns : 0;
    if (rows <= 0)
        return;
    const int totalGapY = (rows - 1) * grid_->gap;
    const int cellHeight = nonNegative(innerHeight - totalGapY) / rows;

    int index{};
    for (const auto& child : Children())
    {
        if (!child->Visible())
            continue;
        const int row = columns > 0 ? index / columns : 0;
        const int column = columns > 0 ? index % columns : 0;
        if (grid_->rows > 0 && row >= grid_->rows)
            break;
        const int left = bounds.left + grid_->paddingLeft + column * (cellWidth + grid_->gap);
        const int top = bounds.top + grid_->paddingTop + row * (cellHeight + grid_->gap);
        child->Layout({left, top, left + cellWidth, top + cellHeight});
        ++index;
    }
}

core::Size DuiUniformGrid::DesiredSize() const
{
    // 无固定内容尺寸：单元格由容器均分，这里只保留内边距与 gap 造成的固定开销。
    // 行数上限已知时按上限报高，便于外层 DuiLayoutHint::Auto() 得到稳定结果。
    const int columns = grid_->columns;
    const int visible = VisibleCount();
    const int rows = grid_->rows > 0 ? grid_->rows : columns > 0 ? (visible + columns - 1) / columns : 0;
    const int width = grid_->paddingLeft + grid_->paddingRight
        + (columns > 0 ? columns - 1 : 0) * grid_->gap;
    const int height = grid_->paddingTop + grid_->paddingBottom
        + (rows > 0 ? rows - 1 : 0) * grid_->gap;
    return {width, height};
}

void DuiUniformGrid::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (EffectivelyVisible())
        render::PaintChildren(*this, canvas, dirty);
}

} // namespace ysDui::controls::layout
