#include "ysDui/platform/win32/DuiWin32NativeViewHost.hpp"

#include <windows.h>

#include "DuiWin32NativeViewRef.hpp"
#include "ysDui/core/DuiDpi.hpp"

namespace ysDui::platform::win32 {
class Win32NativeViewHost::Impl {
public:
    ::HWND container{};
    ::HWND child{};
    ::LONG_PTR style{};
    ::LONG_PTR exStyle{};
    bool owns{};
    bool stripped{};
    core::Rect bounds;
    ui::NativeViewRef reference;
};
Win32NativeViewHost::Win32NativeViewHost(NativeWindowHandle parent) : impl_(std::make_unique<Impl>())
{
    impl_->container = ::CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 0, 0, reinterpret_cast<::HWND>(parent.value), nullptr, ::GetModuleHandleW(nullptr), nullptr);
}
Win32NativeViewHost::~Win32NativeViewHost()
{
    static_cast<void>(Detach());
    if (impl_->container && ::IsWindow(impl_->container)) ::DestroyWindow(impl_->container);
}
bool Win32NativeViewHost::Attach(ui::NativeViewRef child, ui::DuiNativeViewOptions options)
{
    if (!impl_->container || !::IsWindow(impl_->container)) return false;
    const ::HWND nativeChild = detail::ResolveNativeView(child);
    if (nativeChild == nullptr) return false;
    static_cast<void>(Detach());
    impl_->child = nativeChild;
    impl_->reference = std::move(child);
    impl_->owns = options.takeOwnership;
    impl_->stripped = options.stripDecorations;
    impl_->style = ::GetWindowLongPtrW(impl_->child, GWL_STYLE);
    impl_->exStyle = ::GetWindowLongPtrW(impl_->child, GWL_EXSTYLE);
    const auto applyStrippedStyle = [this]
    {
        const ::LONG_PTR strippedStyle =
            (impl_->style & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
                              | WS_SYSMENU | WS_POPUP | WS_OVERLAPPED)) | WS_CHILD;
        const ::LONG_PTR strippedExStyle =
            impl_->exStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE
                               | WS_EX_STATICEDGE | WS_EX_APPWINDOW | WS_EX_TOOLWINDOW);
        ::SetWindowLongPtrW(impl_->child, GWL_STYLE, strippedStyle);
        ::SetWindowLongPtrW(impl_->child, GWL_EXSTYLE, strippedExStyle);
        ::SetWindowPos(impl_->child, nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    };
    if (impl_->stripped)
        applyStrippedStyle();
    ::SetParent(impl_->child, impl_->container);
    if (impl_->stripped)
        applyStrippedStyle();
    SetBounds(impl_->bounds);
    ::ShowWindow(impl_->child, SW_SHOW);
    return true;
}
ui::NativeViewRef Win32NativeViewHost::Detach()
{
    ::HWND child = impl_->child;
    if (!child || !::IsWindow(child)) { impl_->child = nullptr; impl_->reference = {}; return {}; }
    if (impl_->stripped && !impl_->owns)
    {
        ::SetWindowLongPtrW(child, GWL_STYLE, impl_->style);
        ::SetWindowLongPtrW(child, GWL_EXSTYLE, impl_->exStyle);
        ::SetWindowPos(child, nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    ::SetParent(child, HWND_MESSAGE);
    impl_->child = nullptr;
    ui::NativeViewRef reference = std::move(impl_->reference);
    const bool owns = impl_->owns;
    impl_->owns = impl_->stripped = false;
    if (owns) { ::DestroyWindow(child); return {}; }
    return reference;
}
void Win32NativeViewHost::SetBounds(core::Rect bounds)
{
    impl_->bounds = bounds;
    const int dpi = impl_->container != nullptr ? static_cast<int>(::GetDpiForWindow(impl_->container))
                                                : core::DuiDpiScale::DefaultDpi;
    const core::Rect pixels = core::DuiDpiScale(dpi).Scale(bounds);
    if (impl_->container) ::SetWindowPos(impl_->container, nullptr, pixels.left, pixels.top, pixels.Width(), pixels.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
    if (impl_->child && ::IsWindow(impl_->child)) ::SetWindowPos(impl_->child, nullptr, 0, 0, pixels.Width(), pixels.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
}
void Win32NativeViewHost::SetVisible(bool visible) { if (impl_->container) ::ShowWindow(impl_->container, visible ? SW_SHOW : SW_HIDE); }
bool Win32NativeViewHost::HasContent() const { return impl_->child && ::IsWindow(impl_->child); }
} // namespace ysDui::platform::win32
