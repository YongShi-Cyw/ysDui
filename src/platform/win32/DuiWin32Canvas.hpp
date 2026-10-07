/**
 * 文件名：DuiWin32Canvas.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明 Win32 私有 Canvas 工厂，供平台后端离屏绘制使用。
 */
#pragma once

#include <cstdint>
#include <memory>

#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/core/DuiDpi.hpp"

namespace ysDui::platform::win32 {

/**
 * 创建绑定到 Win32 设备上下文的私有 Canvas。
 * @param deviceContext Win32 HDC 的整数表示，仅限平台后端传递。
 * @return 绑定到指定设备上下文的绘制协议实现。
 */
[[nodiscard]] std::unique_ptr<render::Canvas> CreateWin32Canvas(
    std::uintptr_t deviceContext, int dpi = core::DuiDpiScale::DefaultDpi);

} // namespace ysDui::platform::win32
