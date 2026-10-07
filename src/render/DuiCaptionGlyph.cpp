/**
 * 文件名：DuiCaptionGlyph.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：标题栏按钮图标的 SVG 定义与按色光栅化实现。
 */
#include "ysDui/render/DuiCaptionGlyph.hpp"

#include <string_view>
#include <vector>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiSvg.hpp"

namespace ysDui::render {
namespace {
/** 图标像素尺寸（与 DuiDialog 标题栏按钮保持一致）。 */
constexpr int kGlyphPixels = 16;

inline constexpr std::string_view kMinimizeSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect x=".98" y="12.24" width="9.34" height="1.62"/></svg>)SVG";

inline constexpr std::string_view kMaximizeSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="m14.7,1.76h-6.7s-6.7-.01-6.7-.01v6.25s.01,6.26.01,6.26h13.4V1.76Zm-.86,11.61H2.13V3.85h11.71v9.52Z"/></svg>)SVG";

inline constexpr std::string_view kRestoreSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="m4.76,1.79v4.75H1.22v7.67h10.05v-4.68h3.52V1.79H4.76ZM10.41,13.44H2.05V8.21h8.35v5.23ZM14.15,8.74h-2.88v-2.2H5.42V3.53h8.73v5.21Z"/></svg>)SVG";

inline constexpr std::string_view kCloseSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="m3.18,2.31c-.44.1-.75.38-.89.8-.12.36-.05.8.18,1.11.05.06.13.15.17.19s.7.69,1.45,1.42,1.54,1.52,1.76,1.74c.22.22.4.4.4.41,0,.01-2.46,2.47-3.42,3.42-.42.41-.46.47-.55.73-.17.5.02,1.07.44,1.37.4.28,1.01.28,1.41,0,.11-.08.46-.41,2.64-2.58.7-.69,1.21-1.19,1.22-1.18.03,0,.65.62,2.85,2.81.56.56.95.92,1.01.97.15.1.27.14.46.17.62.11,1.18-.22,1.38-.8.04-.12.05-.18.05-.35,0-.25-.04-.41-.14-.61-.08-.15-.13-.2-1.78-1.84-.94-.93-1.79-1.79-1.9-1.9l-.2-.2.99-.98c.54-.54,1.39-1.38,1.89-1.87.78-.78.92-.92.98-1.03.2-.35.21-.79.04-1.16-.09-.18-.28-.4-.45-.5-.41-.24-.95-.23-1.34.04-.08.05-.59.55-1.5,1.45-.76.75-1.59,1.57-1.85,1.83-.26.25-.48.46-.5.47-.02,0-.44-.4-1.45-1.4-2.19-2.18-2.27-2.26-2.4-2.35-.26-.18-.66-.25-.97-.18Z"/></svg>)SVG";

/** @return 图标种类对应的 SVG 源。 */
std::string_view SvgOf(DuiCaptionGlyph glyph)
{
    switch (glyph)
    {
    case DuiCaptionGlyph::Minimize: return kMinimizeSvg;
    case DuiCaptionGlyph::Maximize: return kMaximizeSvg;
    case DuiCaptionGlyph::Restore: return kRestoreSvg;
    case DuiCaptionGlyph::Close: return kCloseSvg;
    }
    return {};
}
} // namespace

std::shared_ptr<const DuiImage> CreateCaptionGlyphImage(DuiCaptionGlyph glyph, core::Color color)
{
    auto buffer = RasterizeSvg(SvgOf(glyph), {kGlyphPixels, kGlyphPixels});
    if (!buffer)
        return {};
    // SVG 光栅化输出为白色 + alpha：按目标颜色预乘后得到着色副本
    std::vector<unsigned char> pixels(buffer->Bytes().begin(), buffer->Bytes().end());
    for (std::size_t offset{}; offset < pixels.size(); offset += 4)
    {
        const unsigned char alpha = pixels[offset + 3];
        pixels[offset] = static_cast<unsigned char>((static_cast<unsigned int>(color.blue) * alpha + 127U) / 255U);
        pixels[offset + 1] = static_cast<unsigned char>((static_cast<unsigned int>(color.green) * alpha + 127U) / 255U);
        pixels[offset + 2] = static_cast<unsigned char>((static_cast<unsigned int>(color.red) * alpha + 127U) / 255U);
    }
    return DuiImage::CreateBgra8Premultiplied(buffer->Size(), std::move(pixels));
}

} // namespace ysDui::render
