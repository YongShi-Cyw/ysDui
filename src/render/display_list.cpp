#include "ysDui/render/DuiDisplayList.hpp"

#include <vector>

namespace ysDui::render {

class DisplayList::Impl {
public:
    struct Fill final { core::Rect bounds; core::Color color; };
    std::vector<Fill> fills;
};

DisplayList::DisplayList() : impl_(std::make_unique<Impl>()) {}
DisplayList::~DisplayList() = default;
DisplayList::DisplayList(DisplayList&&) noexcept = default;
DisplayList& DisplayList::operator=(DisplayList&&) noexcept = default;
void DisplayList::FillRect(core::Rect bounds, core::Color color) { if (!bounds.Empty()) { impl_->fills.push_back({bounds, color}); } }
void DisplayList::Clear() { impl_->fills.clear(); }
bool DisplayList::Empty() const { return impl_->fills.empty(); }
void DisplayList::Replay(Canvas& canvas) const { for (const auto& fill : impl_->fills) { canvas.FillRect(fill.bounds, fill.color); } }

} // namespace ysDui::render
