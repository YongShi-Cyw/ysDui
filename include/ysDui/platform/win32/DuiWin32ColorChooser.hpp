#pragma once

#include <memory>

#include "ysDui/ui/DuiColorChooser.hpp"

namespace ysDui::platform::win32 {

class Win32ColorChooser final : public ui::DuiColorChooser {
public:
    Win32ColorChooser(); ~Win32ColorChooser() override;
    Win32ColorChooser(const Win32ColorChooser&) = delete; Win32ColorChooser& operator=(const Win32ColorChooser&) = delete;
    Win32ColorChooser(Win32ColorChooser&&) noexcept; Win32ColorChooser& operator=(Win32ColorChooser&&) noexcept;
    [[nodiscard]] ui::DuiColorChooserResult Choose(core::Color initial) override;
private: class Impl; std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
