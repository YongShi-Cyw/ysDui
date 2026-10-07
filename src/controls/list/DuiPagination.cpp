/**
 * 文件名：DuiPagination.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-31
 * 用途：实现页码分页控件的布局、绘制和键盘交互。
 */
#include "ysDui/controls/list/DuiPagination.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::list {
namespace {
constexpr int ButtonWidth = 30;
constexpr int ButtonHeight = 28;
constexpr int Gap = 4;
int ClampPage(int page, int count) { return count <= 0 ? 0 : (std::clamp)(page, 0, count - 1); }
}

class DuiPagination::Impl {
public:
    int pageCount{};
    int currentPage{};
    int maxVisiblePages{7};
    std::vector<int> visiblePages;
    std::vector<core::Rect> visibleBounds;
    std::vector<core::Rect> pageBounds;
    core::Rect previous;
    core::Rect next;
    std::function<void(int)> changed;
};

DuiPagination::DuiPagination() : pagination_(std::make_unique<Impl>()) { RebuildVisiblePages(); }
DuiPagination::~DuiPagination() = default;
DuiPagination::DuiPagination(DuiPagination&&) noexcept = default;
DuiPagination& DuiPagination::operator=(DuiPagination&&) noexcept = default;
void DuiPagination::SetPageCount(int count)
{
    pagination_->pageCount = (std::max)(0, count);
    pagination_->currentPage = ClampPage(pagination_->currentPage, pagination_->pageCount);
    Layout(Bounds());
}
int DuiPagination::PageCount() const { return pagination_->pageCount; }
void DuiPagination::SetCurrentPage(int page, bool notify)
{
    const int nextPage = ClampPage(page, pagination_->pageCount);
    if (nextPage == pagination_->currentPage || pagination_->pageCount == 0)
        return;
    pagination_->currentPage = nextPage;
    Layout(Bounds());
    if (notify && pagination_->changed)
        pagination_->changed(nextPage);
}
int DuiPagination::CurrentPage() const { return pagination_->currentPage; }
void DuiPagination::SetMaxVisiblePages(int count)
{
    pagination_->maxVisiblePages = (std::max)(3, count | 1);
    Layout(Bounds());
}
int DuiPagination::MaxVisiblePages() const { return pagination_->maxVisiblePages; }
void DuiPagination::SetPageChangedHandler(std::function<void(int)> handler) { pagination_->changed = std::move(handler); }
core::Rect DuiPagination::PageRect(int page) const
{
    return page >= 0 && page < static_cast<int>(pagination_->pageBounds.size())
        ? pagination_->pageBounds[page] : core::Rect{};
}
core::Rect DuiPagination::PreviousRect() const { return pagination_->previous; }
core::Rect DuiPagination::NextRect() const { return pagination_->next; }
core::Size DuiPagination::DesiredSize() const
{
    const int count = static_cast<int>(pagination_->visiblePages.size());
    return {(count + 2) * ButtonWidth + (count + 1) * Gap, ButtonHeight};
}
void DuiPagination::RebuildVisiblePages()
{
    pagination_->visiblePages.clear();
    const int count = pagination_->pageCount;
    if (count <= 0)
        return;
    if (count <= pagination_->maxVisiblePages)
    {
        for (int page = 0; page < count; ++page)
            pagination_->visiblePages.push_back(page);
        return;
    }
    pagination_->visiblePages.push_back(0);
    const int first = (std::max)(1, pagination_->currentPage - 1);
    const int last = (std::min)(count - 2, pagination_->currentPage + 1);
    if (first > 1)
        pagination_->visiblePages.push_back(-1);
    for (int page = first; page <= last; ++page)
        pagination_->visiblePages.push_back(page);
    if (last < count - 2)
        pagination_->visiblePages.push_back(-1);
    pagination_->visiblePages.push_back(count - 1);
}
void DuiPagination::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    RebuildVisiblePages();
    pagination_->pageBounds.assign(static_cast<std::size_t>(pagination_->pageCount), {});
    pagination_->visibleBounds.clear();
    int left = bounds.left;
    pagination_->previous = {left, bounds.top, left + ButtonWidth, bounds.top + ButtonHeight};
    left += ButtonWidth + Gap;
    for (const int page : pagination_->visiblePages)
    {
        const core::Rect item{left, bounds.top, left + ButtonWidth, bounds.top + ButtonHeight};
        pagination_->visibleBounds.push_back(item);
        if (page >= 0)
            pagination_->pageBounds[page] = item;
        left += ButtonWidth + Gap;
    }
    pagination_->next = {left, bounds.top, left + ButtonWidth, bounds.top + ButtonHeight};
}
bool DuiPagination::OnEvent(const core::Event& event)
{
    if (!Enabled() || pagination_->pageCount == 0)
        return false;
    if (event.type == core::EventType::PointerDown)
    {
        if (pagination_->previous.Contains(event.position))
        {
            SetCurrentPage(pagination_->currentPage - 1, true);
            return true;
        }
        if (pagination_->next.Contains(event.position))
        {
            SetCurrentPage(pagination_->currentPage + 1, true);
            return true;
        }
        for (int page = 0; page < pagination_->pageCount; ++page)
        {
            if (PageRect(page).Contains(event.position))
            {
                SetCurrentPage(page, true);
                return true;
            }
        }
    }
    if (event.type == core::EventType::KeyDown
        && (event.key == core::key::Left || event.key == core::key::Right))
    {
        SetCurrentPage(pagination_->currentPage + (event.key == core::key::Right ? 1 : -1), true);
        return true;
    }
    return false;
}
void DuiPagination::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty() || pagination_->pageCount == 0)
        return;
    const core::DuiTheme& theme = Theme();
    const auto paintButton = [&canvas, &theme](core::Rect button, std::string_view text, bool selected)
    {
        canvas.FillRoundedRect(button, 4, selected ? theme.Get(core::ThemeSlot::BrandPrimary)
                                                  : theme.Get(core::ThemeSlot::ControlBackground));
        canvas.StrokeRoundedRect(button, 4, theme.Get(core::ThemeSlot::ControlBorder), 1.0f);
        canvas.DrawText(text, button, {selected ? theme.Get(core::ThemeSlot::TextOnPrimary)
                                               : theme.Get(core::ThemeSlot::ControlText), {}, 9, false},
                        render::DuiTextAlignment::Center, false);
    };
    paintButton(pagination_->previous, "<", pagination_->currentPage == 0);
    for (std::size_t index = 0; index < pagination_->visiblePages.size(); ++index)
    {
        const int page = pagination_->visiblePages[index];
        if (page < 0)
            canvas.DrawText("...", pagination_->visibleBounds[index],
                            {theme.Get(core::ThemeSlot::ControlText), {}, 9, false},
                            render::DuiTextAlignment::Center, false);
        else
            paintButton(pagination_->visibleBounds[index], std::to_string(page + 1), page == pagination_->currentPage);
    }
    paintButton(pagination_->next, ">", pagination_->currentPage == pagination_->pageCount - 1);
}
} // namespace ysDui::controls::list
