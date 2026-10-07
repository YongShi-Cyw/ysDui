/**
 * 文件名：DuiWin32HostRef.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：实现 Win32 宿主引用的私有绑定与解析。
 */
#include "DuiWin32HostRef.hpp"

namespace ysDui::platform::win32::detail {

bool Win32HostBinding::Valid() const
{
    return handle_ != nullptr && ::IsWindow(handle_) != FALSE;
}

::HWND Win32HostBinding::Handle() const { return handle_; }
void Win32HostBinding::SetHandle(::HWND handle) { handle_ = handle; }

ui::HostRef CreateHostRef(const std::shared_ptr<Win32HostBinding>& binding)
{
    return ui::detail::HostRefAccess::Create(binding);
}

::HWND ResolveHost(const ui::HostRef& reference)
{
    const auto binding = std::dynamic_pointer_cast<const Win32HostBinding>(
        ui::detail::HostRefAccess::Lock(reference));
    return binding && binding->Valid() ? binding->Handle() : nullptr;
}

} // namespace ysDui::platform::win32::detail
