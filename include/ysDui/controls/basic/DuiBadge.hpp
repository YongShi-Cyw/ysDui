#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::basic {

class DuiBadge final : public core::Control, public render::DuiRenderable {
public:
    DuiBadge();
    ~DuiBadge() override;
    DuiBadge(const DuiBadge&) = delete;
    DuiBadge& operator=(const DuiBadge&) = delete;
    DuiBadge(DuiBadge&&) noexcept;
    DuiBadge& operator=(DuiBadge&&) noexcept;

    void SetText(std::string text);
    [[nodiscard]] const std::string& Text() const;
    void SetCount(int count);
    [[nodiscard]] static std::string FormatCount(int count);
    void SetBackgroundColor(core::Color color);
    [[nodiscard]] core::Color BackgroundColor() const;
    void SetTextStyle(render::DuiTextStyle style);
    [[nodiscard]] const render::DuiTextStyle& TextStyle() const;
    void SetHideWhenEmpty(bool hide);
    [[nodiscard]] bool HideWhenEmpty() const;
    [[nodiscard]] bool IsShowing() const;

    void SetLeadingDot(std::optional<core::Color> color);
    [[nodiscard]] std::optional<core::Color> LeadingDot() const;
    void SetLeadingDotRadius(int radius);
    [[nodiscard]] int LeadingDotRadius() const;
    void SetLeadingGap(int gap);
    [[nodiscard]] int LeadingGap() const;
    void SetCornerRadius(int radius);
    [[nodiscard]] int CornerRadius() const;
    void SetMaxDisplayChars(int maximum);
    [[nodiscard]] int MaxDisplayChars() const;

    [[nodiscard]] static int ContentWidth(int textWidth, int dotRadius, int leadingGap, bool hasDot);
    [[nodiscard]] static int AutoDotRadius(int fontHeight);
    [[nodiscard]] static int EffectiveCornerRadius(int rawRadius, int height);
    [[nodiscard]] static std::string ApplyMaxChars(std::string_view text, int maximum);
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> badge_;
};

} // namespace ysDui::controls::basic
