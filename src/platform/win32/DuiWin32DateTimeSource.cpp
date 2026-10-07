#include "ysDui/platform/win32/DuiWin32DateTimeSource.hpp"

#include <windows.h>

namespace ysDui::platform::win32 {
core::DateTime Win32DateTimeSource::LocalNow() const
{
    ::SYSTEMTIME value{};
    ::GetLocalTime(&value);
    return {value.wYear, value.wMonth, value.wDay, value.wHour, value.wMinute, value.wSecond};
}
} // namespace ysDui::platform::win32
