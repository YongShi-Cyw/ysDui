/**
 * 文件名：DuiWin32HostRef.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明 Win32 宿主引用的私有绑定与解析接口。
 */
#pragma once

#include <windows.h>

#include <memory>

#include "DuiHostRefAccess.hpp"

namespace ysDui::platform::win32::detail {

class Win32HostBinding final : public ui::detail::HostBinding
{
public:
    [[nodiscard]] bool Valid() const override;
    [[nodiscard]] ::HWND Handle() const;
    void SetHandle(::HWND handle);

private:
    ::HWND handle_{};
};

[[nodiscard]] ui::HostRef CreateHostRef(
    const std::shared_ptr<Win32HostBinding>& binding);
[[nodiscard]] ::HWND ResolveHost(const ui::HostRef& reference);

} // namespace ysDui::platform::win32::detail
