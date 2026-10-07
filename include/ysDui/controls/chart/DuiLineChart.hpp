/**
 * 文件名：DuiLineChart.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明折线图控件——多系列数值趋势，用于壁厚分布等连续数据。
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

/** 一条折线系列。 */
struct DuiLineSeries final
{
    std::string name;
    std::vector<double> values;
    core::Color color;    // alpha 为 0 时按序号取主题配色
    bool hasColor{};      // 是否使用显式颜色
};

/**
 * 折线图。
 * 纵轴范围默认按数据自动取整；横轴按数据点等分，可提供类别标签。
 */
class DuiLineChart final : public core::Control, public render::DuiRenderable {
public:
    DuiLineChart();
    ~DuiLineChart() override;
    DuiLineChart(const DuiLineChart&) = delete;
    DuiLineChart& operator=(const DuiLineChart&) = delete;
    DuiLineChart(DuiLineChart&&) noexcept;
    DuiLineChart& operator=(DuiLineChart&&) noexcept;

    /** 追加一条系列；返回其序号，越界失败时返回 -1。 */
    int AddSeries(std::string name, std::vector<double> values, core::Color color = {});
    void ClearSeries();
    [[nodiscard]] int SeriesCount() const;
    [[nodiscard]] DuiLineSeries SeriesAt(int index) const;
    /** 设置横轴类别标签（数量应等于点数；不足处留空）。 */
    void SetCategoryLabels(std::vector<std::string> labels);
    /** 固定纵轴范围；未设置时按数据自动取整。 */
    void SetValueRange(double minimum, double maximum);
    void ClearValueRange();
    /** 设置网格分段数（含上下边界），至少 1。 */
    void SetGridDivisions(int divisions);
    /** 设置是否绘制数据点圆点。 */
    void SetShowPoints(bool show);
    /** 设置是否在顶部绘制图例。 */
    void SetShowLegend(bool show);
    void SetLineWidth(int pixels);
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
