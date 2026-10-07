/**
 * 文件名：DuiSvg.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：使用 NanoSVG 实现平台无关 SVG 光栅化。
 */
#include "ysDui/render/DuiSvg.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "DuiUtf8Path.hpp"

namespace ysDui::render {
namespace {

constexpr int MAX_DIMENSION = 32768;
constexpr std::uintmax_t MAX_SVG_BYTES = 64U * 1024U * 1024U;

int RoundDimension(float value)
{
    if (!(value > 0.0F) || value > static_cast<float>(MAX_DIMENSION))
        return 0;
    return static_cast<int>(std::lround(value));
}

std::optional<core::Size> ResolveSize(const NSVGimage& image, core::Size requested)
{
    if (image.width <= 0.0F || image.height <= 0.0F || requested.width < 0 || requested.height < 0)
        return std::nullopt;

    core::Size size = requested;
    if (size.width == 0 && size.height == 0)
    {
        size = {RoundDimension(image.width), RoundDimension(image.height)};
    }
    else if (size.width == 0)
    {
        size.width = RoundDimension(image.width * static_cast<float>(size.height) / image.height);
    }
    else if (size.height == 0)
    {
        size.height = RoundDimension(image.height * static_cast<float>(size.width) / image.width);
    }

    const auto area = static_cast<std::uint64_t>(size.width) * static_cast<std::uint64_t>(size.height);
    if (size.Empty() || size.width > MAX_DIMENSION || size.height > MAX_DIMENSION
        || area > std::numeric_limits<std::uint32_t>::max() / 4U)
        return std::nullopt;
    return size;
}

std::optional<DuiPixelBuffer> Rasterize(NSVGimage& image, core::Size requested)
{
    const auto size = ResolveSize(image, requested);
    if (!size)
        return std::nullopt;

    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
    if (!rasterizer)
        return std::nullopt;

    const std::size_t byteCount = static_cast<std::size_t>(size->width) * size->height * 4U;
    std::vector<unsigned char> rgba(byteCount);
    const float scaleX = static_cast<float>(size->width) / image.width;
    const float scaleY = static_cast<float>(size->height) / image.height;
    const float scale = scaleX < scaleY ? scaleX : scaleY;
    const float translateX = (static_cast<float>(size->width) - image.width * scale) * 0.5F;
    const float translateY = (static_cast<float>(size->height) - image.height * scale) * 0.5F;
    nsvgRasterize(rasterizer, &image, translateX, translateY, scale, rgba.data(),
                  size->width, size->height, size->width * 4);
    nsvgDeleteRasterizer(rasterizer);

    std::vector<std::uint8_t> bgra(byteCount);
    for (std::size_t offset = 0; offset < byteCount; offset += 4)
    {
        const auto alpha = rgba[offset + 3];
        bgra[offset] = static_cast<std::uint8_t>((rgba[offset + 2] * alpha + 127U) / 255U);
        bgra[offset + 1] = static_cast<std::uint8_t>((rgba[offset + 1] * alpha + 127U) / 255U);
        bgra[offset + 2] = static_cast<std::uint8_t>((rgba[offset] * alpha + 127U) / 255U);
        bgra[offset + 3] = alpha;
    }
    return DuiPixelBuffer::Create(*size, std::move(bgra));
}

} // namespace

std::optional<DuiPixelBuffer> RasterizeSvg(std::string_view svgUtf8, core::Size targetSize)
{
    if (svgUtf8.empty())
        return std::nullopt;

    std::vector<char> input(svgUtf8.begin(), svgUtf8.end());
    input.push_back('\0');
    char* parseStart = input.data();
    if (input.size() >= 4 && static_cast<unsigned char>(parseStart[0]) == 0xEFU
        && static_cast<unsigned char>(parseStart[1]) == 0xBBU
        && static_cast<unsigned char>(parseStart[2]) == 0xBFU)
        parseStart += 3;

    NSVGimage* image = nsvgParse(parseStart, "px", 96.0F);
    if (!image)
        return std::nullopt;
    auto result = Rasterize(*image, targetSize);
    nsvgDelete(image);
    return result;
}

std::optional<DuiPixelBuffer> RasterizeSvgFile(std::string_view path, core::Size targetSize)
{
    if (path.empty())
        return std::nullopt;

    const auto nativePath = core::detail::PathFromUtf8(path);
    if (!nativePath)
        return std::nullopt;

    std::ifstream file(*nativePath, std::ios::binary | std::ios::ate);
    if (!file)
        return std::nullopt;
    const auto size = file.tellg();
    if (size < 0 || static_cast<std::uintmax_t>(size) > MAX_SVG_BYTES)
        return std::nullopt;

    std::vector<char> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(bytes.data(), size))
        return std::nullopt;
    return RasterizeSvg({bytes.data(), bytes.size()}, targetSize);
}

} // namespace ysDui::render
