#include "ysDui/controls/chart/DuiPieChart.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "DuiChartShared.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiPath.hpp"

namespace ysDui::controls::chart {
namespace {
constexpr int LegendWidth = 120;      // 右侧图例占位
constexpr int LegendRowHeight = 18;   // 图例行高
constexpr int LegendSwatch = 8;       // 图例色块边长
constexpr int MinSegmentsPerSlice = 6; // 每个扇区的最少分段数
/** 每 6 度一个分段，兼顾平滑度与顶点数。 */
constexpr double DegreesPerSegment = 6.0;
} // namespace

class DuiPieChart::Impl {
public:
    std::vector<DuiPieSlice> slices;
    double innerRatio{};
    double startAngle{-90.0}; // 默认从 12 点钟方向开始
    bool showLegend{true};
    render::DuiTextStyle textStyle{{34, 34, 40, 255}, {}, 8, false};
};

DuiPieChart::DuiPieChart() : chart_(std::make_unique<Impl>()) {}
DuiPieChart::~DuiPieChart() = default;
DuiPieChart::DuiPieChart(DuiPieChart&&) noexcept = default;
DuiPieChart& DuiPieChart::operator=(DuiPieChart&&) noexcept = default;

void DuiPieChart::AddSlice(std::string label, double value, core::Color color)
{
    DuiPieSlice entry;
    entry.label = std::move(label);
    entry.value = value;
    entry.color = color;
    entry.hasColor = color.alpha != 0;
    chart_->slices.push_back(std::move(entry));
}
void DuiPieChart::ClearSlices() { chart_->slices.clear(); }
int DuiPieChart::SliceCount() const { return static_cast<int>(chart_->slices.size()); }
DuiPieSlice DuiPieChart::SliceAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(chart_->slices.size()))
        return {};
    return chart_->slices[static_cast<std::size_t>(index)];
}
void DuiPieChart::SetInnerRadiusRatio(double ratio)
{
    chart_->innerRatio = (std::clamp)(ratio, 0.0, 0.95);
}
double DuiPieChart::InnerRadiusRatio() const { return chart_->innerRatio; }
void DuiPieChart::SetShowLegend(bool show) { chart_->showLegend = show; }
void DuiPieChart::SetStartAngle(double degrees) { chart_->startAngle = degrees; }
void DuiPieChart::SetTextStyle(render::DuiTextStyle style) { chart_->textStyle = std::move(style); }

core::Point DuiPieChart::Center() const
{
    const core::Rect bounds = Bounds();
    const int legend = chart_->showLegend ? LegendWidth : 0;
    const int width = (std::max)(1, bounds.Width() - legend);
    return {bounds.left + width / 2, bounds.top + bounds.Height() / 2};
}

int DuiPieChart::Radius() const
{
    const core::Rect bounds = Bounds();
    const int legend = chart_->showLegend ? LegendWidth : 0;
    const int width = (std::max)(1, bounds.Width() - legend);
    // 留出 8px 边距，避免扇区贴边
    return (std::max)(1, (std::min)(width, bounds.Height()) / 2 - 8);
}

core::Size DuiPieChart::DesiredSize() const { return {320, 180}; }

void DuiPieChart::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    canvas.FillRect(bounds, theme.Get(core::ThemeSlot::SurfaceBackground));
    // 图表底色与页面底色同为浅色：外框在内容之前绘制，避免被扇区覆盖
    canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::GridBorder), 1.0F);

    // 统计有效总量（忽略负值与非有限值）
    double total{};
    for (const auto& slice : chart_->slices)
    {
        if (std::isfinite(slice.value) && slice.value > 0.0)
            total += slice.value;
    }
    if (total <= 0.0)
        return;

    const core::Point center = Center();
    const int radius = Radius();
    const int innerRadius = static_cast<int>(std::lround(radius * chart_->innerRatio));
    const render::DuiTextStyle percentStyle{theme.Get(core::ThemeSlot::TextOnPrimary), {}, 8, true};
    const render::DuiTextStyle legendStyle{theme.Get(core::ThemeSlot::TextSubtle), {}, 8, false};

    double angle = chart_->startAngle;
    int legendRow{};
    for (int index = 0; index < static_cast<int>(chart_->slices.size()); ++index)
    {
        const DuiPieSlice& slice = chart_->slices[static_cast<std::size_t>(index)];
        if (!std::isfinite(slice.value) || slice.value <= 0.0)
            continue;
        const double share = slice.value / total;
        const double sweep = share * 360.0;
        const core::Color color = slice.hasColor ? slice.color : detail::SeriesColor(theme, index);

        const int segments = (std::max)(MinSegmentsPerSlice,
                                        static_cast<int>(std::lround(sweep / DegreesPerSegment)));
        // 实心：直接填充扇形；环形：先填充外圈扇形，再用背景色覆盖内圈
        canvas.FillPath(detail::ArcPath(center, radius, angle, sweep, segments), color);
        if (innerRadius > 0)
        {
            canvas.FillPath(detail::ArcPath(center, innerRadius, angle, sweep, segments),
                            theme.Get(core::ThemeSlot::SurfaceBackground));
        }

        // 扇区占比标注：绘制在扇区中角半径中位处
        if (share >= 0.06)
        {
            constexpr double pi = 3.14159265358979323846;
            const double middle = (angle + sweep / 2.0) * pi / 180.0;
            const int labelRadius = innerRadius > 0 ? (innerRadius + radius) / 2 : radius * 2 / 3;
            const int x = center.x + static_cast<int>(std::lround(labelRadius * std::cos(middle)));
            const int y = center.y + static_cast<int>(std::lround(labelRadius * std::sin(middle)));
            const std::string text = std::to_string(static_cast<int>(std::lround(share * 100.0))) + "%";
            canvas.DrawText(text, {x - 18, y - 7, x + 18, y + 7}, percentStyle,
                            render::DuiTextAlignment::Center, false);
        }

        if (chart_->showLegend && !slice.label.empty())
        {
            const int top = bounds.top + 8 + legendRow * LegendRowHeight;
            const int left = bounds.right - LegendWidth + 8;
            canvas.FillRoundedRect({left, top + (LegendRowHeight - LegendSwatch) / 2,
                                    left + LegendSwatch, top + (LegendRowHeight + LegendSwatch) / 2},
                                   2, color);
            const std::string label = slice.label + "  "
                + std::to_string(static_cast<int>(std::lround(share * 100.0))) + "%";
            canvas.DrawText(label, {left + LegendSwatch + 6, top, bounds.right - 4, top + LegendRowHeight},
                            legendStyle, render::DuiTextAlignment::Start, false);
            ++legendRow;
        }
        angle += sweep;
    }
}

} // namespace ysDui::controls::chart
