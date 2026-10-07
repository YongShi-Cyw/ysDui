/**
 * 文件名：DuiChartShared.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：图表控件共用的绘图数学与图例绘制（私有头，不对外暴露）。
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiPath.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::chart::detail {

constexpr int AxisGutter = 40;      // 左侧数值轴刻度占位
constexpr int AxisLabelHeight = 16; // 底部类别标签高度
constexpr int LegendHeight = 18;    // 图例行高
constexpr int LegendSwatch = 8;     // 图例色块边长

/**
 * 计算绘图区。
 * @param bounds 控件矩形
 * @param withValueAxis 是否预留左侧数值刻度
 * @param withCategoryAxis 是否预留底部类别标签
 * @param legendRows 顶部图例行数
 */
[[nodiscard]] inline core::Rect PlotArea(core::Rect bounds, bool withValueAxis,
                                        bool withCategoryAxis, int legendRows)
{
    const int left = bounds.left + (withValueAxis ? AxisGutter : 4);
    const int top = bounds.top + 4 + legendRows * LegendHeight;
    const int right = bounds.right - 8;
    const int bottom = bounds.bottom - (withCategoryAxis ? AxisLabelHeight : 4);
    return {left, top, (std::max)(left, right), (std::max)(top, bottom)};
}

/** @return 按序号循环取用的系列配色（取自主题，深浅色自动适配）。 */
[[nodiscard]] inline core::Color SeriesColor(const core::DuiTheme& theme, int index)
{
    switch (index % 6)
    {
    case 0: return theme.Get(core::ThemeSlot::BrandPrimary);
    case 1: return theme.Get(core::ThemeSlot::StatusOnline);
    case 2: return theme.Get(core::ThemeSlot::StatusAway);
    case 3: return theme.Get(core::ThemeSlot::Danger);
    case 4: return theme.Get(core::ThemeSlot::LinkVisited);
    default: return theme.Get(core::ThemeSlot::StatusOffline);
    }
}

/**
 * 把值域扩展为"整齐"的上下界。
 * 用途：避免刻度落在 3.714 这类数值上；全零或无效数据退化为 {0, 1}。
 */
inline void NiceRange(double minimum, double maximum, double& outMinimum, double& outMaximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum))
    {
        outMinimum = 0.0;
        outMaximum = 1.0;
        return;
    }
    if (minimum > maximum)
        std::swap(minimum, maximum);
    if (maximum - minimum <= 0.0)
    {
        // 单值或全等：给一个对称的可见区间
        const double magnitude = std::abs(maximum) > 0.0 ? std::abs(maximum) : 1.0;
        outMinimum = maximum - magnitude * 0.5;
        outMaximum = maximum + magnitude * 0.5;
        return;
    }
    const double span = maximum - minimum;
    const double step = std::pow(10.0, std::floor(std::log10(span / 4.0)));
    outMinimum = std::floor(minimum / step) * step;
    outMaximum = std::ceil(maximum / step) * step;
    if (outMaximum - outMinimum <= 0.0)
        outMaximum = outMinimum + step;
}

/** 把数值映射为绘图区内的 Y 坐标（Y 轴向下，值大者在上）。 */
[[nodiscard]] inline int ValueToY(double value, double minimum, double maximum, core::Rect plot)
{
    const double span = maximum - minimum;
    const double ratio = span > 0.0 ? (value - minimum) / span : 0.0;
    const double clamped = (std::clamp)(ratio, 0.0, 1.0);
    return plot.bottom - static_cast<int>(std::lround(clamped * plot.Height()));
}

/** 格式化刻度数值：整数不带小数，否则保留一位。 */
[[nodiscard]] inline std::string FormatValue(double value)
{
    if (std::abs(value - std::round(value)) < 1e-9)
        return std::to_string(static_cast<long long>(std::llround(value)));
    return std::to_string(value).substr(0, std::to_string(value).find('.') + 2);
}

/** 图例条目：颜色 + 名称。 */
struct LegendEntry final
{
    core::Color color;
    std::string name;
};

