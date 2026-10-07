/**
 * 文件名：DuiWin32OffscreenRenderer.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-27
 * 用途：实现 Win32 DIB 离屏绘制并导出平台无关 BGRA 像素。
 */
#include "ysDui/platform/win32/DuiWin32OffscreenRenderer.hpp"

#include <cstring>
#include <cstdint>
#include <utility>
#include <vector>

#include <windows.h>

#include "DuiWin32Canvas.hpp"

namespace ysDui::platform::win32 {

std::optional<render::DuiPixelBuffer> DuiWin32OffscreenRenderer::Render(
    core::Size size, std::function<void(render::Canvas&, core::Rect)> paint)
{
    if (size.width <= 0 || size.height <= 0 || !paint)
        return std::nullopt;

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = size.width;
    bitmapInfo.bmiHeader.biHeight = -size.height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits{};
    HDC deviceContext = ::CreateCompatibleDC(nullptr);
    if (deviceContext == nullptr)
        return std::nullopt;
    HBITMAP bitmap = ::CreateDIBSection(deviceContext, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap == nullptr || bits == nullptr)
    {
        ::DeleteDC(deviceContext);
        return std::nullopt;
    }
    HGDIOBJ previous = ::SelectObject(deviceContext, bitmap);
    if (previous == nullptr || previous == HGDI_ERROR)
    {
        ::DeleteObject(bitmap);
        ::DeleteDC(deviceContext);
        return std::nullopt;
    }

    const std::size_t byteCount = static_cast<std::size_t>(size.width) * size.height * 4;
    std::vector<std::uint8_t> pixels(byteCount, 0);
    auto canvas = CreateWin32Canvas(reinterpret_cast<std::uintptr_t>(deviceContext));
    paint(*canvas, {0, 0, size.width, size.height});
    canvas.reset();
    std::memcpy(pixels.data(), bits, byteCount);
    for (std::size_t index = 3; index < pixels.size(); index += 4)
        pixels[index] = 255;

    ::SelectObject(deviceContext, previous);
    ::DeleteObject(bitmap);
    ::DeleteDC(deviceContext);
    return render::DuiPixelBuffer::Create(size, std::move(pixels));
}

} // namespace ysDui::platform::win32
