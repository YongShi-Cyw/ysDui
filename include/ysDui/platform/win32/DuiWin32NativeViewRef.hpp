/**
 * 文件名：DuiWin32NativeViewRef.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：提供 Win32 原生窗口与跨平台原生视图引用的互操作入口。
 */
#pragma once

#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/ui/DuiNativeViewRef.hpp"

namespace ysDui::platform::win32 {

/** 将有效的 Win32 窗口句柄包装为原生视图引用。 */
[[nodiscard]] ui::NativeViewRef CreateNativeViewRef(NativeWindowHandle handle);

} // namespace ysDui::platform::win32
