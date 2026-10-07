/**
 * 文件名：win32_offscreen_renderer_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-27
 * 用途：验证 Win32 离屏渲染器导出的 BGRA 像素和 PNG 编解码兼容性。
 */
#define NOMINMAX
#include <windows.h>

#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

#include "ysDui/platform/win32/DuiWin32OffscreenRenderer.hpp"
#include "DuiWin32PaintBuffer.hpp"

namespace {

bool HasAntialiasedEdge(const ysDui::render::DuiPixelBuffer& pixels)
{
    const auto& bytes = pixels.Bytes();
    for (std::size_t offset = 0; offset + 3 < bytes.size(); offset += 4)
    {
        if (bytes[offset] > 0 && bytes[offset] < 255
            && bytes[offset] == bytes[offset + 1] && bytes[offset] == bytes[offset + 2])
        {
            return true;
        }
    }
    return false;
}

} // namespace

int main()
{
    ::HDC screen = ::GetDC(nullptr);
    assert(screen != nullptr);
    ysDui::platform::win32::Win32PaintBuffer paintBuffer;
    const auto screenValue = reinterpret_cast<std::uintptr_t>(screen);
    assert(paintBuffer.Prepare(screenValue, 64, 32));
    const std::uintptr_t reusedContext = paintBuffer.Context();
    assert(reusedContext != 0);
    assert((paintBuffer.PixelSize() == ysDui::core::Size{64, 32}));
    assert(paintBuffer.Prepare(screenValue, 64, 32));
    assert(paintBuffer.Context() == reusedContext);
    assert(paintBuffer.Prepare(screenValue, 128, 48));
    assert((paintBuffer.PixelSize() == ysDui::core::Size{128, 48}));
    paintBuffer.Reset();
    assert(paintBuffer.Context() == 0);
    assert((paintBuffer.PixelSize() == ysDui::core::Size{}));
    assert(paintBuffer.Prepare(screenValue, 128, 48));

    const ::RECT blackBounds{0, 0, 128, 48};
    ::FillRect(reinterpret_cast<::HDC>(paintBuffer.Context()), &blackBounds,
               static_cast<::HBRUSH>(::GetStockObject(BLACK_BRUSH)));
    paintBuffer.Clear({16, 8, 48, 24});
    const auto bufferContext = reinterpret_cast<::HDC>(paintBuffer.Context());
    assert(::GetPixel(bufferContext, 8, 8) == RGB(0, 0, 0));
    assert(::GetPixel(bufferContext, 24, 12) == RGB(255, 255, 255));

    ysDui::platform::win32::Win32PaintBuffer targetBuffer;
    assert(targetBuffer.Prepare(screenValue, 128, 48));
    ::FillRect(reinterpret_cast<::HDC>(targetBuffer.Context()), &blackBounds,
               static_cast<::HBRUSH>(::GetStockObject(BLACK_BRUSH)));
    paintBuffer.Present(targetBuffer.Context(), {16, 8, 48, 24});
    const auto targetContext = reinterpret_cast<::HDC>(targetBuffer.Context());
    assert(::GetPixel(targetContext, 8, 8) == RGB(0, 0, 0));
    assert(::GetPixel(targetContext, 24, 12) == RGB(255, 255, 255));
    assert(::ReleaseDC(nullptr, screen) == 1);

    const auto rendered = ysDui::platform::win32::DuiWin32OffscreenRenderer::Render({2, 1},
        [](ysDui::render::Canvas& canvas, ysDui::core::Rect bounds)
        {
            canvas.FillRect(bounds, {255, 0, 0, 255});
        });
    const ysDui::core::Size expectedSize{2, 1};
    const std::vector<std::uint8_t> expectedPixels{0, 0, 255, 255, 0, 0, 255, 255};
    assert(rendered.has_value());
    assert(rendered->Size() == expectedSize);
    assert(rendered->Bytes() == expectedPixels);
    assert(!ysDui::platform::win32::DuiWin32OffscreenRenderer::Render({}, {}).has_value());

    const auto vectorShapes = ysDui::platform::win32::DuiWin32OffscreenRenderer::Render({32, 16},
        [](ysDui::render::Canvas& canvas, ysDui::core::Rect bounds)
        {
            canvas.FillRect(bounds, {255, 255, 255, 255});
            canvas.FillEllipse({2, 2, 14, 14}, {0, 0, 0, 255});
            canvas.StrokeArc({18, 2, 30, 14}, 0.0f, 270.0f, {0, 0, 0, 255}, 2.0f);
        });
    assert(vectorShapes.has_value());
    assert(HasAntialiasedEdge(*vectorShapes));

    const auto onePixelBorder = ysDui::platform::win32::DuiWin32OffscreenRenderer::Render({8, 8},
        [](ysDui::render::Canvas& canvas, ysDui::core::Rect bounds)
        {
            canvas.FillRect(bounds, {255, 255, 255, 255});
            canvas.StrokeRoundedRect({1, 1, 7, 7}, 0, {0, 0, 0, 255}, 1.0F);
        });
    assert(onePixelBorder.has_value());
    const auto& borderBytes = onePixelBorder->Bytes();
    const auto blueAt = [&borderBytes](int x, int y)
    {
        return borderBytes[(static_cast<std::size_t>(y) * 8 + static_cast<std::size_t>(x)) * 4];
    };
    assert(blueAt(0, 3) == 255 && blueAt(1, 3) == 0 && blueAt(2, 3) == 255);

    std::vector<std::uint8_t> blackPixels(8 * 8 * 4);
    for (std::size_t offset = 3; offset < blackPixels.size(); offset += 4)
    {
        blackPixels[offset] = 255;
    }
    const auto image = ysDui::render::DuiImage::CreateBgra8Premultiplied({8, 8}, std::move(blackPixels));
    assert(image != nullptr);
    const auto clippedImage = ysDui::platform::win32::DuiWin32OffscreenRenderer::Render({16, 16},
        [&image](ysDui::render::Canvas& canvas, ysDui::core::Rect bounds)
        {
            canvas.FillRect(bounds, {255, 255, 255, 255});
            canvas.DrawImageEllipse(*image, {2, 2, 14, 14});
        });
    assert(clippedImage.has_value());
    assert(HasAntialiasedEdge(*clippedImage));
    return 0;
}
