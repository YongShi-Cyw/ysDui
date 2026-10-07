#include "ysDui/controls/layout/DuiLayout.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/render/DuiPaintChildren.hpp"

namespace ysDui::controls::layout {

class DuiLayout::Impl {
public:
    struct Entry { core::Control* child{}; DuiLayoutHint hint; };
    int left{};
    int top{};
    int right{};
    int bottom{};
    int gap{};
    std::vector<Entry> hints;
};

DuiLayoutHint& DuiLayoutHint::Fixed(int main, int cross) { fixedMain = (std::max)(0, main); fixedCross = cross; return *this; }
DuiLayoutHint& DuiLayoutHint::Flexible(int value) { fixedMain = -1; weight = (std::max)(1, value); return *this; }
DuiLayoutHint& DuiLayoutHint::Auto() { fixedMain = AutoMain; return *this; }
DuiLayoutHint& DuiLayoutHint::Margin(int left, int top, int right, int bottom) { marginLeft = left; marginTop = top; marginRight = right; marginBottom = bottom; return *this; }
DuiLayoutHint& DuiLayoutHint::Margin(int all) { return Margin(all, all, all, all); }
DuiLayoutHint& DuiLayoutHint::MainAlign(DuiLayoutAlign align) { mainAlign = align; return *this; }
DuiLayoutHint& DuiLayoutHint::CrossAlign(DuiLayoutAlign align) { crossAlign = align; return *this; }

DuiLayout::DuiLayout() : layout_(std::make_unique<Impl>()) {}
DuiLayout::~DuiLayout() = default;
void DuiLayout::SetPadding(int left, int top, int right, int bottom) { layout_->left = (std::max)(0, left); layout_->top = (std::max)(0, top); layout_->right = (std::max)(0, right); layout_->bottom = (std::max)(0, bottom); }
void DuiLayout::SetPadding(int all) { SetPadding(all, all, all, all); }
void DuiLayout::SetGap(int pixels) { layout_->gap = (std::max)(0, pixels); }
int DuiLayout::Gap() const { return layout_->gap; }
void DuiLayout::AddChild(std::unique_ptr<core::Control> child, DuiLayoutHint hint) { if (!child) return; core::Control* raw = child.get(); core::Control::AddChild(std::move(child)); layout_->hints.push_back({raw, hint}); }
std::unique_ptr<core::Control> DuiLayout::RemoveChild(core::Control* child) { auto result = core::Control::RemoveChild(child); std::erase_if(layout_->hints, [child](const Impl::Entry& entry) { return entry.child == child; }); return result; }
void DuiLayout::SetHint(core::Control* child, DuiLayoutHint hint) { for (Impl::Entry& entry : layout_->hints) { if (entry.child == child) { entry.hint = hint; return; } } if (child) layout_->hints.push_back({child, hint}); }
DuiLayoutHint DuiLayout::HintFor(const core::Control* child) const { for (const Impl::Entry& entry : layout_->hints) { if (entry.child == child) return entry.hint; } return {}; }
core::Rect DuiLayout::InnerBounds() const { const core::Rect bounds = Bounds(); return {bounds.left + layout_->left, bounds.top + layout_->top, (std::max)(bounds.left + layout_->left, bounds.right - layout_->right), (std::max)(bounds.top + layout_->top, bounds.bottom - layout_->bottom)}; }

core::Rect DuiLayout::ApplyHint(core::Rect cell, const DuiLayoutHint& hint, bool horizontal,
                                core::Size desired)
{
    cell.left += hint.marginLeft; cell.top += hint.marginTop; cell.right -= hint.marginRight; cell.bottom -= hint.marginBottom;
    const int mainSize = horizontal ? cell.Width() : cell.Height();
    const int crossSize = horizontal ? cell.Height() : cell.Width();
    const int desiredAutoMain = horizontal ? desired.width : desired.height;
    const int desiredAutoCross = horizontal ? desired.height : desired.width;
    const int desiredMain = hint.fixedMain == DuiLayoutHint::AutoMain ? desiredAutoMain
        : hint.fixedMain >= 0 ? hint.fixedMain : mainSize;
    const int desiredCross = hint.fixedCross == DuiLayoutHint::AutoMain ? desiredAutoCross
        : hint.fixedCross >= 0 ? hint.fixedCross : crossSize;
    const auto align = [](int start, int length, int desired, DuiLayoutAlign alignment) {
        if (alignment == DuiLayoutAlign::Fill || desired >= length) return std::pair{start, start + length};
        if (alignment == DuiLayoutAlign::Center) return std::pair{start + (length - desired) / 2, start + (length - desired) / 2 + desired};
        if (alignment == DuiLayoutAlign::Far) return std::pair{start + length - desired, start + length};
        return std::pair{start, start + desired};
    };
    const auto main = align(horizontal ? cell.left : cell.top, mainSize, desiredMain, hint.mainAlign);
    const auto cross = align(horizontal ? cell.top : cell.left, crossSize, desiredCross, hint.crossAlign);
    return horizontal ? core::Rect{main.first, cross.first, main.second, cross.second} : core::Rect{cross.first, main.first, cross.second, main.second};
}

void DuiLayout::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (EffectivelyVisible())
        render::PaintChildren(*this, canvas, dirty);
}

