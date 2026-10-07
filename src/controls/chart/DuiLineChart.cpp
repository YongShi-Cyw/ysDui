#include "ysDui/controls/chart/DuiLineChart.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "DuiChartShared.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiPath.hpp"

namespace ysDui::controls::chart {
using namespace detail;

class DuiLineChart::Impl {
public:
    std::vector<DuiLineSeries> series;
    std::vector<std::string> labels;
    double explicitMinimum{};
    double explicitMaximum{};
    bool hasExplicitRange{};
    int gridDivisions{4};
    int lineWidth{2};
    bool showPoints{true};
    bool showLegend{true};
    render::DuiTextStyle textStyle{{34, 34, 40, 255}, {}, 8, false};
};

DuiLineChart::DuiLineChart() : chart_(std::make_unique<Impl>()) {}
DuiLineChart::~DuiLineChart() = default;
DuiLineChart::DuiLineChart(DuiLineChart&&) noexcept = default;
DuiLineChart& DuiLineChart::operator=(DuiLineChart&&) noexcept = default;

int DuiLineChart::AddSeries(std::string name, std::vector<double> values, core::Color color)
{
    DuiLineSeries entry;
    entry.name = std::move(name);
    entry.values = std::move(values);
    entry.color = color;
    entry.hasColor = color.alpha != 0;
    chart_->series.push_back(std::move(entry));
    return static_cast<int>(chart_->series.size()) - 1;
}
void DuiLineChart::ClearSeries() { chart_->series.clear(); }
int DuiLineChart::SeriesCount() const { return static_cast<int>(chart_->series.size()); }
DuiLineSeries DuiLineChart::SeriesAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(chart_->series.size()))
        return {};
    return chart_->series[static_cast<std::size_t>(index)];
}
void DuiLineChart::SetCategoryLabels(std::vector<std::string> labels) { chart_->labels = std::move(labels); }
void DuiLineChart::SetValueRange(double minimum, double maximum)
{
    if (minimum > maximum)
        std::swap(minimum, maximum);
    chart_->explicitMinimum = minimum;
    chart_->explicitMaximum = maximum;
    chart_->hasExplicitRange = true;
}
void DuiLineChart::ClearValueRange() { chart_->hasExplicitRange = false; }
void DuiLineChart::SetGridDivisions(int divisions) { chart_->gridDivisions = (std::max)(1, divisions); }
void DuiLineChart::SetShowPoints(bool show) { chart_->showPoints = show; }
void DuiLineChart::SetShowLegend(bool show) { chart_->showLegend = show; }
void DuiLineChart::SetLineWidth(int pixels) { chart_->lineWidth = (std::max)(1, pixels); }
void DuiLineChart::SetTextStyle(render::DuiTextStyle style) { chart_->textStyle = std::move(style); }

std::pair<double, double> DuiLineChart::ValueRange() const
{
    if (chart_->hasExplicitRange)
        return {chart_->explicitMinimum, chart_->explicitMaximum};
    double minimum{};
    double maximum{};
    bool first = true;
    for (const auto& entry : chart_->series)
    {
        for (const double value : entry.values)
        {
            if (!std::isfinite(value))
                continue;
            minimum = first ? value : (std::min)(minimum, value);
            maximum = first ? value : (std::max)(maximum, value);
            first = false;
        }
    }
    double niceMinimum{};
    double niceMaximum{};
    NiceRange(first ? 0.0 : minimum, first ? 1.0 : maximum, niceMinimum, niceMaximum);
    return {niceMinimum, niceMaximum};
}

core::Size DuiLineChart::DesiredSize() const { return {320, 180}; }

void DuiLineChart::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    canvas.FillRect(bounds, theme.Get(core::ThemeSlot::SurfaceBackground));
    // 图表底色与页面底色同为浅色：外框在内容之前绘制，避免被系列/扇区覆盖
    canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::GridBorder), 1.0F);

    const bool legend = chart_->showLegend && !chart_->series.empty();
    const core::Rect plot = PlotArea(Bounds(), true, !chart_->labels.empty(), legend ? 1 : 0);
    if (plot.Empty())
        return;

    const auto [minimum, maximum] = ValueRange();
    PaintValueAxis(canvas, theme, plot, minimum, maximum, chart_->gridDivisions);
    if (!chart_->labels.empty())
        PaintCategoryLabels(canvas, theme, plot, chart_->labels);

    const int pointCount = [this]
    {
        std::size_t count{};
        for (const auto& entry : chart_->series)
            count = (std::max)(count, entry.values.size());
        return static_cast<int>(count);
    }();
    if (pointCount <= 0)
        return;
    // 单点时居中绘制；多点时端点贴边
    const auto xAt = [pointCount, plot](int index)
    {
        if (pointCount <= 1)
            return (plot.left + plot.right) / 2;
        return plot.left + plot.Width() * index / (pointCount - 1);
    };

    for (int seriesIndex = 0; seriesIndex < static_cast<int>(chart_->series.size()); ++seriesIndex)
    {
        const DuiLineSeries& entry = chart_->series[static_cast<std::size_t>(seriesIndex)];
        if (entry.values.empty())
            continue;
        const core::Color color = entry.hasColor ? entry.color : SeriesColor(theme, seriesIndex);
        render::DuiPath line;
        for (std::size_t index = 0; index < entry.values.size(); ++index)
        {
            const int x = xAt(static_cast<int>(index));
            const int y = ValueToY(entry.values[index], minimum, maximum, plot);
            if (index == 0)
                line.MoveTo({x, y});
            else
                line.LineTo({x, y});
        }
        canvas.StrokePath(line, color, static_cast<float>(chart_->lineWidth));
        if (chart_->showPoints)
        {
            for (std::size_t index = 0; index < entry.values.size(); ++index)
            {
                const int x = xAt(static_cast<int>(index));
                const int y = ValueToY(entry.values[index], minimum, maximum, plot);
                canvas.FillEllipse({x - 3, y - 3, x + 4, y + 4}, color);
            }
        }
    }

    if (legend)
    {
        std::vector<LegendEntry> entries;
        for (int index = 0; index < static_cast<int>(chart_->series.size()); ++index)
        {
            const DuiLineSeries& entry = chart_->series[static_cast<std::size_t>(index)];
            entries.push_back({entry.hasColor ? entry.color : SeriesColor(theme, index), entry.name});
        }
        PaintLegend(canvas, theme, entries,
                    {Bounds().left + 6, Bounds().top + 2, Bounds().right - 6, Bounds().top + 2 + LegendHeight});
    }
}

} // namespace ysDui::controls::chart
