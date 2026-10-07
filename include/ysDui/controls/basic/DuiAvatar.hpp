#pragma once

#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::basic {

enum class DuiAvatarShape { Circle, RoundedRectangle };
enum class DuiAvatarStatus { None, Online, Away, Busy, Offline };

class DuiAvatar final : public core::Control, public render::DuiRenderable {
public:
    DuiAvatar();
    ~DuiAvatar() override;
    DuiAvatar(const DuiAvatar&) = delete;
    DuiAvatar& operator=(const DuiAvatar&) = delete;
    DuiAvatar(DuiAvatar&&) noexcept;
    DuiAvatar& operator=(DuiAvatar&&) noexcept;

    void SetImage(std::shared_ptr<const render::DuiImage> image);
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& Image() const;
    void SetName(std::string name);
    [[nodiscard]] const std::string& Name() const;
    void SetFallbackColor(core::Color color);
    [[nodiscard]] core::Color FallbackColor() const;
    void SetInitialsStyle(render::DuiTextStyle style);
    [[nodiscard]] const render::DuiTextStyle& InitialsStyle() const;
    void SetShape(DuiAvatarShape shape);
    [[nodiscard]] DuiAvatarShape Shape() const;
    void SetCornerRadius(int radius);
    [[nodiscard]] int CornerRadius() const;
    void SetStatus(DuiAvatarStatus status);
    [[nodiscard]] DuiAvatarStatus Status() const;
    [[nodiscard]] static core::Color StatusColor(DuiAvatarStatus status);
    [[nodiscard]] static std::string ComputeInitials(std::string_view name);
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> avatar_;
};

} // namespace ysDui::controls::basic
