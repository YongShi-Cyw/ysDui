#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::layout {

enum class DuiLayoutAlign {
    Near,
    Center,
    Far,
    Fill,
};

struct DuiLayoutHint final {
    /** 主轴尺寸取自子控件 DesiredSize 的哨兵值（见 DuiLayoutHint::Auto()）。 */
    static constexpr int AutoMain = -2;
    int fixedMain{-1};
    int fixedCross{-1};
    int weight{1};
    int marginLeft{};
    int marginTop{};
    int marginRight{};
    int marginBottom{};
    DuiLayoutAlign mainAlign{DuiLayoutAlign::Fill};
    DuiLayoutAlign crossAlign{DuiLayoutAlign::Fill};

    DuiLayoutHint& Fixed(int main, int cross = -1);
    DuiLayoutHint& Flexible(int value = 1);
    /**
     * 主轴按子控件的 `DesiredSize()` 自适应（内容驱动尺寸）。
     * 与 `Fixed`/`Flexible` 并列的第三种主尺寸来源；未重写 `DesiredSize()` 的控件
     * 返回 `{0, 0}`，此时主轴退化为 0。
     */
    DuiLayoutHint& Auto();
    DuiLayoutHint& Margin(int left, int top, int right, int bottom);
    DuiLayoutHint& Margin(int all);
    DuiLayoutHint& MainAlign(DuiLayoutAlign align);
    DuiLayoutHint& CrossAlign(DuiLayoutAlign align);
};

class DuiLayout : public core::Control, public render::DuiRenderable {
public:
    DuiLayout();
    ~DuiLayout() override;
    DuiLayout(const DuiLayout&) = delete;
    DuiLayout& operator=(const DuiLayout&) = delete;
    DuiLayout(DuiLayout&&) = delete;
    DuiLayout& operator=(DuiLayout&&) = delete;

    void SetPadding(int left, int top, int right, int bottom);
    void SetPadding(int all);
    void SetGap(int pixels);
    [[nodiscard]] int Gap() const;
    void AddChild(std::unique_ptr<core::Control> child, DuiLayoutHint hint);
    [[nodiscard]] std::unique_ptr<core::Control> RemoveChild(core::Control* child);
    void SetHint(core::Control* child, DuiLayoutHint hint);
    [[nodiscard]] DuiLayoutHint HintFor(const core::Control* child) const;
    [[nodiscard]] core::Rect InnerBounds() const;
    [[nodiscard]] static core::Rect ApplyHint(core::Rect cell, const DuiLayoutHint& hint,
                                              bool horizontal, core::Size desired = {});
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> layout_;
};

class DuiHBox final : public DuiLayout {
public:
    void Layout(core::Rect bounds);
};

class DuiVBox final : public DuiLayout {
public:
    void Layout(core::Rect bounds);
};

class DuiGrid final : public DuiLayout {
public:
    DuiGrid();
    ~DuiGrid() override;
    DuiGrid(const DuiGrid&) = delete;
    DuiGrid& operator=(const DuiGrid&) = delete;
    DuiGrid(DuiGrid&&) = delete;
    DuiGrid& operator=(DuiGrid&&) = delete;
    void SetGrid(int rows, int columns);
    void SetColumnWidth(int column, int pixels);
    void SetColumnWeight(int column, int weight);
    void SetRowHeight(int row, int pixels);
    void SetRowWeight(int row, int weight);
    void SetCell(core::Control* child, int row, int column, int rowSpan = 1, int columnSpan = 1);
    void Layout(core::Rect bounds);

private:
    class GridImpl;
    std::unique_ptr<GridImpl> grid_;
};

} // namespace ysDui::controls::layout
