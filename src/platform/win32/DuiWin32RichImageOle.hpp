#pragma once

#define NOMINMAX
#include <windows.h>

namespace ysDui::platform::win32 {

[[nodiscard]] bool InsertRichImageOle(HWND richEdit, HBITMAP bitmap);

} // namespace ysDui::platform::win32
