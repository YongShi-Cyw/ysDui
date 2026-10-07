#include "ysDui/controls/layout/DuiFlow.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/render/DuiPaintChildren.hpp"

namespace ysDui::controls::layout {
namespace {
int nonNegative(int value)
{
    return std::max(0, value);
}

int outerWidth(DuiFlowItem item)
{
    return nonNegative(item.desiredSize.width) + item.margin.left + item.margin.right;
}

int outerHeight(DuiFlowItem item)
{
    return nonNegative(item.desiredSize.height) + item.margin.top + item.margin.bottom;
}
} // namespace

class DuiFlow::Impl {
public:
    DuiFlowThickness padding{};
    int gap{};
    std::vector<std::pair<core::Control*, DuiFlowItem>> items;
};

DuiFlow::DuiFlow() : flow_(std::make_unique<Impl>()) {}
DuiFlow::~DuiFlow() = default;
DuiFlow::DuiFlow(DuiFlow&&) noexcept = default;
DuiFlow& DuiFlow::operator=(DuiFlow&&) noexcept = default;

void DuiFlow::SetPadding(DuiFlowThickness padding) { flow_->padding = padding; }
DuiFlowThickness DuiFlow::GetPadding() const { return flow_->padding; }
void DuiFlow::SetGap(int pixels) { flow_->gap = nonNegative(pixels); }
int DuiFlow::GetGap() const { return flow_->gap; }

void DuiFlow::AddChild(std::unique_ptr<core::Control> child, DuiFlowItem item)
{
    core::Control* raw = child.get();
    if (raw == nullptr)
        return;
    core::Control::AddChild(std::move(child));
    flow_->items.push_back({raw, item});
}

void DuiFlow::SetItem(core::Control* child, DuiFlowItem item)
{
    for (auto& current : flow_->items)
    {
        if (current.first == child)
        {
            current.second = item;
            return;
        }
    }
    if (child != nullptr)
        flow_->items.push_back({child, item});
}

DuiFlowItem DuiFlow::GetItem(core::Control* child) const
{
    for (const auto& current : flow_->items)
    {
        if (current.first == child)
            return current.second;
    }
    return {};
}

void DuiFlow::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    const core::Rect inner{bounds.left + flow_->padding.left,
                           bounds.top + flow_->padding.top,
                           bounds.right - flow_->padding.right,
                           bounds.bottom - flow_->padding.bottom};
    const int availableWidth = std::max(0, inner.Width());
    int cursorX{};
    int cursorY{};
    int lineHeight{};

    for (const auto& child : Children())
    {
        if (!child->Visible())
            continue;
        const DuiFlowItem item = GetItem(child.get());
        const int width = outerWidth(item);
        const int height = outerHeight(item);
        if (cursorX > 0 && cursorX + width > availableWidth)
        {
            cursorY += lineHeight + flow_->gap;
            cursorX = 0;
            lineHeight = 0;
        }
        const int left = inner.left + cursorX + item.margin.left;
        const int top = inner.top + cursorY + item.margin.top;
        child->Layout({left, top,
                          left + nonNegative(item.desiredSize.width),
                          top + nonNegative(item.desiredSize.height)});
        cursorX += width + flow_->gap;
        lineHeight = std::max(lineHeight, height);
    }
}

core::Size DuiFlow::DesiredSize() const
{
    const core::Rect bounds = Bounds();
    const int availableWidth = std::max(0, bounds.Width() - flow_->padding.left - flow_->padding.right);
    if (availableWidth == 0)
    {
        int width{};
        int height{};
        bool hasChild{};
        for (const auto& child : Children())
        {
            if (!child->Visible())
                continue;
            const DuiFlowItem item = GetItem(child.get());
            if (hasChild)
                width += flow_->gap;
            width += outerWidth(item);
            height = std::max(height, outerHeight(item));
            hasChild = true;
        }
        return {flow_->padding.left + width + flow_->padding.right,
                flow_->padding.top + height + flow_->padding.bottom};
    }

    int cursorX{};
    int totalHeight{};
    int lineHeight{};
    for (const auto& child : Children())
    {
        if (!child->Visible())
            continue;
        const DuiFlowItem item = GetItem(child.get());
        const int width = outerWidth(item);
        const int height = outerHeight(item);
        if (cursorX > 0 && cursorX + width > availableWidth)
        {
            totalHeight += lineHeight + flow_->gap;
            cursorX = 0;
            lineHeight = 0;
        }
        cursorX += width + flow_->gap;
        lineHeight = std::max(lineHeight, height);
    }
    if (lineHeight > 0)
        totalHeight += lineHeight;
    return {flow_->padding.left + availableWidth + flow_->padding.right,
            flow_->padding.top + totalHeight + flow_->padding.bottom};
}

void DuiFlow::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (EffectivelyVisible()) render::PaintChildren(*this, canvas, dirty);
}

} // namespace ysDui::controls::layout
