#include "ysDui/controls/layout/DuiStack.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/render/DuiPaintChildren.hpp"

namespace ysDui::controls::layout {
namespace {
int clampNonNegative(int value)
{
    return std::max(0, value);
}

int mainStart(core::Rect rect, DuiStackOrientation orientation)
{
    return orientation == DuiStackOrientation::Horizontal ? rect.left : rect.top;
}

int mainEnd(core::Rect rect, DuiStackOrientation orientation)
{
    return orientation == DuiStackOrientation::Horizontal ? rect.right : rect.bottom;
}

int crossStart(core::Rect rect, DuiStackOrientation orientation)
{
    return orientation == DuiStackOrientation::Horizontal ? rect.top : rect.left;
}

int crossEnd(core::Rect rect, DuiStackOrientation orientation)
{
    return orientation == DuiStackOrientation::Horizontal ? rect.bottom : rect.right;
}

core::Rect makeRect(DuiStackOrientation orientation, int main0, int cross0, int main1, int cross1)
{
    if (orientation == DuiStackOrientation::Horizontal)
        return {main0, cross0, main1, cross1};
    return {cross0, main0, cross1, main1};
}
} // namespace

class DuiStack::Impl {
public:
    DuiStackMode mode{DuiStackMode::WeightedLayout};
    DuiStackOrientation orientation{DuiStackOrientation::Horizontal};
    DuiThickness padding{};
    int gap{};
    std::vector<std::pair<core::Control*, DuiStackItem>> items;
    std::vector<core::Control*> pages;
    int currentIndex{-1};
};

DuiStack::DuiStack() : stack_(std::make_unique<Impl>()) {}
DuiStack::~DuiStack() = default;
DuiStack::DuiStack(DuiStack&&) noexcept = default;
DuiStack& DuiStack::operator=(DuiStack&&) noexcept = default;

void DuiStack::SetOrientation(DuiStackOrientation orientation) { stack_->orientation = orientation; }
DuiStackOrientation DuiStack::GetOrientation() const { return stack_->orientation; }
void DuiStack::SetPadding(DuiThickness padding) { stack_->padding = padding; }
DuiThickness DuiStack::GetPadding() const { return stack_->padding; }
void DuiStack::SetGap(int pixels) { stack_->gap = clampNonNegative(pixels); }
int DuiStack::GetGap() const { return stack_->gap; }

void DuiStack::SetMode(DuiStackMode mode)
{
    stack_->mode = mode;
    for (int index = 0; index < static_cast<int>(stack_->pages.size()); ++index)
        stack_->pages[index]->SetVisible(mode != DuiStackMode::Pages || index == stack_->currentIndex);
}

DuiStackMode DuiStack::GetMode() const { return stack_->mode; }

void DuiStack::AddChild(std::unique_ptr<core::Control> child, DuiStackItem item)
{
    core::Control* raw = child.get();
    if (raw == nullptr)
        return;
    core::Control::AddChild(std::move(child));
    stack_->items.push_back({raw, item});
}

void DuiStack::SetItem(core::Control* child, DuiStackItem item)
{
    for (auto& current : stack_->items)
    {
        if (current.first == child)
        {
            current.second = item;
            return;
        }
    }
    if (child != nullptr)
        stack_->items.push_back({child, item});
}

DuiStackItem DuiStack::GetItem(core::Control* child) const
{
    for (const auto& current : stack_->items)
    {
        if (current.first == child)
            return current.second;
    }
    return {};
}

void DuiStack::AddPage(std::unique_ptr<core::Control> page)
{
    if (page == nullptr)
        return;
    core::Control* raw = page.get();
    core::Control::AddChild(std::move(page));
    stack_->pages.push_back(raw);
    if (stack_->currentIndex < 0)
        stack_->currentIndex = 0;
    if (stack_->mode == DuiStackMode::Pages)
        raw->SetVisible(stack_->currentIndex == PageCount() - 1);
}

std::unique_ptr<core::Control> DuiStack::RemovePage(int index)
{
    if (index < 0 || index >= PageCount())
        return {};
    core::Control* page = stack_->pages[index];
    stack_->pages.erase(stack_->pages.begin() + index);
    if (stack_->pages.empty())
        stack_->currentIndex = -1;
    else if (index < stack_->currentIndex)
        --stack_->currentIndex;
    else if (index == stack_->currentIndex)
        stack_->currentIndex = std::min(index, PageCount() - 1);
    if (stack_->mode == DuiStackMode::Pages)
    {
        for (int pageIndex = 0; pageIndex < PageCount(); ++pageIndex)
            stack_->pages[pageIndex]->SetVisible(pageIndex == stack_->currentIndex);
    }
    return core::Control::RemoveChild(page);
}

int DuiStack::PageCount() const { return static_cast<int>(stack_->pages.size()); }

void DuiStack::SetCurrentIndex(int index)
{
    if (index < 0 || index >= PageCount())
        return;
    stack_->currentIndex = index;
    if (stack_->mode != DuiStackMode::Pages)
        return;
    for (int pageIndex = 0; pageIndex < PageCount(); ++pageIndex)
        stack_->pages[pageIndex]->SetVisible(pageIndex == index);
}

int DuiStack::CurrentIndex() const { return stack_->currentIndex; }

void DuiStack::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    if (stack_->mode == DuiStackMode::Pages)
    {
        for (int index = 0; index < PageCount(); ++index)
        {
            core::Control* page = stack_->pages[index];
            page->SetVisible(index == stack_->currentIndex);
            if (index == stack_->currentIndex)
                page->Layout(bounds);
        }
        return;
    }
    const core::Rect inner{bounds.left + stack_->padding.left,
                           bounds.top + stack_->padding.top,
                           bounds.right - stack_->padding.right,
                           bounds.bottom - stack_->padding.bottom};
    if (inner.Empty())
        return;