namespace {
void LayoutLinear(DuiLayout& layout, core::Rect bounds, bool horizontal)
{
    layout.SetBounds(bounds);
    const core::Rect inner = layout.InnerBounds();
    std::vector<core::Control*> visible;
    int fixed{};
    int weights{};
    for (const auto& child : layout.Children()) {
        if (!child->Visible()) continue;
        visible.push_back(child.get());
        const DuiLayoutHint hint = layout.HintFor(child.get());
        const int margins = horizontal ? hint.marginLeft + hint.marginRight : hint.marginTop + hint.marginBottom;
        if (hint.fixedMain == DuiLayoutHint::AutoMain) {
            // 内容驱动：主轴占用子控件自报的首选尺寸
            const core::Size desired = child->DesiredSize();
            fixed += (horizontal ? desired.width : desired.height) + margins;
        }
        else if (hint.fixedMain >= 0) fixed += hint.fixedMain + margins;
        else weights += (std::max)(1, hint.weight) + 0;
    }
    const int length = horizontal ? inner.Width() : inner.Height();
    const int remaining = (std::max)(0, length - fixed - (std::max)(0, static_cast<int>(visible.size()) - 1) * layout.Gap());
    int cursor = horizontal ? inner.left : inner.top;
    int distributed{};
    int lastFlexible{-1};
    for (int index = 0; index < static_cast<int>(visible.size()); ++index) {
        // 仅 Flexible（fixedMain == -1）参与余量分配；Auto 与 Fixed 都已计入 fixed
        if (layout.HintFor(visible[index]).fixedMain == -1)
            lastFlexible = index;
    }
    for (int index = 0; index < static_cast<int>(visible.size()); ++index) {
        core::Control* child = visible[index];
        const DuiLayoutHint hint = layout.HintFor(child);
        const core::Size desired = child->DesiredSize();
        const bool flexible = hint.fixedMain == -1;
        int main = hint.fixedMain == DuiLayoutHint::AutoMain
            ? (horizontal ? desired.width : desired.height)
            : hint.fixedMain >= 0 ? hint.fixedMain
                                  : remaining * (std::max)(1, hint.weight) / (std::max)(1, weights);
        if (flexible && index == lastFlexible) main += remaining - distributed - main;
        if (flexible) distributed += main;
        const int margins = horizontal ? hint.marginLeft + hint.marginRight : hint.marginTop + hint.marginBottom;
        const core::Rect cell = horizontal ? core::Rect{cursor, inner.top, cursor + main + margins, inner.bottom} : core::Rect{inner.left, cursor, inner.right, cursor + main + margins};
        child->Layout(DuiLayout::ApplyHint(cell, hint, horizontal, desired));
        cursor += main + margins + (index + 1 < static_cast<int>(visible.size()) ? layout.Gap() : 0);
    }
}
}

void DuiHBox::Layout(core::Rect bounds) { LayoutLinear(*this, bounds, true); }
void DuiVBox::Layout(core::Rect bounds) { LayoutLinear(*this, bounds, false); }

