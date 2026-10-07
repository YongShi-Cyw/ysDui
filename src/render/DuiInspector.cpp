/**
 * 文件名：DuiInspector.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现平台无关的控件树调试覆盖层。
 */
#include "ysDui/render/DuiInspector.hpp"

#include <utility>

namespace ysDui::render {
namespace {

constexpr core::Color OutlineColor{220, 50, 50, 255};
constexpr core::Color HoverOutlineColor{60, 200, 80, 255};
constexpr core::Color LabelBackgroundColor{20, 20, 20, 255};
constexpr core::Color LabelTextColor{255, 255, 255, 255};
constexpr int OutlineWidth = 1;
constexpr int HoverOutlineWidth = 2;
constexpr int LabelHorizontalPadding = 6;
constexpr int LabelVerticalPadding = 3;

void AppendUtf8(std::string& destination, const std::string& source)
{
    destination += source;
}

void CollectVisibleRects(const core::Control& control, std::vector<core::Rect>& bounds)
{
    if (!control.Visible())
        return;

    bounds.push_back(control.Bounds());
    for (const auto& child : control.Children())
        CollectVisibleRects(*child, bounds);
}

const core::Control* FindHoveredControl(const core::Control& control)
{
    if (!control.Visible())
        return nullptr;

    for (const auto& child : control.Children())
    {
        if (const core::Control* hovered = FindHoveredControl(*child))
            return hovered;
    }
    return control.Hovered() ? &control : nullptr;
}

} // namespace

class DuiInspector::Impl
{
public:
    bool enabled{};
};

DuiInspector::DuiInspector()
    : impl_(std::make_unique<Impl>())
{
}

DuiInspector::~DuiInspector() = default;
DuiInspector::DuiInspector(DuiInspector&&) noexcept = default;
DuiInspector& DuiInspector::operator=(DuiInspector&&) noexcept = default;

void DuiInspector::SetEnabled(bool enabled)
{
    impl_->enabled = enabled;
}

bool DuiInspector::Enabled() const
{
    return impl_->enabled;
}

void DuiInspector::CollectVisibleRects(const core::Control& root, std::vector<core::Rect>& bounds)
{
    ::ysDui::render::CollectVisibleRects(root, bounds);
}

std::string DuiInspector::FormatControlInfo(const core::Control& control)
{
    std::string result = "Control";
    if (!control.Name().empty())
    {
        result += " name=";
        AppendUtf8(result, control.Name());
    }
    const core::Rect bounds = control.Bounds();
    result += " rect=(";
    result += std::to_string(bounds.left);
    result += ',';
    result += std::to_string(bounds.top);
    result += ',';
    result += std::to_string(bounds.right);
    result += ',';
    result += std::to_string(bounds.bottom);
    result += ')';
    return result;
}

void DuiInspector::PaintOverlay(const core::Control& root, Canvas& canvas, core::Rect dirty) const
{
    if (!Enabled())
        return;

    std::vector<core::Rect> bounds;
    CollectVisibleRects(root, bounds);
    for (const core::Rect& bound : bounds)
    {
        if (!core::Rect::Intersect(bound, dirty).Empty())
            canvas.StrokeRoundedRect(bound, 0, OutlineColor, static_cast<float>(OutlineWidth));
    }

    const core::Control* hovered = FindHoveredControl(root);
    if (hovered == nullptr || core::Rect::Intersect(hovered->Bounds(), dirty).Empty())
        return;

    const core::Rect hoverBounds = hovered->Bounds();
    canvas.StrokeRoundedRect(hoverBounds, 0, HoverOutlineColor, static_cast<float>(HoverOutlineWidth));
    const std::string label = FormatControlInfo(*hovered);
    const DuiTextStyle labelStyle{LabelTextColor, "Segoe UI, Arial, SimSun", 9, false};
    const DuiTextMetrics labelMetrics = canvas.MeasureText(label, labelStyle, {});
    const int labelWidth = labelMetrics.size.width + LabelHorizontalPadding * 2;
    const int labelHeight = labelMetrics.size.height + LabelVerticalPadding * 2;
    const int top = hoverBounds.top - labelHeight >= dirty.top
        ? hoverBounds.top - labelHeight
        : hoverBounds.top;
    const core::Rect labelBounds{hoverBounds.left, top, hoverBounds.left + labelWidth, top + labelHeight};
    canvas.FillRect(labelBounds, LabelBackgroundColor);
    canvas.DrawText(label, {labelBounds.left + LabelHorizontalPadding, labelBounds.top + LabelVerticalPadding,
                            labelBounds.right - LabelHorizontalPadding, labelBounds.bottom - LabelVerticalPadding},
                    labelStyle, DuiTextAlignment::Start, false);
}

} // namespace ysDui::render
