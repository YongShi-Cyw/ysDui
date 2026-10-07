/**
 * 文件名：DuiNativeWindowHandle.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明仅供 Win32 互操作 API 使用的非拥有窗口句柄值。
 */
#pragma once

#include <cstdint>

namespace ysDui::platform::win32 {

struct NativeWindowHandle final
{
    std::uintptr_t value{};
    [[nodiscard]] constexpr bool Empty() const { return value == 0; }
};

} // namespace ysDui::platform::win32
