/**
 * 文件名：DuiWin32NativeViewRef.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：实现 Win32 原生窗口与跨平台原生视图引用的互操作。
 */
#include "DuiWin32NativeViewRef.hpp"

#include <memory>

namespace ysDui::platform::win32 {
namespace {

class Win32NativeViewBinding final : public ui::detail::NativeViewBinding
{
public:
    explicit Win32NativeViewBinding(::HWND handle) : handle_(handle) {}

    [[nodiscard]] bool Valid() const override
    {
        return handle_ != nullptr && ::IsWindow(handle_) != FALSE;
    }

    [[nodiscard]] ::HWND Handle() const { return handle_; }

private:
    ::HWND handle_{};
};

} // namespace

ui::NativeViewRef CreateNativeViewRef(NativeWindowHandle handle)
{
    const auto native = reinterpret_cast<::HWND>(handle.value);
    if (native == nullptr || ::IsWindow(native) == FALSE)
        return {};
    return ui::detail::NativeViewRefAccess::Create(
        std::make_shared<Win32NativeViewBinding>(native));
}

::HWND detail::ResolveNativeView(const ui::NativeViewRef& reference)
{
    const auto binding = std::dynamic_pointer_cast<const Win32NativeViewBinding>(
        ui::detail::NativeViewRefAccess::Binding(reference));
    return binding && binding->Valid() ? binding->Handle() : nullptr;
}

} // namespace ysDui::platform::win32
