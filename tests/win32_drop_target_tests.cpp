/**
 * 文件名：win32_drop_target_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：验证 Win32 拖放接收器的 OLE 注册生命周期和公开操作。
 */
#define NOMINMAX
#include <windows.h>
#include <ole2.h>

#include <cassert>
#include <cstdint>

#include "ysDui/ui/DuiDropTarget.hpp"
#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/platform/win32/DuiWin32DropTarget.hpp"

int main()
{
    assert(::OleInitialize(nullptr) == S_OK);
    ::HWND host = ::CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                     0, 0, 1, 1, nullptr, nullptr,
                                     ::GetModuleHandleW(nullptr), nullptr);
    assert(host != nullptr);
    {
        ysDui::platform::win32::Win32DropTarget target(
            {reinterpret_cast<std::uintptr_t>(host)});
        bool handled{};
        target.SetHandler([&handled](const ysDui::ui::DuiDropPayload&) { handled = true; });
        target.SetEnabled(false);
        target.SetEnabled(true);
        assert(!handled);
    }
    ::DestroyWindow(host);
    ::OleUninitialize();
}
