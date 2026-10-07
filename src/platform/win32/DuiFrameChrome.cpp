#include "ysDui/platform/win32/DuiFrameChrome.hpp"

#include <windows.h>
#include <dwmapi.h>

namespace ysDui::platform::win32 {
namespace {
constexpr ::DWORD WindowCornerPreference = 33;
constexpr int DefaultCornerPreference = 0;
constexpr int SquareCornerPreference = 1;
using VersionFunction = ::LONG(WINAPI*)(::PRTL_OSVERSIONINFOW);

bool QueryBuild(::DWORD& build)
{
    const auto module = ::GetModuleHandleW(L"ntdll.dll");
    const auto function = module ? reinterpret_cast<VersionFunction>(::GetProcAddress(module, "RtlGetVersion")) : nullptr;
    if (!function) return false;
    ::RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (function(&version) != 0) return false;
    build = version.dwBuildNumber;
    return true;
}
}

bool IsWindows11OrGreater()
{
    ::DWORD build{};
    return QueryBuild(build) && build >= 22000;
}

void ApplySquareFrameCorners(NativeWindowHandle window, bool preferSquare)
{
    const auto handle = reinterpret_cast<::HWND>(window.value);
    if (!handle || !::IsWindow(handle)) return;
    const int preference = preferSquare ? SquareCornerPreference : DefaultCornerPreference;
    ::DwmSetWindowAttribute(handle, WindowCornerPreference, &preference, sizeof(preference));
    if (!preferSquare || IsWindows11OrGreater()) { ::SetWindowRgn(handle, nullptr, TRUE); return; }
    ::RECT bounds{};
    ::GetWindowRect(handle, &bounds);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width > 0 && height > 0) ::SetWindowRgn(handle, ::CreateRectRgn(0, 0, width, height), TRUE);
}

} // namespace ysDui::platform::win32