class DuiGrid::GridImpl {
public:
    struct Axis { int fixed{-1}; int weight{1}; };
    struct Cell { core::Control* child{}; int row{}; int column{}; int rowSpan{1}; int columnSpan{1}; };
    int rows{1};
    int columns{1};
    std::vector<Axis> rowAxes{1};
    std::vector<Axis> columnAxes{1};
    std::vector<Cell> cells;
};

DuiGrid::DuiGrid() : grid_(std::make_unique<GridImpl>()) {}
DuiGrid::~DuiGrid() = default;
void DuiGrid::SetGrid(int rows, int columns) { grid_->rows = (std::max)(1, rows); grid_->columns = (std::max)(1, columns); grid_->rowAxes.assign(grid_->rows, {}); grid_->columnAxes.assign(grid_->columns, {}); }
void DuiGrid::SetColumnWidth(int column, int pixels) { if (column >= 0 && column < grid_->columns) grid_->columnAxes[column].fixed = (std::max)(0, pixels); }
void DuiGrid::SetColumnWeight(int column, int weight) { if (column >= 0 && column < grid_->columns) grid_->columnAxes[column] = {-1, (std::max)(1, weight)}; }
void DuiGrid::SetRowHeight(int row, int pixels) { if (row >= 0 && row < grid_->rows) grid_->rowAxes[row].fixed = (std::max)(0, pixels); }
void DuiGrid::SetRowWeight(int row, int weight) { if (row >= 0 && row < grid_->rows) grid_->rowAxes[row] = {-1, (std::max)(1, weight)}; }
void DuiGrid::SetCell(core::Control* child, int row, int column, int rowSpan, int columnSpan) { if (!child) return; row = std::clamp(row, 0, grid_->rows - 1); column = std::clamp(column, 0, grid_->columns - 1); for (GridImpl::Cell& cell : grid_->cells) { if (cell.child == child) { cell = {child, row, column, (std::max)(1, rowSpan), (std::max)(1, columnSpan)}; return; } } grid_->cells.push_back({child, row, column, (std::max)(1, rowSpan), (std::max)(1, columnSpan)}); }

void DuiGrid::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    const core::Rect inner = InnerBounds();
    const auto distribute = [](const std::vector<GridImpl::Axis>& axes, int total, int gap) { std::vector<int> sizes(axes.size()); int fixed{}; int weights{}; for (const auto& axis : axes) { if (axis.fixed >= 0) fixed += axis.fixed; else weights += axis.weight; } int remaining = (std::max)(0, total - static_cast<int>(axes.size() - 1) * gap - fixed); int used{}; for (int index = 0; index < static_cast<int>(axes.size()); ++index) { sizes[index] = axes[index].fixed >= 0 ? axes[index].fixed : remaining * axes[index].weight / (std::max)(1, weights); if (axes[index].fixed < 0) used += sizes[index]; } for (int index = static_cast<int>(axes.size()) - 1; index >= 0 && used < remaining; --index) { if (axes[index].fixed < 0) { sizes[index] += remaining - used; break; } } return sizes; };
    const std::vector<int> widths = distribute(grid_->columnAxes, inner.Width(), Gap());
    const std::vector<int> heights = distribute(grid_->rowAxes, inner.Height(), Gap());
    std::vector<int> x(grid_->columns), y(grid_->rows); for (int index = 0, value = inner.left; index < grid_->columns; ++index) { x[index] = value; value += widths[index] + Gap(); } for (int index = 0, value = inner.top; index < grid_->rows; ++index) { y[index] = value; value += heights[index] + Gap(); }
    for (const GridImpl::Cell& cell : grid_->cells) { const int lastRow = (std::min)(grid_->rows - 1, cell.row + cell.rowSpan - 1); const int lastColumn = (std::min)(grid_->columns - 1, cell.column + cell.columnSpan - 1); cell.child->Layout(ApplyHint({x[cell.column], y[cell.row], x[lastColumn] + widths[lastColumn], y[lastRow] + heights[lastRow]}, HintFor(cell.child), true, cell.child->DesiredSize())); }
}

} // namespace ysDui::controls::layout
