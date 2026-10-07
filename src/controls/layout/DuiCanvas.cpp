#include "ysDui/controls/layout/DuiCanvas.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/render/DuiPaintChildren.hpp"

namespace ysDui::controls::layout {
namespace {
int nonNegative(int value)
{
    return (std::max)(0, value);
}

/** @return 该子控件在给定 item 下的实际尺寸：显式尺寸优先，否则取首选尺寸。 */
core::Size ResolveSize(const core::Control& child, DuiCanvasItem item)
{
    const core::Size desired = child.DesiredSize();
    return {item.width > 0 ? item.width : nonNegative(desired.width),
            item.height > 0 ? item.height : nonNegative(desired.height)};
}
} // namespace

class DuiCanvas::Impl {
public:
    std::vector<std::pair<core::Control*, DuiCanvasItem>> items;
};

DuiCanvas::DuiCanvas() : canvas_(std::make_unique<Impl>()) {}
DuiCanvas::~DuiCanvas() = default;
DuiCanvas::DuiCanvas(DuiCanvas&&) noexcept = default;
DuiCanvas& DuiCanvas::operator=(DuiCanvas&&) noexcept = default;

void DuiCanvas::AddChild(std::unique_ptr<core::Control> child, DuiCanvasItem item)
{
    core::Control* raw = child.get();
    if (raw == nullptr)
        return;
    core::Control::AddChild(std::move(child));
    canvas_->items.push_back({raw, item});
}

void DuiCanvas::SetItem(core::Control* child, DuiCanvasItem item)
{
    for (auto& current : canvas_->items)
    {
        if (current.first == child)
        {
            current.second = item;
            return;
        }
    }
    if (child != nullptr)
        canvas_->items.push_back({child, item});
}

DuiCanvasItem DuiCanvas::GetItem(core::Control* child) const
{
    for (const auto& current : canvas_->items)
    {
        if (current.first == child)
            return current.second;
    }
    return {};
}

void DuiCanvas::SetPosition(core::Control* child, int left, int top)
{
    for (auto& current : canvas_->items)
    {
        if (current.first == child)
        {
            current.second.left = left;
            current.second.top = top;
            return;
        }
    }
    if (child != nullptr)
        canvas_->items.push_back({child, {left, top, 0, 0}});
}

void DuiCanvas::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    for (const auto& item : canvas_->items)
    {
        if (item.first == nullptr)
            continue;
        const core::Size size = ResolveSize(*item.first, item.second);
        const int left = bounds.left + item.second.left;
        const int top = bounds.top + item.second.top;
        item.first->Layout({left, top, left + size.width, top + size.height});
    }
}

core::Size DuiCanvas::DesiredSize() const
{
    int width{};
    int height{};
    for (const auto& item : canvas_->items)
    {
        if (item.first == nullptr || !item.first->Visible())
            continue;
        const core::Size size = ResolveSize(*item.first, item.second);
        width = (std::max)(width, item.second.left + size.width);
        height = (std::max)(height, item.second.top + size.height);
    }
    return {nonNegative(width), nonNegative(height)};
}

void DuiCanvas::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (EffectivelyVisible())
        render::PaintChildren(*this, canvas, dirty);
}

} // namespace ysDui::controls::layout
