#pragma once

#include "ysDui/ui/DuiDateTimeSource.hpp"

namespace ysDui::platform::win32 {

class Win32DateTimeSource final : public ui::DuiDateTimeSource {
public:
    [[nodiscard]] core::DateTime LocalNow() const override;
};

} // namespace ysDui::platform::win32
