#include "ysDui/controls/basic/DuiBadge.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {

class DuiBadge::Impl {
public:
    std::string text;
    core::Color background{220, 60, 60, 255};
    render::DuiTextStyle textStyle{{255, 255, 255, 255}, {}, 9, false};
    bool backgroundOverride{};
    bool textStyleOverride{};
    bool hideWhenEmpty{true};
    std::optional<core::Color> leadingDot;
    int dotRadius{};
    int leadingGap{4};
    int cornerRadius{-1};
    int maximumChars{4};
};

DuiBadge::DuiBadge() : badge_(std::make_unique<Impl>()) {}
DuiBadge::~DuiBadge() = default;
DuiBadge::DuiBadge(DuiBadge&&) noexcept = default;
DuiBadge& DuiBadge::operator=(DuiBadge&&) noexcept = default;
void DuiBadge::SetText(std::string text) { badge_->text = std::move(text); }
const std::string& DuiBadge::Text() const { return badge_->text; }
void DuiBadge::SetCount(int count) { SetText(FormatCount(count)); }
std::string DuiBadge::FormatCount(int count) {
    if (count <= 0) return {};
    if (count > 99) return "99+";
    return std::to_string(count);
}
void DuiBadge::SetBackgroundColor(core::Color color) { badge_->background = color; badge_->backgroundOverride = true; }
core::Color DuiBadge::BackgroundColor() const { return badge_->backgroundOverride ? badge_->background : Theme().Get(core::ThemeSlot::BadgeBackground); }
void DuiBadge::SetTextStyle(render::DuiTextStyle style) { badge_->textStyle = std::move(style); badge_->textStyleOverride = true; }
const render::DuiTextStyle& DuiBadge::TextStyle() const { return badge_->textStyle; }
void DuiBadge::SetHideWhenEmpty(bool hide) { badge_->hideWhenEmpty = hide; }
bool DuiBadge::HideWhenEmpty() const { return badge_->hideWhenEmpty; }
bool DuiBadge::IsShowing() const { return EffectivelyVisible() && (!badge_->text.empty() || !badge_->hideWhenEmpty); }
void DuiBadge::SetLeadingDot(std::optional<core::Color> color) { badge_->leadingDot = color; }
std::optional<core::Color> DuiBadge::LeadingDot() const { return badge_->leadingDot; }
void DuiBadge::SetLeadingDotRadius(int radius) { badge_->dotRadius = radius; }
int DuiBadge::LeadingDotRadius() const { return badge_->dotRadius; }
void DuiBadge::SetLeadingGap(int gap) { badge_->leadingGap = gap; }
int DuiBadge::LeadingGap() const { return badge_->leadingGap; }
void DuiBadge::SetCornerRadius(int radius) { badge_->cornerRadius = radius; }
int DuiBadge::CornerRadius() const { return badge_->cornerRadius; }
void DuiBadge::SetMaxDisplayChars(int maximum) { badge_->maximumChars = maximum; }
int DuiBadge::MaxDisplayChars() const { return badge_->maximumChars; }

int DuiBadge::ContentWidth(int textWidth, int dotRadius, int leadingGap, bool hasDot) {
    if (!hasDot) return (std::max)(0, textWidth);
    return (std::max)(0, textWidth) + 2 * (std::max)(0, dotRadius) + (std::max)(0, leadingGap);
}
int DuiBadge::AutoDotRadius(int fontHeight) { return (std::max)(3, fontHeight > 0 ? fontHeight / 4 : 3); }
int DuiBadge::EffectiveCornerRadius(int rawRadius, int height) {
    if (rawRadius == -1) return (std::max)(0, height / 2);
    return (std::max)(0, rawRadius);
}
std::string DuiBadge::ApplyMaxChars(std::string_view text, int maximum) {
    if (maximum <= 0 || text.size() <= static_cast<std::size_t>(maximum)) return std::string(text);
    std::size_t end{};
    for (int count = 0; end < text.size() && count < maximum; ++count) {
        const unsigned char first = static_cast<unsigned char>(text[end]);
        const std::size_t width = first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
        end += (std::min)(width, text.size() - end);
    }
    return std::string(text.substr(0, end));
}

void DuiBadge::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!IsShowing()) return;
    const core::Rect available = core::Rect::Intersect(Bounds(), dirty);
    if (available.Empty()) return;
    render::DuiTextStyle textStyle = badge_->textStyle;
    if (!badge_->textStyleOverride)
        textStyle.color = Theme().Get(core::ThemeSlot::TextOnPrimary);
    const std::string display = ApplyMaxChars(badge_->text, badge_->maximumChars);
    const render::DuiTextMetrics text = canvas.MeasureText(display, textStyle, {});
    const bool hasDot = badge_->leadingDot.has_value();
    const int dotRadius = hasDot ? (badge_->dotRadius > 0 ? badge_->dotRadius : AutoDotRadius(text.size.height)) : 0;
    const int height = (std::min)(available.Height(), 20);
    const int width = (std::min)(available.Width(), (std::max)(height, ContentWidth(text.size.width, dotRadius, badge_->leadingGap, hasDot) + 10));
    const int centerX = (available.left + available.right) / 2;
    const int centerY = (available.top + available.bottom) / 2;
    const core::Rect pill{centerX - width / 2, centerY - height / 2, centerX - width / 2 + width, centerY - height / 2 + height};
    canvas.FillRoundedRect(pill, EffectiveCornerRadius(badge_->cornerRadius, height), BackgroundColor());
    if (hasDot) {
        const int groupLeft = pill.left + 5;
        const int dotCenterY = (pill.top + pill.bottom) / 2;
        canvas.FillEllipse({groupLeft, dotCenterY - dotRadius, groupLeft + dotRadius * 2, dotCenterY + dotRadius}, *badge_->leadingDot);
        if (!display.empty()) canvas.DrawText(display, {groupLeft + dotRadius * 2 + (std::max)(0, badge_->leadingGap), pill.top, pill.right - 5, pill.bottom}, textStyle, render::DuiTextAlignment::Start, false);
        return;
    }
    if (!display.empty()) canvas.DrawText(display, pill, textStyle, render::DuiTextAlignment::Center, false);
}

} // namespace ysDui::controls::basic
