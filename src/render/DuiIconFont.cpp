/**
 * 文件名：DuiIconFont.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现平台无关图标字体服务。
 */
#include "ysDui/render/DuiIconFont.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace ysDui::render {
namespace {

constexpr std::array<std::string_view, 4> DefaultFamilies{
    "Font Awesome 6 Free Solid",
    "Font Awesome 6 Free",
    "Font Awesome 5 Free Solid",
    "FontAwesome",
};

} // namespace

class DuiIconFont::Impl {
public:
    explicit Impl(render::DuiFontRegistry& fontRegistry)
        : registry(fontRegistry) {
    }

    bool resolveFamily(std::string_view preferred) {
        if (!preferred.empty())
            return setFamily(std::string(preferred));

        for (const std::string_view candidate : DefaultFamilies) {
            if (registry.HasFamily(candidate)) {
                family = candidate;
                return true;
            }
        }
        return false;
    }

    bool setFamily(std::string value) {
        if (value.empty() || !registry.HasFamily(value)) {
            return false;
        }
        family = std::move(value);
        return true;
    }

    render::DuiFontRegistry& registry;
    std::string family;
};

DuiIconFont::DuiIconFont(render::DuiFontRegistry& registry)
    : impl_(std::make_unique<Impl>(registry)) {
}

DuiIconFont::~DuiIconFont() = default;
DuiIconFont::DuiIconFont(DuiIconFont&&) noexcept = default;
DuiIconFont& DuiIconFont::operator=(DuiIconFont&&) noexcept = default;

bool DuiIconFont::LoadFromFile(std::string path, std::string_view family) {
    return !path.empty() && impl_->registry.RegisterFile(std::move(path)) && impl_->resolveFamily(family);
}

bool DuiIconFont::LoadFromMemory(std::vector<std::uint8_t> bytes, std::string_view family) {
    return !bytes.empty() && impl_->registry.RegisterMemory(std::move(bytes)) && impl_->resolveFamily(family);
}

bool DuiIconFont::SetFamily(std::string family) {
    return impl_->setFamily(std::move(family));
}

std::string_view DuiIconFont::Family() const {
    return impl_->family;
}

bool DuiIconFont::IsReady() const {
    return !impl_->family.empty();
}

DuiTextStyle DuiIconFont::TextStyle(int pointSize, core::Color color, bool bold) const {
    return {color, impl_->family, (std::max)(1, pointSize), bold};
}

DuiTextMetrics DuiIconFont::Measure(Canvas& canvas, std::string_view glyph,
                                    const DuiTextStyle& style) const {
    if (!IsReady() || glyph.empty())
        return {};
    return canvas.MeasureText(glyph, style, {});
}

void DuiIconFont::Draw(Canvas& canvas, core::Rect bounds, std::string_view glyph,
                       const DuiTextStyle& style, DuiTextAlignment alignment) const {
    if (!IsReady() || bounds.Empty() || glyph.empty())
        return;
    canvas.DrawText(glyph, bounds, style, alignment, false);
}

} // namespace ysDui::render
