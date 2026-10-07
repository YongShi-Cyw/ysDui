#include "ysDui/controls/basic/DuiBreadcrumb.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {
class DuiBreadcrumb::Impl {
public:
    struct Segment { int index; std::string text; core::Rect bounds; bool separator; };
    std::vector<std::string> items;
    mutable std::vector<Segment> segments;
    std::function<void(int)> clicked;
    render::DuiTextStyle style{{80, 80, 90, 255}, {}, 9, false};
};
DuiBreadcrumb::DuiBreadcrumb() : breadcrumb_(std::make_unique<Impl>()) {}
DuiBreadcrumb::~DuiBreadcrumb() = default;
DuiBreadcrumb::DuiBreadcrumb(DuiBreadcrumb&&) noexcept = default;
DuiBreadcrumb& DuiBreadcrumb::operator=(DuiBreadcrumb&&) noexcept = default;
void DuiBreadcrumb::SetItems(std::vector<std::string> items) { breadcrumb_->items = std::move(items); }
void DuiBreadcrumb::AddItem(std::string text) { breadcrumb_->items.push_back(std::move(text)); }
void DuiBreadcrumb::ClearItems() { breadcrumb_->items.clear(); breadcrumb_->segments.clear(); }
const std::vector<std::string>& DuiBreadcrumb::Items() const { return breadcrumb_->items; }
void DuiBreadcrumb::SetItemClickedHandler(std::function<void(int)> handler) { breadcrumb_->clicked = std::move(handler); }
bool DuiBreadcrumb::OnEvent(const core::Event& event) {
    if (event.type != core::EventType::PointerDown) return false;
    for (const auto& segment : breadcrumb_->segments) if (!segment.separator && segment.index >= 0 && segment.bounds.Contains(event.position)) {
        if (breadcrumb_->clicked) breadcrumb_->clicked(segment.index);
        return true;
    }
    return false;
}
void DuiBreadcrumb::Paint(render::Canvas& canvas, core::Rect dirty) const {
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty()) return;
    render::DuiTextStyle baseStyle = breadcrumb_->style;
    baseStyle.color = Theme().Get(core::ThemeSlot::BreadcrumbText);
    breadcrumb_->segments.clear();
    const std::string separator = " > ";
    const std::string ellipsis = "\xE2\x80\xA6";
    const int separatorWidth = canvas.MeasureText(separator, baseStyle, {}).size.width;
    std::vector<int> widths;
    widths.reserve(breadcrumb_->items.size());
    for (const auto& item : breadcrumb_->items)
        widths.push_back(canvas.MeasureText(item, baseStyle, {}).size.width);
    int firstVisible{};
    int totalWidth{};
    for (const int width : widths)
        totalWidth += width;
    if (!widths.empty())
        totalWidth += separatorWidth * (static_cast<int>(widths.size()) - 1);
    if (totalWidth > bounds.Width() && !widths.empty()) {
        const int ellipsisWidth = canvas.MeasureText(ellipsis, baseStyle, {}).size.width;
        int used = ellipsisWidth + separatorWidth + widths.back();
        firstVisible = static_cast<int>(widths.size()) - 1;
        while (firstVisible > 0 && used + separatorWidth + widths[firstVisible - 1] <= bounds.Width()) {
            --firstVisible;
            used += separatorWidth + widths[firstVisible];
        }
        const int ellipsisRight = (std::min)(bounds.right, bounds.left + ellipsisWidth);
        breadcrumb_->segments.push_back({-1, ellipsis,
            {bounds.left, bounds.top, ellipsisRight, bounds.bottom}, true});
        const int separatorRight = (std::min)(bounds.right, ellipsisRight + separatorWidth);
        breadcrumb_->segments.push_back({-1, separator, {ellipsisRight, bounds.top, separatorRight, bounds.bottom}, true});
    }
    int x = breadcrumb_->segments.empty() ? bounds.left : breadcrumb_->segments.back().bounds.right;
    for (int index = firstVisible; index < static_cast<int>(breadcrumb_->items.size()); ++index) {
        const int width = widths[index];
        if (x + width > bounds.right)
            break;
        breadcrumb_->segments.push_back({index, breadcrumb_->items[index], {x, bounds.top, x + width, bounds.bottom}, false});
        x += width;
        if (index + 1 < static_cast<int>(breadcrumb_->items.size()) && x + separatorWidth <= bounds.right) {
            breadcrumb_->segments.push_back({-1, separator, {x, bounds.top, x + separatorWidth, bounds.bottom}, true});
            x += separatorWidth;
        }
    }
    for (const auto& segment : breadcrumb_->segments) {
        render::DuiTextStyle style = baseStyle;
        if (!segment.separator && segment.index == static_cast<int>(breadcrumb_->items.size()) - 1)
            style.color = Theme().Get(core::ThemeSlot::TextLink);
        else if (segment.separator)
            style.color = Theme().Get(core::ThemeSlot::BreadcrumbSeparator);
        canvas.DrawText(segment.text, segment.bounds, style, render::DuiTextAlignment::Start, false);
    }
}
} // namespace ysDui::controls::basic
