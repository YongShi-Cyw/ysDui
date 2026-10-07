#pragma once

#include <memory>

#define NOMINMAX
#include <windows.h>

#include "ysDui/core/DuiHost.hpp"

struct IRawElementProviderSimple;

namespace ysDui::platform::win32 {

[[nodiscard]] IRawElementProviderSimple* CreateUiaElementProvider(
    HWND window, const std::shared_ptr<core::Host>& host, core::Control* control);
[[nodiscard]] LRESULT ReturnUiaProvider(HWND window, const std::shared_ptr<core::Host>& host,
                                        WPARAM word, LPARAM data);

} // namespace ysDui::platform::win32
