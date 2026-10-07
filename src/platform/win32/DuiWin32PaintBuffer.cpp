/**
 * 文件名：DuiWin32PaintBuffer.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：实现可按客户区尺寸复用的 Win32 GDI 离屏缓冲。
 */
#include "DuiWin32PaintBuffer.hpp"

#include <windows.h>

namespace ysDui::platform::win32 {

class Win32PaintBuffer::Impl final
{
public:
    ~Impl()
    {
        Reset();
    }

    void Reset()
    {
        if (context != nullptr && previous != nullptr && previous != HGDI_ERROR)
            ::SelectObject(context, previous);
        if (bitmap != nullptr)
            ::DeleteObject(bitmap);
        if (context != nullptr)
            ::DeleteDC(context);
        context = nullptr;
        bitmap = nullptr;
        previous = nullptr;
        size = {};
    }

    core::Size size;
    ::HDC context{};
    ::HBITMAP bitmap{};
    ::HGDIOBJ previous{};
};

Win32PaintBuffer::Win32PaintBuffer() : buffer_(std::make_unique<Impl>()) {}
Win32PaintBuffer::~Win32PaintBuffer() = default;

bool Win32PaintBuffer::Prepare(std::uintptr_t target, int width, int height)
{
    const auto targetContext = reinterpret_cast<::HDC>(target);
    if (targetContext == nullptr || width <= 0 || height <= 0)
        return false;
    if (buffer_->context != nullptr && buffer_->bitmap != nullptr
        && buffer_->size == core::Size{width, height})
    {
        return true;
    }

    buffer_->Reset();
    buffer_->context = ::CreateCompatibleDC(targetContext);
    if (buffer_->context == nullptr)
        return false;
    buffer_->bitmap = ::CreateCompatibleBitmap(targetContext, width, height);
    if (buffer_->bitmap == nullptr)
    {
        buffer_->Reset();
        return false;
    }
    buffer_->previous = ::SelectObject(buffer_->context, buffer_->bitmap);
    if (buffer_->previous == nullptr || buffer_->previous == HGDI_ERROR)
    {
        buffer_->Reset();
        return false;
    }
    buffer_->size = {width, height};
    return true;
}

void Win32PaintBuffer::Reset()
{
    buffer_->Reset();
}

void Win32PaintBuffer::Clear() const
{
    Clear({0, 0, buffer_->size.width, buffer_->size.height});
}

void Win32PaintBuffer::Clear(core::Rect bounds) const
{
    bounds = core::Rect::Intersect(bounds, {0, 0, buffer_->size.width, buffer_->size.height});
    if (buffer_->context == nullptr || bounds.Empty())
        return;
    const ::RECT native{bounds.left, bounds.top, bounds.right, bounds.bottom};
    ::FillRect(buffer_->context, &native, static_cast<::HBRUSH>(::GetStockObject(WHITE_BRUSH)));
}

std::uintptr_t Win32PaintBuffer::Context() const
{
    return reinterpret_cast<std::uintptr_t>(buffer_->context);
}

void Win32PaintBuffer::Present(std::uintptr_t target) const
{
    Present(target, {0, 0, buffer_->size.width, buffer_->size.height});
}

void Win32PaintBuffer::Present(std::uintptr_t target, core::Rect bounds) const
{
    bounds = core::Rect::Intersect(bounds, {0, 0, buffer_->size.width, buffer_->size.height});
    const auto targetContext = reinterpret_cast<::HDC>(target);
    if (targetContext == nullptr || buffer_->context == nullptr || bounds.Empty())
        return;
    ::BitBlt(targetContext, bounds.left, bounds.top, bounds.Width(), bounds.Height(),
             buffer_->context, bounds.left, bounds.top, SRCCOPY);
}

core::Size Win32PaintBuffer::PixelSize() const
{
    return buffer_->size;
}

} // namespace ysDui::platform::win32
