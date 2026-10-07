#pragma once

#include <memory>
#include <optional>

#include "ysDui/ui/DuiClipboard.hpp"

namespace ysDui::platform::win32 {

/** Win32 文本剪贴板适配器。 */
class Win32Clipboard final : public ui::DuiClipboard
{
public:
    Win32Clipboard();
    ~Win32Clipboard() override;
    Win32Clipboard(const Win32Clipboard&) = delete;
    Win32Clipboard& operator=(const Win32Clipboard&) = delete;
    Win32Clipboard(Win32Clipboard&&) noexcept;
    Win32Clipboard& operator=(Win32Clipboard&&) noexcept;

    bool SetText(std::string text) override;
    [[nodiscard]] std::optional<std::string> GetText() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
