#pragma once

#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"

namespace ysDui::platform::win32 {

[[nodiscard]] bool IsWindows11OrGreater();
void ApplySquareFrameCorners(NativeWindowHandle window, bool preferSquare);

} // namespace ysDui::platform::win32
