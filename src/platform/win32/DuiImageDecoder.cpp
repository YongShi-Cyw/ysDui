#include "ysDui/platform/win32/DuiImageDecoder.hpp"
#include "DuiWin32GraphicsContext.hpp"
#include "DuiWin32Utf8.hpp"
#include "DuiImageAccess.hpp"

#include <windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace ysDui::platform::win32 {
namespace {
struct DecodedBitmap final {
    core::Size size;
    std::vector<unsigned char> pixels;
};

struct DecodedAnimation final {
    std::vector<DecodedBitmap> frames;
    std::vector<int> delays;
};

std::uint16_t ReadU16Le(const unsigned char* data) {
    return static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8));
}

std::uint32_t ReadU32Le(const unsigned char* data) {
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
}

/**
 * 读取 32 位 BMP 的直通 alpha（自顶向下）
 * 背景：PNG 转 32 位 BMP 常用 40 字节 BITMAPINFOHEADER + BI_RGB，这种形态的 alpha 按规范
 *      "未定义"，GDI+ 会一律视为不透明（System.Drawing 实测 alpha 全为 255），
 *      透明背景因此丢失；此函数直接解析像素字节取回真实 alpha。
 * @param bytes BMP 完整字节
 * @param width 期望宽度（须与文件一致）
 * @param height 期望高度（须与文件一致）
 * @return 与像素一一对应的 alpha；非该形态或 alpha 全不透明时返回空
 */
std::vector<unsigned char> ReadBmp32TopDownAlpha(std::span<const std::uint8_t> bytes, int width,
                                                 int height) {
    constexpr std::size_t kFileHeaderBytes = 14; // BITMAPFILEHEADER
    constexpr std::size_t kInfoHeaderBytes = 40; // BITMAPINFOHEADER
    constexpr std::uint32_t kBiRgb = 0;          // 无压缩（alpha 未定义）
    if (width <= 0 || height <= 0 || bytes.size() < kFileHeaderBytes + kInfoHeaderBytes)
        return {};

    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
    if (data[0] != 'B' || data[1] != 'M') return {};

    const std::uint32_t offBits = ReadU32Le(data + 10);
    const std::uint32_t infoSize = ReadU32Le(data + 14);
    const auto bmpWidth = static_cast<std::int32_t>(ReadU32Le(data + 18));
    const auto bmpHeight = static_cast<std::int32_t>(ReadU32Le(data + 22));
    const std::uint16_t bpp = ReadU16Le(data + 28);
    const std::uint32_t compression = ReadU32Le(data + 30);
    // 仅接管 40 字节头 + BI_RGB 的 32 位；带 alpha mask 的 V4/V5 头 GDI+ 本就能正确读取
    if (infoSize != kInfoHeaderBytes || bpp != 32 || compression != kBiRgb) return {};
    if (bmpWidth != width || std::abs(bmpHeight) != height) return {};
    if (offBits < kFileHeaderBytes + kInfoHeaderBytes || offBits >= bytes.size()) return {};

    const bool bottomUp = bmpHeight > 0;
    const std::size_t stride = static_cast<std::size_t>(width) * 4;
    if (static_cast<std::size_t>(offBits) + stride * static_cast<std::size_t>(height) >
        bytes.size())
        return {};

    std::vector<unsigned char> alphas(static_cast<std::size_t>(width) * height);
    bool hasTransparent = false;
    for (int y = 0; y < height; ++y) {
        const int sourceY = bottomUp ? (height - 1 - y) : y;
        const unsigned char* row = data + offBits + stride * static_cast<std::size_t>(sourceY);
        for (int x = 0; x < width; ++x) {
            const unsigned char alpha = row[static_cast<std::size_t>(x) * 4 + 3];
            alphas[static_cast<std::size_t>(y) * width + x] = alpha;
            if (alpha != 255) hasTransparent = true;
        }
    }
    if (!hasTransparent) return {}; // alpha 全不透明：与 GDI+ 解出的结果等价
    return alphas;
}

/** 以 alpha 重算预乘 BGRA（GDI+ 视 alpha 为 255，故其 RGB 即原色）。 */
void ApplyAlphaPremultiplied(std::vector<unsigned char>& pixels,
                             const std::vector<unsigned char>& alphas) {
    for (std::size_t i = 0, offset = 0; i < alphas.size() && offset + 3 < pixels.size();
         ++i, offset += 4) {
        const unsigned int alpha = alphas[i];
        pixels[offset] = static_cast<unsigned char>((pixels[offset] * alpha + 127) / 255);
        pixels[offset + 1] = static_cast<unsigned char>((pixels[offset + 1] * alpha + 127) / 255);
        pixels[offset + 2] = static_cast<unsigned char>((pixels[offset + 2] * alpha + 127) / 255);
        pixels[offset + 3] = static_cast<unsigned char>(alpha);
    }
}

