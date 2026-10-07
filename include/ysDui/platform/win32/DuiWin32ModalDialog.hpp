#pragma once

namespace ysDui::controls::window {
class DuiDialog;
}

namespace ysDui::platform::win32 {

class DuiWin32ModalDialog final {
public:
    [[nodiscard]] static int Wait(controls::window::DuiDialog& dialog);
};

} // namespace ysDui::platform::win32
