/**
 * 文件名：DuiWin32PaintBuffer.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明 Win32 窗口绘制使用的可复用离屏缓冲。
 */
#pragma once

#include <cstdint>
#include <memory>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::platform::win32 {

class Win32PaintBuffer final
{
public:
    Win32PaintBuffer();
    ~Win32PaintBuffer();
    Win32PaintBuffer(const Win32PaintBuffer&) = delete;
    Win32PaintBuffer& operator=(const Win32PaintBuffer&) = delete;

    [[nodiscard]] bool Prepare(std::uintptr_t target, int width, int height);
    void Reset();
    void Clear() const;
    void Clear(core::Rect bounds) const;
    [[nodiscard]] std::uintptr_t Context() const;
    void Present(std::uintptr_t target) const;
    void Present(std::uintptr_t target, core::Rect bounds) const;
    [[nodiscard]] core::Size PixelSize() const;

private:
    class Impl;
    std::unique_ptr<Impl> buffer_;
};

} // namespace ysDui::platform::win32
