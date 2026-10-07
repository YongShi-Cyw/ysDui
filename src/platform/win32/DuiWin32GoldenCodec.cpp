/**
 * 文件名：DuiWin32GoldenCodec.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现 Win32 基准图像的 PNG 编解码能力。
 */
#include "ysDui/platform/win32/DuiWin32GoldenCodec.hpp"
#include "DuiWin32GraphicsContext.hpp"
#include "DuiWin32Utf8.hpp"

#include <windows.h>
#include <gdiplus.h>

#include <cstring>
#include <filesystem>
#include <system_error>
#include <vector>

namespace ysDui::platform::win32 {
namespace {

bool FindPngEncoder(CLSID& identifier)
{
    UINT count{};
    UINT bytes{};
    if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok || bytes == 0)
        return false;

    std::vector<std::uint8_t> storage(bytes);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
    if (Gdiplus::GetImageEncoders(count, bytes, encoders) != Gdiplus::Ok)
        return false;

    for (UINT index{}; index < count; ++index)
    {
        if (std::wcscmp(encoders[index].MimeType, L"image/png") == 0)
        {
            identifier = encoders[index].Clsid;
            return true;
        }
    }
    return false;
}

} // namespace

bool DuiWin32GoldenCodec::SavePng(const render::DuiPixelBuffer& pixels, std::string_view path)
{
    if (pixels.Empty() || path.empty() || !EnsureGdiPlus())
        return false;

    const std::wstring nativePath = detail::Utf8ToWide(path);
    const std::filesystem::path destination(nativePath);
    std::error_code error;
    if (!destination.parent_path().empty())
        std::filesystem::create_directories(destination.parent_path(), error);
    if (error)
        return false;

    const core::Size size = pixels.Size();
    Gdiplus::Bitmap bitmap(size.width, size.height, size.width * 4, PixelFormat32bppPARGB,
                            const_cast<BYTE*>(reinterpret_cast<const BYTE*>(pixels.Bytes().data())));
    CLSID encoder{};
    return FindPngEncoder(encoder) && bitmap.Save(nativePath.c_str(), &encoder, nullptr) == Gdiplus::Ok;
}

std::optional<render::DuiPixelBuffer> DuiWin32GoldenCodec::LoadPng(std::string_view path)
{
    if (path.empty() || !EnsureGdiPlus())
        return std::nullopt;

    const std::wstring nativePath = detail::Utf8ToWide(path);
    Gdiplus::Bitmap bitmap(nativePath.c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok)
        return std::nullopt;

    const UINT width = bitmap.GetWidth();
    const UINT height = bitmap.GetHeight();
    if (width == 0 || height == 0)
        return std::nullopt;

    const Gdiplus::Rect bounds(0, 0, static_cast<INT>(width), static_cast<INT>(height));
    Gdiplus::BitmapData source{};
    if (bitmap.LockBits(&bounds, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &source) != Gdiplus::Ok)
        return std::nullopt;

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4U);
    const auto* row = static_cast<const std::uint8_t*>(source.Scan0);
    if (!row)
    {
        bitmap.UnlockBits(&source);
        return std::nullopt;
    }
    for (UINT index{}; index < height; ++index)
    {
        std::memcpy(pixels.data() + static_cast<std::size_t>(index) * width * 4U,
                    row + static_cast<std::ptrdiff_t>(index) * source.Stride,
                    static_cast<std::size_t>(width) * 4U);
    }
    bitmap.UnlockBits(&source);
    return render::DuiPixelBuffer::Create({static_cast<int>(width), static_cast<int>(height)}, std::move(pixels));
}

} // namespace ysDui::platform::win32
