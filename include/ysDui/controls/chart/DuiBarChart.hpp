/**
 * 文件名：DuiBarChart.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明柱状图控件——按类别比较数值，用于 BOM 数量等分类统计。
 */
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::chart {

/** 一根柱子。 */
struct DuiBar final
{
    std::string label;
    double value{};
    core::Color color; // alpha 为 0 时按序号取主题配色
    bool hasColor{};
};

/**
 * 柱状图。
 * 纵轴范围默认按数据自动取整；柱子等宽等距排布。
 */
class DuiBarChart final : public core::Control, public render::DuiRenderable {
public:
    DuiBarChart();
    ~DuiBarChart() override;
    DuiBarChart(const DuiBarChart&) = delete;
    DuiBarChart& operator=(const DuiBarChart&) = delete;
    DuiBarChart(DuiBarChart&&) noexcept;
    DuiBarChart& operator=(DuiBarChart&&) noexcept;

    /** 追加一根柱子。 */
    void AddBar(std::string label, double value, core::Color color = {});
    void ClearBars();
    [[nodiscard]] int BarCount() const;
    [[nodiscard]] DuiBar BarAt(int index) const;
    /** 固定纵轴范围；未设置时按数据自动取整。 */
    void SetValueRange(double minimum, double maximum);
    void ClearValueRange();
    /** 设置柱子之间的间距（逻辑像素，占槽位的固定留白）。 */
    void SetBarGap(int pixels);
    void SetGridDivisions(int divisions);
    /** 设置是否在柱顶显示数值。 */
    void SetShowValues(bool show);
    void SetTextStyle(render::DuiTextStyle style);

    /** @return 当前生效的纵轴范围（自动或显式）。 */
    [[nodiscard]] std::pair<double, double> ValueRange() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> chart_;
};

} // namespace ysDui::controls::chart
