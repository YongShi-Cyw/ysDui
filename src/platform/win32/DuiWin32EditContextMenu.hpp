/**
 * 文件名：DuiWin32EditContextMenu.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明 Win32 文本输入右键菜单的私有显示入口。
 */
#pragma once

#include <windows.h>

#include "ysDui/controls/input/DuiEditContextMenu.hpp"

namespace ysDui::platform::win32 {

/**
 * 显示编辑命令菜单并将选中的命令发送给原生编辑窗口。
 * @param handle 原生编辑窗口。
 * @param point 菜单屏幕坐标，{-1, -1} 表示窗口中心。
 * @param state 由适配器采集的无平台编辑状态。
 * @return 已处理时返回 true。
 */
bool ShowWin32EditContextMenu(::HWND handle, ::POINT point, const controls::input::DuiEditContextState& state);

} // namespace ysDui::platform::win32
