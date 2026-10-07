#include "ysDui/controls/layout/DuiDock.hpp"

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
} // namespace

class DuiDock::Impl {
public:
    struct Item final {
        core::Control* control{};
        DuiDockSide side{DuiDockSide::Fill};
        int size{};
    };

    DuiDockPadding padding{};
    int gap{};
    std::vector<Item> items;
};

DuiDock::DuiDock() : dock_(std::make_unique<Impl>()) {}
DuiDock::~DuiDock() = default;
DuiDock::DuiDock(DuiDock&&) noexcept = default;
DuiDock& DuiDock::operator=(DuiDock&&) noexcept = default;

void DuiDock::SetPadding(DuiDockPadding padding) { dock_->padding = padding; }
DuiDockPadding DuiDock::GetPadding() const { return dock_->padding; }
void DuiDock::SetGap(int pixels) { dock_->gap = nonNegative(pixels); }
int DuiDock::GetGap() const { return dock_->gap; }

void DuiDock::AddDocked(std::unique_ptr<core::Control> child, DuiDockSide side, int size)
{
    core::Control* raw = child.get();
    if (raw == nullptr)
        return;
    core::Control::AddChild(std::move(child));
    dock_->items.push_back({raw, side, nonNegative(size)});
}

void DuiDock::SetDock(core::Control* child, DuiDockSide side, int size)
{
    for (auto& item : dock_->items)
    {
        if (item.control == child)
        {
            item.side = side;
            item.size = nonNegative(size);
            return;
        }
    }
    if (child != nullptr)
        dock_->items.push_back({child, side, nonNegative(size)});
}

DuiDockSide DuiDock::GetDock(core::Control* child) const
{
    for (const auto& item : dock_->items)
    {
        if (item.control == child)
            return item.side;
    }
    return DuiDockSide::Fill;
}

int DuiDock::GetDockSize(core::Control* child) const
{
    for (const auto& item : dock_->items)
    {
        if (item.control == child)
            return item.size;
    }
    return 0;
}

void DuiDock::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    core::Rect inner{bounds.left + dock_->padding.left,
                     bounds.top + dock_->padding.top,
                     bounds.right - dock_->padding.right,
                     bounds.bottom - dock_->padding.bottom};
    if (inner.Empty())
        return;

    for (const auto& item : dock_->items)
    {
        if (!item.control->Visible() || inner.Empty())
            continue;
        core::Rect child;
        switch (item.side)
        {
        case DuiDockSide::Top:
            child = {inner.left, inner.top, inner.right,
                     inner.top + std::min(nonNegative(item.size), inner.Height())};
            inner.top = child.bottom + dock_->gap;
            break;
        case DuiDockSide::Bottom:
            child = {inner.left, inner.bottom - std::min(nonNegative(item.size), inner.Height()),
                     inner.right, inner.bottom};
            inner.bottom = child.top - dock_->gap;
            break;
        case DuiDockSide::Left:
            child = {inner.left, inner.top,
                     inner.left + std::min(nonNegative(item.size), inner.Width()), inner.bottom};
            inner.left = child.right + dock_->gap;
            break;
        case DuiDockSide::Right:
            child = {inner.right - std::min(nonNegative(item.size), inner.Width()), inner.top,
                     inner.right, inner.bottom};
            inner.right = child.left - dock_->gap;
            break;
        case DuiDockSide::Fill:
            child = inner;
            inner.right = inner.left;
            inner.bottom = inner.top;
            break;
        }
        item.control->Layout(child);
    }
}

void DuiDock::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (EffectivelyVisible()) render::PaintChildren(*this, canvas, dirty);
}

} // namespace ysDui::controls::layout
