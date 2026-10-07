/**
 * 文件名：DuiWin32HostRefAdapter.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：将外部 Win32 窗口的生命周期显式适配为跨平台宿主引用。
 */
#pragma once

#include <memory>

#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/ui/DuiHostRef.hpp"

namespace ysDui::platform::win32 {

/** 外部 Win32 窗口的宿主引用适配器；适配器销毁后已发出的引用自动失效。 */
class Win32HostRefAdapter final
{
public:
    /** @param handle 由调用方管理生命周期的 Win32 窗口句柄。 */
    explicit Win32HostRefAdapter(NativeWindowHandle handle);
    ~Win32HostRefAdapter();
    Win32HostRefAdapter(const Win32HostRefAdapter&) = delete;
    Win32HostRefAdapter& operator=(const Win32HostRefAdapter&) = delete;
    Win32HostRefAdapter(Win32HostRefAdapter&&) noexcept;
    Win32HostRefAdapter& operator=(Win32HostRefAdapter&&) noexcept;

    /** @return 不延长适配器或原生窗口生命周期的宿主弱引用。 */
    [[nodiscard]] ui::HostRef Reference() const;

private:
    class Impl;
    std::unique_ptr<Impl> adapter_;
};

} // namespace ysDui::platform::win32
