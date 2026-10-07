/**
 * 文件名：DuiWin32EditContextMenu.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现 Win32 文本输入右键菜单的创建和命令分派。
 */
#include "DuiWin32EditContextMenu.hpp"
#include "DuiWin32Utf8.hpp"

namespace ysDui::platform::win32 {

bool ShowWin32EditContextMenu(::HWND handle, ::POINT point, const controls::input::DuiEditContextState& state)
{
    if (handle == nullptr || !::IsWindow(handle))
        return false;
    if (point.x == -1 && point.y == -1) {
        ::RECT bounds{};
        ::GetWindowRect(handle, &bounds);
        point = {(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
    }

    ::HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr)
        return false;
    for (const auto& item : controls::input::BuildDuiEditContextMenu(state)) {
        if (item.command == controls::input::DuiEditContextCommand::Separator) {
            ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            continue;
        }
        const std::string_view label = controls::input::DuiEditContextCommandLabel(item.command);
        const std::wstring nativeLabel = detail::Utf8ToWide(label);
        const UINT flags = MF_STRING | (item.enabled ? MF_ENABLED : MF_GRAYED);
        ::AppendMenuW(menu, flags, static_cast<UINT_PTR>(item.command) + 1,
                      nativeLabel.c_str());
    }

    const UINT command = ::TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                          point.x, point.y, 0, handle, nullptr);
    ::DestroyMenu(menu);
    if (command == 0)
        return true;
    switch (static_cast<controls::input::DuiEditContextCommand>(command - 1)) {
    case controls::input::DuiEditContextCommand::Cut: ::SendMessageW(handle, WM_CUT, 0, 0); break;
    case controls::input::DuiEditContextCommand::Copy: ::SendMessageW(handle, WM_COPY, 0, 0); break;
    case controls::input::DuiEditContextCommand::Paste: ::SendMessageW(handle, WM_PASTE, 0, 0); break;
    case controls::input::DuiEditContextCommand::SelectAll: ::SendMessageW(handle, EM_SETSEL, 0, -1); break;
    case controls::input::DuiEditContextCommand::Separator: break;
    }
    return true;
}

} // namespace ysDui::platform::win32