std::optional<DecodedBitmap> DecodeBitmap(Gdiplus::Bitmap& bitmap) {
    const UINT width = bitmap.GetWidth();
    const UINT height = bitmap.GetHeight();
    if (width == 0 || height == 0) return std::nullopt;
    const Gdiplus::Rect bounds(0, 0, static_cast<INT>(width), static_cast<INT>(height));
    Gdiplus::BitmapData source{};
    if (bitmap.LockBits(&bounds, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &source) != Gdiplus::Ok) return std::nullopt;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    const auto* row = static_cast<const unsigned char*>(source.Scan0);
    if (!row) {
        bitmap.UnlockBits(&source);
        return std::nullopt;
    }
    const std::ptrdiff_t stride = source.Stride;
    for (UINT y = 0; y < height; ++y) {
        std::memcpy(pixels.data() + static_cast<std::size_t>(y) * width * 4,
                    row + static_cast<std::ptrdiff_t>(y) * stride,
                    static_cast<std::size_t>(width) * 4);
    }
    bitmap.UnlockBits(&source);
    return DecodedBitmap{{static_cast<int>(width), static_cast<int>(height)}, std::move(pixels)};
}

IStream* CreateImageStream(std::span<const std::uint8_t> bytes) {
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())) return nullptr;
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!memory) return nullptr;
    void* destination = ::GlobalLock(memory);
    if (!destination) {
        ::GlobalFree(memory);
        return nullptr;
    }
    std::memcpy(destination, bytes.data(), bytes.size());
    ::GlobalUnlock(memory);
    IStream* stream{};
    if (::CreateStreamOnHGlobal(memory, TRUE, &stream) != S_OK) {
        ::GlobalFree(memory);
        return nullptr;
    }
    return stream;
}

std::optional<DecodedAnimation> DecodeAnimatedBitmap(Gdiplus::Bitmap& bitmap) {
    const UINT dimensionsCount = bitmap.GetFrameDimensionsCount();
    if (dimensionsCount == 0) return std::nullopt;
    std::vector<GUID> dimensions(dimensionsCount);
    if (bitmap.GetFrameDimensionsList(dimensions.data(), dimensionsCount) != Gdiplus::Ok) return std::nullopt;
    const UINT frameCount = bitmap.GetFrameCount(&dimensions.front());
    if (frameCount == 0) return std::nullopt;

    std::vector<int> delays(frameCount, 100);
    const UINT propertySize = bitmap.GetPropertyItemSize(PropertyTagFrameDelay);
    if (propertySize >= sizeof(Gdiplus::PropertyItem)) {
        std::vector<unsigned char> propertyData(propertySize);
        auto* property = reinterpret_cast<Gdiplus::PropertyItem*>(propertyData.data());
        if (bitmap.GetPropertyItem(PropertyTagFrameDelay, propertySize, property) == Gdiplus::Ok && property->value) {
            const auto* sourceDelays = static_cast<const LONG*>(property->value);
            const UINT count = (std::min)(frameCount,
                                          static_cast<UINT>(property->length / sizeof(LONG)));
            for (UINT index = 0; index < count; ++index) {
                delays[index] = (std::max)(100, static_cast<int>(sourceDelays[index]) * 10);
            }
        }
    }

    std::vector<DecodedBitmap> frames;
    frames.reserve(frameCount);
    for (UINT index = 0; index < frameCount; ++index) {
        if (bitmap.SelectActiveFrame(&dimensions.front(), index) != Gdiplus::Ok) return std::nullopt;
        std::optional<DecodedBitmap> decoded = DecodeBitmap(bitmap);
        if (!decoded) return std::nullopt;
        frames.push_back(std::move(*decoded));
    }
    return DecodedAnimation{std::move(frames), std::move(delays)};
}
}

std::optional<render::DuiImage> DuiImageDecoder::DecodeFile(std::string_view path) {
    if (path.empty()) return std::nullopt;
    if (!EnsureGdiPlus()) return std::nullopt;
    const std::wstring nativePath = detail::Utf8ToWide(path);
    Gdiplus::Bitmap bitmap(nativePath.c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok) return std::nullopt;
    std::optional<DecodedBitmap> decoded = DecodeBitmap(bitmap);
    if (!decoded) return std::nullopt;
    return render::DuiImageAccess::Create(decoded->size, render::DuiImageFormat::Bgra8Premultiplied,
        std::move(decoded->pixels));
}

