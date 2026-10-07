/**
 * 文件名：DuiWin32HostRefAdapter.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：实现外部 Win32 窗口到跨平台宿主引用的生命周期适配。
 */
#include "ysDui/platform/win32/DuiWin32HostRefAdapter.hpp"

#include <utility>

#include "DuiWin32HostRef.hpp"

namespace ysDui::platform::win32 {

class Win32HostRefAdapter::Impl
{
public:
    std::shared_ptr<detail::Win32HostBinding> binding = std::make_shared<detail::Win32HostBinding>();
};

Win32HostRefAdapter::Win32HostRefAdapter(NativeWindowHandle handle) : adapter_(std::make_unique<Impl>())
{
    adapter_->binding->SetHandle(reinterpret_cast<::HWND>(handle.value));
}

Win32HostRefAdapter::~Win32HostRefAdapter() = default;
Win32HostRefAdapter::Win32HostRefAdapter(Win32HostRefAdapter&&) noexcept = default;
Win32HostRefAdapter& Win32HostRefAdapter::operator=(Win32HostRefAdapter&&) noexcept = default;

ui::HostRef Win32HostRefAdapter::Reference() const
{
    return adapter_ ? detail::CreateHostRef(adapter_->binding) : ui::HostRef{};
}

} // namespace ysDui::platform::win32
