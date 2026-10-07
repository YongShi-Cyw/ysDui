#include "ysDui/platform/win32/DuiWin32ModalDialog.hpp"

#define NOMINMAX
#include <windows.h>

#include "ysDui/controls/window/DuiDialog.hpp"

namespace ysDui::platform::win32 {

int DuiWin32ModalDialog::Wait(controls::window::DuiDialog& dialog)
{
    MSG message{};
    while (dialog.Visible() && ::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    return dialog.Result();
}

} // namespace ysDui::platform::win32
