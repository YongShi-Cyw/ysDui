#pragma once

#include <memory>

#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/ui/DuiNativeViewHost.hpp"

namespace ysDui::platform::win32 {

class Win32NativeViewHost final : public ui::DuiNativeViewHost {
public:
    explicit Win32NativeViewHost(NativeWindowHandle parent);
    ~Win32NativeViewHost() override;
    Win32NativeViewHost(const Win32NativeViewHost&) = delete;
    Win32NativeViewHost& operator=(const Win32NativeViewHost&) = delete;
    bool Attach(ui::NativeViewRef child, ui::DuiNativeViewOptions options) override;
    [[nodiscard]] ui::NativeViewRef Detach() override;
    void SetBounds(core::Rect bounds) override;
    void SetVisible(bool visible) override;
    [[nodiscard]] bool HasContent() const override;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
