#pragma once

#include <memory>

#include "ysDui/ui/DuiPopupHost.hpp"

namespace ysDui::platform::win32 {

class Win32PopupHost final : public ui::IPopupHost {
public:
    explicit Win32PopupHost(ui::HostRef owner);
    ~Win32PopupHost() override;
    Win32PopupHost(const Win32PopupHost&) = delete;
    Win32PopupHost& operator=(const Win32PopupHost&) = delete;
    Win32PopupHost(Win32PopupHost&&) noexcept;
    Win32PopupHost& operator=(Win32PopupHost&&) noexcept;
    bool Show(const ui::DuiPopupOptions& options, std::unique_ptr<core::Control> content,
              LayoutHandler layout, PaintHandler paint) override;
    void SetOptions(const ui::DuiPopupOptions& options) override;
    void Hide() override;
    void RequestHide() override;
    void RequestMinimize() override;
    void RequestMove() override;
    [[nodiscard]] bool Visible() const override;
    [[nodiscard]] ui::HostRef Reference() const override;
    [[nodiscard]] std::uintptr_t NativeHandle() const override;
    void SetDismissedHandler(std::function<void()> handler) override;
    void SetAutoCollapse(bool enabled, int expandDelayMs, int collapseDelayMs, bool collapseNow,
                         std::function<void(bool)> toggle) override;
    void SetEdgeAutoHide(const ui::DuiEdgeAutoHideOptions& options) override;
    void TryDockEdgeNow() override;
    [[nodiscard]] bool EdgeDocked() const override;
    void SetEdgeHidePrepareHandler(std::function<void()> handler) override;
    void SetEdgeHiddenHandler(std::function<void()> handler) override;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
