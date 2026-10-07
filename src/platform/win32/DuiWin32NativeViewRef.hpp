/**
 * 文件名：DuiWin32NativeViewRef.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明 Win32 后端私有的原生视图引用解析函数。
 */
#pragma once

#include <windows.h>

#include "DuiNativeViewRefAccess.hpp"
#include "ysDui/platform/win32/DuiWin32NativeViewRef.hpp"

namespace ysDui::platform::win32::detail {

[[nodiscard]] ::HWND ResolveNativeView(const ui::NativeViewRef& reference);

} // namespace ysDui::platform::win32::detail
