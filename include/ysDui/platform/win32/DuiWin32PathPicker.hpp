#pragma once

#include <memory>

#include "ysDui/ui/DuiPathPicker.hpp"

namespace ysDui::platform::win32 {

class Win32PathPicker final : public ui::DuiPathPicker {
public:
    Win32PathPicker();
    ~Win32PathPicker() override;
    Win32PathPicker(const Win32PathPicker&) = delete;
    Win32PathPicker& operator=(const Win32PathPicker&) = delete;
    Win32PathPicker(Win32PathPicker&&) noexcept;
    Win32PathPicker& operator=(Win32PathPicker&&) noexcept;
    [[nodiscard]] ui::DuiPathPickerResult Browse(ui::DuiPathPickerMode mode,
                                               std::string_view initialPath) override;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