/** 在给定区域内横向绘制图例；超出区域宽度的条目会被裁剪。 */
inline void PaintLegend(render::Canvas& canvas, const core::DuiTheme& theme,
                        const std::vector<LegendEntry>& entries, core::Rect bounds)
{
    if (entries.empty() || bounds.Width() <= 0)
        return;
    render::DuiTextStyle style{theme.Get(core::ThemeSlot::TextSubtle), {}, 8, false};
    int cursor = bounds.left;
    for (const auto& entry : entries)
    {
        if (cursor + LegendSwatch > bounds.right)
            break;
        const int top = bounds.top + (LegendHeight - LegendSwatch) / 2;
        canvas.FillRoundedRect({cursor, top, cursor + LegendSwatch, top + LegendSwatch}, 2, entry.color);
        cursor += LegendSwatch + 4;
        const int textWidth = canvas.MeasureText(entry.name, style, {}).size.width;
        canvas.DrawText(entry.name, {cursor, bounds.top, cursor + textWidth, bounds.top + LegendHeight},
                        style, render::DuiTextAlignment::Start, false);
        cursor += textWidth + 12;
    }
}

/**
 * 绘制横向网格线与左侧数值刻度。
 * @param divisions 网格分段数（至少 1）
 */
inline void PaintValueAxis(render::Canvas& canvas, const core::DuiTheme& theme, core::Rect plot,
                           double minimum, double maximum, int divisions)
{
    if (divisions < 1 || plot.Height() <= 0)
        return;
    const core::Color grid = theme.Get(core::ThemeSlot::GridLine);
    const render::DuiTextStyle style{theme.Get(core::ThemeSlot::TextSubtle), {}, 8, false};
    for (int index = 0; index <= divisions; ++index)
    {
        const double ratio = static_cast<double>(index) / divisions;
        const double value = minimum + (maximum - minimum) * ratio;
        const int y = ValueToY(value, minimum, maximum, plot);
        canvas.FillRect({plot.left, y, plot.right, y + 1}, grid);
        const std::string label = FormatValue(value);
        canvas.DrawText(label, {plot.left - AxisGutter, y - AxisLabelHeight / 2, plot.left - 4, y + AxisLabelHeight / 2},
                        style, render::DuiTextAlignment::End, false);
    }
}

/** 绘制底部类别标签；标签在绘图区内按等分中位对齐。 */
inline void PaintCategoryLabels(render::Canvas& canvas, const core::DuiTheme& theme, core::Rect plot,
                                const std::vector<std::string>& labels)
{
    if (labels.empty() || plot.Width() <= 0)
        return;
    const render::DuiTextStyle style{theme.Get(core::ThemeSlot::TextSubtle), {}, 8, false};
    const int slot = plot.Width() / static_cast<int>(labels.size());
    for (std::size_t index = 0; index < labels.size(); ++index)
    {
        const int left = plot.left + static_cast<int>(index) * slot;
        canvas.DrawText(labels[index], {left, plot.bottom + 2, left + slot, plot.bottom + AxisLabelHeight},
                        style, render::DuiTextAlignment::Center, false);
    }
}

/**
 * 生成一段圆弧的折线近似（用于填充扇形）。
 * 说明：`DuiPath` 只支持 MoveTo/LineTo/Close，没有弧线指令，故按角度分段逼近。
 * @param center 圆心
 * @param radius 半径
 * @param startDegrees 起始角（0 度为 3 点钟方向，顺时针为正）
 * @param sweepDegrees 扫过角度
 * @param segments 分段数（越大越平滑）
 * @return 从圆心出发、沿弧线回到圆心的闭合路径
 */
[[nodiscard]] inline render::DuiPath ArcPath(core::Point center, int radius, double startDegrees,
                                             double sweepDegrees, int segments)
{
    constexpr double pi = 3.14159265358979323846;
    segments = (std::max)(2, segments);
    render::DuiPath path;
    path.MoveTo(center);
    for (int index = 0; index <= segments; ++index)
    {
        const double angle = (startDegrees + sweepDegrees * index / segments) * pi / 180.0;
        path.LineTo({center.x + static_cast<int>(std::lround(radius * std::cos(angle))),
                     center.y + static_cast<int>(std::lround(radius * std::sin(angle)))});
    }
    path.Close();
    return path;
}

} // namespace ysDui::controls::chart::detail
