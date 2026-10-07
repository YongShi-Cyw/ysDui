/**
 * 文件名：DuiWin32DropTarget.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明 Win32 平台的拖放接收器。
 */
#pragma once

#include <memory>

#include "ysDui/ui/DuiDropTarget.hpp"
#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"

namespace ysDui::platform::win32 {

/**
 * Win32 拖放接收器。
 * 构造时向指定原生窗口注册，析构时自动注销；原生拖放类型不会离开实现文件。
 */
class Win32DropTarget final : public ui::DuiDropTarget
{
public:
    /**
     * 创建并注册拖放接收器。
     * @param host 用于接收拖放的原生宿主窗口。
     */
    explicit Win32DropTarget(NativeWindowHandle host);
    ~Win32DropTarget() override;
    Win32DropTarget(const Win32DropTarget&) = delete;
    Win32DropTarget& operator=(const Win32DropTarget&) = delete;
    Win32DropTarget(Win32DropTarget&&) noexcept;
    Win32DropTarget& operator=(Win32DropTarget&&) noexcept;

    void SetHandler(Handler handler) override;
    void SetEnabled(bool enabled) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
