/**
 * 文件名：DuiPieChart.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明饼图控件——按占比展示构成，支持环形样式与外部图例。
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::chart {

/** 一个扇区。 */
struct DuiPieSlice final
{
    std::string label;
    double value{};
    core::Color color; // alpha 为 0 时按序号取主题配色
    bool hasColor{};
};

/**
 * 饼图 / 环形图。
 * 数值为负或非有限的扇区会被忽略；总和为 0 时不绘制扇区。
 * 扇区用多边形按角度分段逼近（渲染层没有弧线路径指令）。
 */
class DuiPieChart final : public core::Control, public render::DuiRenderable {
public:
    DuiPieChart();
    ~DuiPieChart() override;
    DuiPieChart(const DuiPieChart&) = delete;
    DuiPieChart& operator=(const DuiPieChart&) = delete;
    DuiPieChart(DuiPieChart&&) noexcept;
    DuiPieChart& operator=(DuiPieChart&&) noexcept;

    /** 追加一个扇区。 */
    void AddSlice(std::string label, double value, core::Color color = {});
    void ClearSlices();
    [[nodiscard]] int SliceCount() const;
    [[nodiscard]] DuiPieSlice SliceAt(int index) const;
    /** 设置内圈半径占比（0 = 实心饼图，0.6 = 典型环形图）；范围 [0, 0.95]。 */
    void SetInnerRadiusRatio(double ratio);
    [[nodiscard]] double InnerRadiusRatio() const;
    /** 设置是否在右侧绘制图例（含各扇区占比）。 */
    void SetShowLegend(bool show);
    /** 设置扇区起始角（度，0 度为 3 点钟方向，顺时针为正）。 */
    void SetStartAngle(double degrees);
    void SetTextStyle(render::DuiTextStyle style);

    /** @return 扇区圆心。 */
    [[nodiscard]] core::Point Center() const;
    /** @return 扇区外半径。 */
    [[nodiscard]] int Radius() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> chart_;
};

} // namespace ysDui::controls::chart