std::optional<render::DuiImage> DuiImageDecoder::DecodeBytes(std::span<const std::uint8_t> bytes) {
    if (!EnsureGdiPlus()) return std::nullopt;
    IStream* stream = CreateImageStream(bytes);
    if (!stream) return std::nullopt;
    std::optional<DecodedBitmap> decoded;
    {
        Gdiplus::Bitmap bitmap(stream, FALSE);
        if (bitmap.GetLastStatus() == Gdiplus::Ok)
            decoded = DecodeBitmap(bitmap);
    }
    stream->Release();
    if (!decoded) return std::nullopt;
    // 40 字节头 + BI_RGB 的 32 位 BMP：alpha 需自行补回，否则透明背景会整片丢失
    const std::vector<unsigned char> alphas =
        ReadBmp32TopDownAlpha(bytes, decoded->size.width, decoded->size.height);
    if (!alphas.empty())
        ApplyAlphaPremultiplied(decoded->pixels, alphas);
    return render::DuiImageAccess::Create(decoded->size, render::DuiImageFormat::Bgra8Premultiplied,
        std::move(decoded->pixels));
}

std::shared_ptr<const render::DuiImage> DuiImageDecoder::LoadSharedFile(std::string_view path)
{
    std::optional<render::DuiImage> decoded = DecodeFile(path);
    if (!decoded || decoded->Empty())
        return {};
    return std::make_shared<const render::DuiImage>(std::move(*decoded));
}

std::shared_ptr<const render::DuiImage> DuiImageDecoder::LoadSharedBytes(
    std::span<const std::uint8_t> bytes)
{
    std::optional<render::DuiImage> decoded = DecodeBytes(bytes);
    if (!decoded || decoded->Empty())
        return {};
    return std::make_shared<const render::DuiImage>(std::move(*decoded));
}

std::shared_ptr<const render::DuiImage> DuiImageDecoder::LoadSharedBytesNearWhiteTransparent(
    std::span<const std::uint8_t> bytes, int whiteThreshold)
{
    if (!EnsureGdiPlus())
        return {};
    IStream* stream = CreateImageStream(bytes);
    if (!stream)
        return {};
    std::optional<DecodedBitmap> decoded;
    {
        Gdiplus::Bitmap bitmap(stream, FALSE);
        if (bitmap.GetLastStatus() == Gdiplus::Ok)
            decoded = DecodeBitmap(bitmap);
    }
    stream->Release();
    if (!decoded || decoded->pixels.empty())
        return {};

    const int threshold = (std::max)(0, (std::min)(255, whiteThreshold));
    for (std::size_t i = 0; i + 3 < decoded->pixels.size(); i += 4)
    {
        const unsigned char b = decoded->pixels[i];
        const unsigned char g = decoded->pixels[i + 1];
        const unsigned char r = decoded->pixels[i + 2];
        if (r >= threshold && g >= threshold && b >= threshold)
        {
            decoded->pixels[i] = 0;
            decoded->pixels[i + 1] = 0;
            decoded->pixels[i + 2] = 0;
            decoded->pixels[i + 3] = 0;
        }
    }

    auto image = render::DuiImageAccess::Create(decoded->size, render::DuiImageFormat::Bgra8Premultiplied,
                                                std::move(decoded->pixels));
    if (image.Empty())
        return {};
    return std::make_shared<const render::DuiImage>(std::move(image));
}

std::optional<render::DuiAnimatedImage> DuiImageDecoder::DecodeAnimatedFile(std::string_view path) {
    if (path.empty()) return std::nullopt;
    if (!EnsureGdiPlus()) return std::nullopt;
    const std::wstring nativePath = detail::Utf8ToWide(path);
    Gdiplus::Bitmap bitmap(nativePath.c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok) return std::nullopt;
    const auto decoded = DecodeAnimatedBitmap(bitmap);
    if (!decoded) return std::nullopt;
    std::vector<render::DuiImage> frames;
    frames.reserve(decoded->frames.size());
    for (const DecodedBitmap& frame : decoded->frames) {
        frames.push_back(render::DuiImageAccess::Create(frame.size, render::DuiImageFormat::Bgra8Premultiplied,
            frame.pixels));
    }
    return render::DuiImageAccess::CreateAnimated(std::move(frames), decoded->delays);
}

std::optional<render::DuiAnimatedImage> DuiImageDecoder::DecodeAnimatedBytes(
    std::span<const std::uint8_t> bytes) {
    if (!EnsureGdiPlus()) return std::nullopt;
    IStream* stream = CreateImageStream(bytes);
    if (!stream) return std::nullopt;
    std::optional<DecodedAnimation> decoded;
    {
        Gdiplus::Bitmap bitmap(stream, FALSE);
        if (bitmap.GetLastStatus() == Gdiplus::Ok)
            decoded = DecodeAnimatedBitmap(bitmap);
    }
    stream->Release();
    if (!decoded) return std::nullopt;
    std::vector<render::DuiImage> frames;
    frames.reserve(decoded->frames.size());
    for (const DecodedBitmap& frame : decoded->frames) {
        frames.push_back(render::DuiImageAccess::Create(frame.size, render::DuiImageFormat::Bgra8Premultiplied,
            frame.pixels));
    }
    return render::DuiImageAccess::CreateAnimated(std::move(frames), decoded->delays);
}

} // namespace ysDui::platform::win32