    std::vector<core::Control*> visible;
    int occupiedMain = 0;
    int weight = 0;
    int flexibleCount = 0;
    for (const auto& child : Children())
    {
        if (!child->Visible())
            continue;
        visible.push_back(child.get());
        const DuiStackItem item = GetItem(child.get());
        const int marginMain = stack_->orientation == DuiStackOrientation::Horizontal
            ? item.margin.left + item.margin.right
            : item.margin.top + item.margin.bottom;
        occupiedMain += marginMain;
        if (item.mainLength >= 0)
            occupiedMain += item.mainLength;
        else
        {
            ++flexibleCount;
            weight += std::max(1, item.weight);
        }
    }
    if (visible.empty())
        return;

    const int totalGap = stack_->gap * static_cast<int>(visible.size() - 1);
    const int availableMain = std::max(0, mainEnd(inner, stack_->orientation) - mainStart(inner, stack_->orientation));
    const int flexibleMain = std::max(0, availableMain - occupiedMain - totalGap);
    int cursor = mainStart(inner, stack_->orientation);
    int assignedFlexible = 0;
    int flexibleSeen = 0;

    for (core::Control* child : visible)
    {
        const DuiStackItem item = GetItem(child);
        const bool flex = item.mainLength < 0;
        int childMain = item.mainLength;
        if (flex)
        {
            ++flexibleSeen;
            const int itemWeight = std::max(1, item.weight);
            childMain = weight == 0 ? 0 : flexibleMain * itemWeight / weight;
            if (flexibleSeen == flexibleCount)
                childMain = flexibleMain - assignedFlexible;
            assignedFlexible += childMain;
        }
        const int beforeMainMargin = stack_->orientation == DuiStackOrientation::Horizontal ? item.margin.left : item.margin.top;
        const int afterMainMargin = stack_->orientation == DuiStackOrientation::Horizontal ? item.margin.right : item.margin.bottom;
        const int beforeCrossMargin = stack_->orientation == DuiStackOrientation::Horizontal ? item.margin.top : item.margin.left;
        const int afterCrossMargin = stack_->orientation == DuiStackOrientation::Horizontal ? item.margin.bottom : item.margin.right;

        const int childMainStart = cursor + beforeMainMargin;
        const int childMainEnd = childMainStart + std::max(0, childMain);
        const int childCrossStart = crossStart(inner, stack_->orientation) + beforeCrossMargin;
        const int childCrossEnd = crossEnd(inner, stack_->orientation) - afterCrossMargin;
        child->Layout(makeRect(stack_->orientation, childMainStart, childCrossStart, childMainEnd, childCrossEnd));

        cursor = childMainEnd + afterMainMargin + stack_->gap;
    }
}

void DuiStack::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (EffectivelyVisible()) render::PaintChildren(*this, canvas, dirty);
}

} // namespace ysDui::controls::layout
