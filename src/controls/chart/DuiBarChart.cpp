#include "ysDui/controls/chart/DuiBarChart.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "DuiChartShared.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::chart {
using namespace detail;

class DuiBarChart::Impl {
public:
    std::vector<DuiBar> bars;
    double explicitMinimum{};
    double explicitMaximum{};
    bool hasExplicitRange{};
    int barGap{6};
    int gridDivisions{4};
    bool showValues{true};
    render::DuiTextStyle textStyle{{34, 34, 40, 255}, {}, 8, false};
};

DuiBarChart::DuiBarChart() : chart_(std::make_unique<Impl>()) {}
DuiBarChart::~DuiBarChart() = default;
DuiBarChart::DuiBarChart(DuiBarChart&&) noexcept = default;
DuiBarChart& DuiBarChart::operator=(DuiBarChart&&) noexcept = default;

void DuiBarChart::AddBar(std::string label, double value, core::Color color)
{
    DuiBar entry;
    entry.label = std::move(label);
    entry.value = value;
    entry.color = color;
    entry.hasColor = color.alpha != 0;
    chart_->bars.push_back(std::move(entry));
}
void DuiBarChart::ClearBars() { chart_->bars.clear(); }
int DuiBarChart::BarCount() const { return static_cast<int>(chart_->bars.size()); }
DuiBar DuiBarChart::BarAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(chart_->bars.size()))
        return {};
    return chart_->bars[static_cast<std::size_t>(index)];
}
void DuiBarChart::SetValueRange(double minimum, double maximum)
{
    if (minimum > maximum)
        std::swap(minimum, maximum);
    chart_->explicitMinimum = minimum;
    chart_->explicitMaximum = maximum;
    chart_->hasExplicitRange = true;
}
void DuiBarChart::ClearValueRange() { chart_->hasExplicitRange = false; }
void DuiBarChart::SetBarGap(int pixels) { chart_->barGap = (std::max)(0, pixels); }
void DuiBarChart::SetGridDivisions(int divisions) { chart_->gridDivisions = (std::max)(1, divisions); }
void DuiBarChart::SetShowValues(bool show) { chart_->showValues = show; }
void DuiBarChart::SetTextStyle(render::DuiTextStyle style) { chart_->textStyle = std::move(style); }

std::pair<double, double> DuiBarChart::ValueRange() const
{
    if (chart_->hasExplicitRange)
        return {chart_->explicitMinimum, chart_->explicitMaximum};
    double minimum{};
    double maximum{};
    bool first = true;
    for (const auto& bar : chart_->bars)
    {
        if (!std::isfinite(bar.value))
            continue;
        minimum = first ? bar.value : (std::min)(minimum, bar.value);
        maximum = first ? bar.value : (std::max)(maximum, bar.value);
        first = false;
    }
    double niceMinimum{};
    double niceMaximum{};
    NiceRange(first ? 0.0 : minimum, first ? 1.0 : maximum, niceMinimum, niceMaximum);
    // 柱状图以 0 为基线更符合直觉，除非数据全为负
    if (niceMaximum > 0.0 && niceMinimum > 0.0)
        niceMinimum = 0.0;
    return {niceMinimum, niceMaximum};
}

core::Size DuiBarChart::DesiredSize() const { return {320, 180}; }

void DuiBarChart::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    canvas.FillRect(bounds, theme.Get(core::ThemeSlot::SurfaceBackground));
    // 图表底色与页面底色同为浅色：外框在内容之前绘制，避免被柱子覆盖
    canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::GridBorder), 1.0F);

    const core::Rect plot = PlotArea(Bounds(), true, true, 0);
    if (plot.Empty() || chart_->bars.empty())
        return;
    const auto [minimum, maximum] = ValueRange();
    PaintValueAxis(canvas, theme, plot, minimum, maximum, chart_->gridDivisions);

    const int count = static_cast<int>(chart_->bars.size());
    const int slot = plot.Width() / count;
    const int barWidth = (std::max)(1, slot - chart_->barGap);
    const int zeroY = ValueToY(0.0, minimum, maximum, plot);
    const render::DuiTextStyle valueStyle{theme.Get(core::ThemeSlot::TextSubtle), {}, 8, false};

    for (int index = 0; index < count; ++index)
    {
        const DuiBar& bar = chart_->bars[static_cast<std::size_t>(index)];
        const core::Color color = bar.hasColor ? bar.color : SeriesColor(theme, index);
        const int left = plot.left + index * slot + chart_->barGap / 2;
        const int valueY = ValueToY(bar.value, minimum, maximum, plot);
        // 以 0 为基线：正值向上、负值向下
        const core::Rect rect{left, (std::min)(valueY, zeroY), left + barWidth, (std::max)(valueY, zeroY)};
        if (rect.Width() > 0 && rect.Height() > 0)
            canvas.FillRoundedRect(rect, 2, color);

        if (chart_->showValues)
        {
            const std::string text = FormatValue(bar.value);
            const int top = valueY < zeroY ? valueY - 14 : valueY + 2;
            canvas.DrawText(text, {left, top, left + barWidth, top + 14}, valueStyle,
                            render::DuiTextAlignment::Center, false);
        }
        if (!bar.label.empty())
        {
            canvas.DrawText(bar.label, {plot.left + index * slot, plot.bottom + 2,
                                        plot.left + (index + 1) * slot, plot.bottom + AxisLabelHeight},
                            valueStyle, render::DuiTextAlignment::Center, false);
        }
    }
}

} // namespace ysDui::controls::chart
