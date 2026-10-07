#include "ysDui/controls/basic/DuiAvatar.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {
namespace {
bool IsSpace(char value) { return value == ' ' || value == '\t' || value == '\n' || value == '\r'; }
char ToUpper(char value) { return value >= 'a' && value <= 'z' ? static_cast<char>(value - 'a' + 'A') : value; }
std::size_t Utf8Length(std::string_view value, std::size_t offset)
{
    const unsigned char first = static_cast<unsigned char>(value[offset]);
    const std::size_t width = first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
    return (std::min)(width, value.size() - offset);
}
}

class DuiAvatar::Impl {
public:
    std::shared_ptr<const render::DuiImage> image;
    std::string name;
    core::Color fallback{45, 108, 223, 255};
    render::DuiTextStyle initials{{255, 255, 255, 255}, {}, 9, true};
    bool fallbackOverride{};
    bool initialsOverride{};
    DuiAvatarShape shape{DuiAvatarShape::Circle};
    int cornerRadius{8};
    DuiAvatarStatus status{DuiAvatarStatus::None};
};

DuiAvatar::DuiAvatar() : avatar_(std::make_unique<Impl>()) {}
DuiAvatar::~DuiAvatar() = default;
DuiAvatar::DuiAvatar(DuiAvatar&&) noexcept = default;
DuiAvatar& DuiAvatar::operator=(DuiAvatar&&) noexcept = default;
void DuiAvatar::SetImage(std::shared_ptr<const render::DuiImage> image) { avatar_->image = std::move(image); }
const std::shared_ptr<const render::DuiImage>& DuiAvatar::Image() const { return avatar_->image; }
void DuiAvatar::SetName(std::string name) { avatar_->name = std::move(name); }
const std::string& DuiAvatar::Name() const { return avatar_->name; }
void DuiAvatar::SetFallbackColor(core::Color color) { avatar_->fallback = color; avatar_->fallbackOverride = true; }
core::Color DuiAvatar::FallbackColor() const { return avatar_->fallbackOverride ? avatar_->fallback : Theme().Get(core::ThemeSlot::BrandPrimary); }
void DuiAvatar::SetInitialsStyle(render::DuiTextStyle style) { avatar_->initials = std::move(style); avatar_->initialsOverride = true; }
const render::DuiTextStyle& DuiAvatar::InitialsStyle() const { return avatar_->initials; }
void DuiAvatar::SetShape(DuiAvatarShape shape) { avatar_->shape = shape; }
DuiAvatarShape DuiAvatar::Shape() const { return avatar_->shape; }
void DuiAvatar::SetCornerRadius(int radius) { avatar_->cornerRadius = (std::max)(0, radius); }
int DuiAvatar::CornerRadius() const { return avatar_->cornerRadius; }
void DuiAvatar::SetStatus(DuiAvatarStatus status) { avatar_->status = status; }
DuiAvatarStatus DuiAvatar::Status() const { return avatar_->status; }
core::Color DuiAvatar::StatusColor(DuiAvatarStatus status) {
    switch (status) {
    case DuiAvatarStatus::Online: return {60, 200, 120, 255};
    case DuiAvatarStatus::Away: return {220, 170, 45, 255};
    case DuiAvatarStatus::Busy: return {220, 60, 60, 255};
    case DuiAvatarStatus::Offline: return {140, 145, 155, 255};
    case DuiAvatarStatus::None: return {};
    }
    return {};
}
std::string DuiAvatar::ComputeInitials(std::string_view name) {
    std::string first;
    std::string last;
    bool inWord{};
    for (std::size_t index = 0; index < name.size(); ++index) {
        if (IsSpace(name[index])) { inWord = false; continue; }
        if (inWord) continue;
        inWord = true;
        const std::size_t length = Utf8Length(name, index);
        std::string initial(name.substr(index, length));
        if (length == 1) initial[0] = ToUpper(initial[0]);
        if (first.empty()) first = initial;
        last = std::move(initial);
        index += length - 1;
    }
    return first == last ? first : first + last;
}
void DuiAvatar::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible()) return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty()) return;
    const int cornerRadius = (std::min)(avatar_->cornerRadius, (std::min)(bounds.Width(), bounds.Height()) / 2);
    const core::Color fallback = FallbackColor();
    render::DuiTextStyle initialsStyle = avatar_->initials;
    if (!avatar_->initialsOverride)
        initialsStyle.color = Theme().Get(core::ThemeSlot::TextOnPrimary);
    const bool hasImage = avatar_->image && !avatar_->image->Empty();
    if (hasImage) {
        if (avatar_->shape == DuiAvatarShape::Circle) canvas.DrawImageEllipse(*avatar_->image, bounds);
        else canvas.DrawImageRounded(*avatar_->image, bounds, cornerRadius);
    } else {
        if (avatar_->shape == DuiAvatarShape::Circle) canvas.FillEllipse(bounds, fallback);
        else canvas.FillRoundedRect(bounds, cornerRadius, fallback);
        const std::string initials = ComputeInitials(avatar_->name);
        if (!initials.empty())
            canvas.DrawText(initials, bounds, initialsStyle, render::DuiTextAlignment::Center, false);
    }
    if (avatar_->status == DuiAvatarStatus::None) return;
    const int side = (std::min)(bounds.Width(), bounds.Height());
    const int dotRadius = (std::max)(4, side / 8);
    const int centerX = bounds.left + bounds.Width() * 78 / 100;
    const int centerY = bounds.top + bounds.Height() * 78 / 100;
    const int ringRadius = dotRadius + 2;
    canvas.FillEllipse({centerX - ringRadius, centerY - ringRadius, centerX + ringRadius, centerY + ringRadius},
                       Theme().Get(core::ThemeSlot::SurfaceBackground));
    const core::ThemeSlot statusSlot = avatar_->status == DuiAvatarStatus::Online
        ? core::ThemeSlot::AvatarStatusOnline
        : avatar_->status == DuiAvatarStatus::Away ? core::ThemeSlot::AvatarStatusAway
        : avatar_->status == DuiAvatarStatus::Busy ? core::ThemeSlot::AvatarStatusBusy
                                                  : core::ThemeSlot::AvatarStatusOffline;
    canvas.FillEllipse({centerX - dotRadius, centerY - dotRadius, centerX + dotRadius, centerY + dotRadius},
                       Theme().Get(statusSlot));
}

} // namespace ysDui::controls::basic
