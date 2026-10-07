/**
 * 文件名：DuiWin32GraphicsContext.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明 Win32 图形运行时的私有初始化入口。
 */
#pragma once

namespace ysDui::platform::win32 {

/**
 * 确保 Win32 图形运行时已初始化。
 * @return 初始化成功或已初始化时返回 true。
 */
[[nodiscard]] bool EnsureGdiPlus();

} // namespace ysDui::platform::win32
