/**
 * 文件名：DuiWin32FrameHost.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明 Win32 顶层窗口宿主的私有实现。
 */
#pragma once

#include <memory>

#include "ysDui/ui/DuiFrameHost.hpp"

namespace ysDui::platform::win32 {

class Win32FrameHost final : public ui::IFrameHost
{
public:
    Win32FrameHost();
    ~Win32FrameHost() override;
    Win32FrameHost(const Win32FrameHost&) = delete;
    Win32FrameHost& operator=(const Win32FrameHost&) = delete;

    bool Show(const ui::DuiFrameOptions& options, std::unique_ptr<core::Control> content,
              LayoutHandler layout, PaintHandler paint) override;
    void SetOptions(const ui::DuiFrameOptions& options) override;
    void SetContent(std::unique_ptr<core::Control> content, LayoutHandler layout, PaintHandler paint) override;
    void Close() override;
    void RequestClose() override;
    [[nodiscard]] std::unique_ptr<core::Control> DetachContent() override;
    [[nodiscard]] bool Visible() const override;
    [[nodiscard]] ui::HostRef Reference() const override;
    void SetCaptionButtonHandler(std::function<void(int)> handler) override;
    void SetClosedHandler(std::function<void()> handler) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
