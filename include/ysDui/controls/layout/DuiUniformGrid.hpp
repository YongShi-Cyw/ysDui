/**
 * 文件名：DuiUniformGrid.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明等分网格容器——所有单元格尺寸相同，按行列均匀铺满可用区域。
 */
#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::layout {

/**
 * 等分网格容器。
 * 与 DuiGrid 的区别：DuiGrid 支持固定尺寸与权重列/行、单元格跨行列；
 * DuiUniformGrid 舍弃这些能力，只做「所有单元格等宽等高」这一种最简单布局，
 * 适合色号板、图标墙、工具按钮阵列等场景。
 */
class DuiUniformGrid final : public core::Control, public render::DuiRenderable {
public:
    DuiUniformGrid();
    ~DuiUniformGrid() override;
    DuiUniformGrid(const DuiUniformGrid&) = delete;
    DuiUniformGrid& operator=(const DuiUniformGrid&) = delete;
    DuiUniformGrid(DuiUniformGrid&&) noexcept;
    DuiUniformGrid& operator=(DuiUniformGrid&&) noexcept;

    /**
     * 设置列数与行数上限。
     * @param columns 列数，至少为 1。
     * @param rows 行数；传 0（默认）表示不限行数，按列数自动换行。
     */
    void SetGrid(int columns, int rows = 0);
    /** @return 当前列数。 */
    [[nodiscard]] int Columns() const;
    /** @return 当前行数上限；0 表示不限（自动换行）。 */
    [[nodiscard]] int Rows() const;
    void SetGap(int pixels);
    [[nodiscard]] int Gap() const;
    void SetPadding(int left, int top, int right, int bottom);
    void SetPadding(int all);
    /** @return 内边距；顺序为 left/top/right/bottom。 */
    [[nodiscard]] core::Rect Padding() const;

    /** 追加子控件；调用方不保留所有权。 */
    void AddChild(std::unique_ptr<core::Control> child);

    void Layout(core::Rect bounds);
    /** @return 单元格期望尺寸（gap 固定步长下的整体内容尺寸）。 */
    [[nodiscard]] core::Size DesiredSize() const override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    /** @return 参与布局的可见子控件数量。 */
    [[nodiscard]] int VisibleCount() const;

    class Impl;
    std::unique_ptr<Impl> grid_;
};

} // namespace ysDui::controls::layout
