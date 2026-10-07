#include "ysDui/platform/win32/DuiWin32ColorChooser.hpp"

#include <windows.h>
#include <commdlg.h>

#include <cstdint>

namespace ysDui::platform::win32 {
class Win32ColorChooser::Impl { public: ::COLORREF custom[16]{}; };
Win32ColorChooser::Win32ColorChooser() : impl_(std::make_unique<Impl>()) {}
Win32ColorChooser::~Win32ColorChooser() = default;
Win32ColorChooser::Win32ColorChooser(Win32ColorChooser&&) noexcept = default;
Win32ColorChooser& Win32ColorChooser::operator=(Win32ColorChooser&&) noexcept = default;
ui::DuiColorChooserResult Win32ColorChooser::Choose(core::Color initial) {
    CHOOSECOLORW dialog{};
    dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = ::GetActiveWindow();
    dialog.rgbResult = RGB(initial.red, initial.green, initial.blue); dialog.lpCustColors = impl_->custom;
    dialog.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!::ChooseColorW(&dialog)) return {};
    return {true, {static_cast<std::uint8_t>(GetRValue(dialog.rgbResult)), static_cast<std::uint8_t>(GetGValue(dialog.rgbResult)), static_cast<std::uint8_t>(GetBValue(dialog.rgbResult)), 255}};
}
} // namespace ysDui::platform::win32
