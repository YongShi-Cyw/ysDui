#include "ysDui/platform/win32/DuiWin32UiHostFactory.hpp"

#include <windows.h>
#include <commctrl.h>

#include <cstring>
#include <cstddef>

#include "DuiWin32Canvas.hpp"
#include "DuiWin32FrameHost.hpp"
#include "DuiWin32HostRef.hpp"
#include "ysDui/platform/win32/DuiWin32NativeViewHost.hpp"
#include "ysDui/platform/win32/DuiWin32PopupHost.hpp"
#include "ysDui/platform/win32/DuiWindow.hpp"
#include "ysDui/core/DuiDpi.hpp"

namespace ysDui::platform::win32 {
namespace {

::HWND LayeredOwnerRoot(const ui::HostRef& owner)
{
    auto window = detail::ResolveHost(owner);
    if (window == nullptr || !::IsWindow(window))
        return nullptr;
    if (const auto root = ::GetAncestor(window, GA_ROOT); root != nullptr)
        window = root;
    return window;
}

class LayeredSurface final {
public:
    ~LayeredSurface() { Reset(); }

    bool Ensure(core::Size size)
    {
        if (size.width <= 0 || size.height <= 0)
            return false;
        if (size_ == size && context_ != nullptr)
            return true;

        Reset();
        ::BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = size.width;
        info.bmiHeader.biHeight = -size.height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        ::HDC screen = ::GetDC(nullptr);
        context_ = ::CreateCompatibleDC(screen);
        bitmap_ = ::CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits_, nullptr, 0);
        if (screen != nullptr)
            ::ReleaseDC(nullptr, screen);
        if (context_ == nullptr || bitmap_ == nullptr || bits_ == nullptr) {
            Reset();
            return false;
        }
        previous_ = ::SelectObject(context_, bitmap_);
        size_ = size;
        return true;
    }

    void Clear() const
    {
        std::memset(bits_, 0, static_cast<std::size_t>(size_.width) * static_cast<std::size_t>(size_.height) * 4U);
    }

    [[nodiscard]] std::uintptr_t Context() const { return reinterpret_cast<std::uintptr_t>(context_); }

    bool Present(::HWND target, core::Point position, unsigned char opacity) const
    {
        ::POINT destination{position.x, position.y};
        ::POINT source{};
        ::SIZE size{size_.width, size_.height};
        ::BLENDFUNCTION blend{AC_SRC_OVER, 0, opacity, AC_SRC_ALPHA};
        ::HDC screen = ::GetDC(nullptr);
        const BOOL result = screen != nullptr && ::UpdateLayeredWindow(target, screen, &destination, &size,
                                                                         context_, &source, 0, &blend, ULW_ALPHA);
        if (screen != nullptr)
            ::ReleaseDC(nullptr, screen);
        return result != FALSE;
    }

    /** 修复 GDI/BitBlt 绘制未写入 alpha 的像素，保证分层窗口文字可见且可命中。 */
    void RepairAlphaForGdiDrawnPixels() const
    {
        if (bits_ == nullptr || size_.width <= 0 || size_.height <= 0)
            return;
        unsigned char* pixels = static_cast<unsigned char*>(bits_);
        const std::size_t pixelCount =
            static_cast<std::size_t>(size_.width) * static_cast<std::size_t>(size_.height);
        for (std::size_t index = 0; index < pixelCount; ++index)
        {
            unsigned char* pixel = pixels + index * 4U;
            if (pixel[3] != 0)
                continue;
            if ((pixel[0] | pixel[1] | pixel[2]) == 0)
                continue;
            pixel[3] = 255;
        }
    }

private:
    void Reset()
    {
        if (context_ != nullptr && previous_ != nullptr)
            ::SelectObject(context_, previous_);
        if (bitmap_ != nullptr)
            ::DeleteObject(bitmap_);
        if (context_ != nullptr)
            ::DeleteDC(context_);
        context_ = nullptr;
        bitmap_ = nullptr;
        previous_ = nullptr;
        bits_ = nullptr;
        size_ = {};
    }

    ::HDC context_{};
    ::HBITMAP bitmap_{};
    ::HGDIOBJ previous_{};
    void* bits_{};
    core::Size size_;
};

class Win32EmbeddedHost final : public ui::IEmbeddedHost {
public:
    explicit Win32EmbeddedHost(ui::HostRef anchor) : anchor_(std::move(anchor)) {}
    ~Win32EmbeddedHost() override { Hide(); }

    bool Show(const ui::DuiEmbeddedHostOptions& options, std::unique_ptr<core::Control> content,
              ui::IPopupHost::LayoutHandler layout, ui::IPopupHost::PaintHandler paint) override
    {
        const auto anchor = detail::ResolveHost(anchor_);
        if (anchor == nullptr || !::IsWindow(anchor) || !content || options.bounds.Empty())
            return false;

        Hide();
        options_ = options;
        child_ = (::GetWindowLongPtrW(anchor, GWL_STYLE) & WS_CHILD) != 0;
        host_ = std::make_shared<core::Host>();
        host_->SetRoot(std::move(content));
        if (layout)
            layout({0, 0, options.bounds.Width(), options.bounds.Height()});

        window_ = std::make_unique<Window>(host_);
        window_->SetPaintHandler([paint = std::move(paint)](render::Canvas& canvas, core::Rect dirty) {
            if (paint)
                paint(canvas, dirty);
        });
        if (options.dismissOnFocusLost) {
            Win32EmbeddedHost* state = this;
            window_->SetFocusLostHandler([state](NativeWindowHandle) {
                if (!state->visible_)
                    return;
                state->RequestHide();
            });
        }

        WindowOptions windowOptions;
        windowOptions.size = {options.bounds.Width(), options.bounds.Height()};
        windowOptions.position = Scale().Unscale(ScreenPosition(options.bounds));
        windowOptions.owner = {reinterpret_cast<std::uintptr_t>(anchor)};
        windowOptions.kind = WindowOptions::Kind::Popup;
        if (!window_->Create(windowOptions)) {
            window_.reset();
            host_.reset();
            return false;
        }

        const auto handle = reinterpret_cast<::HWND>(window_->NativeHandle().value);
        if (child_) {
            const ::LONG_PTR style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
            const ::LONG_PTR exStyle = (::GetWindowLongPtrW(handle, GWL_EXSTYLE) | WS_EX_NOPARENTNOTIFY) & ~WS_EX_TOOLWINDOW;
            ::SetWindowLongPtrW(handle, GWL_STYLE, style);
            ::SetWindowLongPtrW(handle, GWL_EXSTYLE, exStyle);
            ::SetParent(handle, anchor);
        }
        visible_ = true;
        UpdateWindowBounds();
        return true;
    }

    void Hide() override
    {
        visible_ = false;
        if (window_)
            window_->Close();
        window_.reset();
        host_.reset();
    }

    void RequestHide() override
    {
        visible_ = false;
        if (window_)
            window_->RequestClose();
    }

    void SetBounds(core::Rect bounds) override
    {
        options_.bounds = bounds;
        UpdateWindowBounds();
    }

    [[nodiscard]] bool Visible() const override { return visible_; }
    [[nodiscard]] ui::HostRef Reference() const override
    {
        return window_ ? window_->Reference() : ui::HostRef{};
    }

private:
    [[nodiscard]] core::DuiDpiScale Scale() const
    {
        if (const auto anchor = detail::ResolveHost(anchor_); anchor != nullptr)
            return core::DuiDpiScale(static_cast<int>(::GetDpiForWindow(anchor)));
        return core::DuiDpiScale();
    }

    [[nodiscard]] core::Point ScreenPosition(core::Rect bounds) const
    {
        const core::Rect pixels = Scale().Scale(bounds);
        if (child_)
            return {pixels.left, pixels.top};

        ::POINT point{pixels.left, pixels.top};
        const auto anchor = detail::ResolveHost(anchor_);
        if (anchor != nullptr)
            ::ClientToScreen(anchor, &point);
        return {point.x, point.y};
    }

    void UpdateWindowBounds()
    {
        if (!window_ || options_.bounds.Empty())
            return;

        const auto handle = reinterpret_cast<::HWND>(window_->NativeHandle().value);
        const core::Point position = ScreenPosition(options_.bounds);
        const core::Rect pixels = Scale().Scale(options_.bounds);
        ::SetWindowPos(handle, nullptr, position.x, position.y, pixels.Width(), pixels.Height(),
                       SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    ui::HostRef anchor_;
    ui::DuiEmbeddedHostOptions options_;
    std::shared_ptr<core::Host> host_;
    std::unique_ptr<Window> window_;
    bool child_{};
    bool visible_{};
};
class Win32LayeredHost final : public ui::ILayeredHost {
public:
    explicit Win32LayeredHost(ui::HostRef owner) : owner_(std::move(owner)) {}
    ~Win32LayeredHost() override { Hide(); }

    void StopFollowingOwner()
    {
        if (followedOwner_ != nullptr && ::IsWindow(followedOwner_))
        {
            ::RemoveWindowSubclass(followedOwner_, OwnerProcedure,
                                   reinterpret_cast<::UINT_PTR>(this));
        }
        followedOwner_ = nullptr;
        hasOwnerBounds_ = false;
    }

    void StopGeometryTracking()
    {
        if (geometryTrackedOwner_ != nullptr && ::IsWindow(geometryTrackedOwner_))
        {
            ::RemoveWindowSubclass(geometryTrackedOwner_, GeometryOwnerProcedure,
                                   GeometrySubclassId());
        }
        geometryTrackedOwner_ = nullptr;
    }

    [[nodiscard]] ::UINT_PTR GeometrySubclassId() const
    {
        return reinterpret_cast<::UINT_PTR>(this) + 1;
    }

    void UpdateGeometryTracking()
    {
        const ::HWND requestedOwner =
            ownerGeometryHandler_ ? detail::ResolveHost(owner_) : nullptr;
        if (requestedOwner == geometryTrackedOwner_)
            return;
        StopGeometryTracking();
        if (requestedOwner == nullptr)
            return;

        if (!::SetWindowSubclass(requestedOwner, GeometryOwnerProcedure, GeometrySubclassId(),
                                 reinterpret_cast<::DWORD_PTR>(this)))
        {
            return;
        }
        geometryTrackedOwner_ = requestedOwner;
    }

    [[nodiscard]] bool InvokeOwnerGeometryHandler()
    {
        if (!ownerGeometryHandler_)
            return false;
        return ownerGeometryHandler_();
    }

    void UpdateOwnerTracking()
    {
        const ::HWND requestedOwner = options_.followOwner ? LayeredOwnerRoot(owner_) : nullptr;
        if (requestedOwner == followedOwner_)
            return;
        StopFollowingOwner();
        if (requestedOwner == nullptr)
            return;

        ::RECT bounds{};
        if (!::GetWindowRect(requestedOwner, &bounds)
            || !::SetWindowSubclass(requestedOwner, OwnerProcedure,
                                    reinterpret_cast<::UINT_PTR>(this),
                                    reinterpret_cast<::DWORD_PTR>(this)))
        {
            return;
        }
        followedOwner_ = requestedOwner;
        ownerBounds_ = bounds;
        hasOwnerBounds_ = true;
    }

    void FollowOwnerMove(::HWND handle)
    {
        if (!visible_ || handle != followedOwner_ || ::IsIconic(handle) || window_ == nullptr)
            return;

        ::RECT nextOwnerBounds{};
        if (!::GetWindowRect(handle, &nextOwnerBounds))
            return;
        if (!hasOwnerBounds_)
        {
            ownerBounds_ = nextOwnerBounds;
            hasOwnerBounds_ = true;
            return;
        }

        const int prevWidth = ownerBounds_.right - ownerBounds_.left;
        const int prevHeight = ownerBounds_.bottom - ownerBounds_.top;
        const int nextWidth = nextOwnerBounds.right - nextOwnerBounds.left;
        const int nextHeight = nextOwnerBounds.bottom - nextOwnerBounds.top;
        const int deltaX = nextOwnerBounds.left - ownerBounds_.left;
        const int deltaY = nextOwnerBounds.top - ownerBounds_.top;
        const bool sizeChanged = prevWidth != nextWidth || prevHeight != nextHeight;
        ownerBounds_ = nextOwnerBounds;

        // 纯平移：只 SetWindowPos，避免业务重算布局 / UpdateLayeredWindow 重绘导致拖动卡顿
        if (!sizeChanged)
        {
            if (deltaX == 0 && deltaY == 0)
                return;
            const auto popup = reinterpret_cast<::HWND>(window_->NativeHandle().value);
            ::RECT popupBounds{};
            if (popup != nullptr && ::GetWindowRect(popup, &popupBounds))
            {
                ::SetWindowPos(popup, nullptr, popupBounds.left + deltaX, popupBounds.top + deltaY, 0, 0,
                               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
            }
            return;
        }

        // 尺寸变化：优先交给业务重算贴边；未处理时不可再用旧尺寸的 delta 硬平移
        if (InvokeOwnerGeometryHandler())
            return;
    }

    static ::LRESULT CALLBACK OwnerProcedure(::HWND handle, ::UINT message, ::WPARAM word,
                                             ::LPARAM data, ::UINT_PTR, ::DWORD_PTR reference)
    {
        auto* state = reinterpret_cast<Win32LayeredHost*>(reference);
        if (message == WM_WINDOWPOSCHANGED)
            state->FollowOwnerMove(handle);
        if (message == WM_NCDESTROY)
        {
            state->followedOwner_ = nullptr;
            state->hasOwnerBounds_ = false;
            ::RemoveWindowSubclass(handle, OwnerProcedure,
                                   reinterpret_cast<::UINT_PTR>(state));
        }
        return ::DefSubclassProc(handle, message, word, data);
    }

    static ::LRESULT CALLBACK GeometryOwnerProcedure(::HWND handle, ::UINT message, ::WPARAM word,
                                                   ::LPARAM data, ::UINT_PTR, ::DWORD_PTR reference)
    {
        auto* state = reinterpret_cast<Win32LayeredHost*>(reference);
        // 仅客户区/窗口尺寸变化需要业务重算；平移由 followOwner 处理，避免拖动时重复 Compose
        bool sizeMessage = false;
        if (message == WM_SIZE)
        {
            sizeMessage = true;
        }
        else if (message == WM_WINDOWPOSCHANGED)
        {
            const auto* windowPos = reinterpret_cast<const ::WINDOWPOS*>(data);
            sizeMessage = windowPos != nullptr && (windowPos->flags & SWP_NOSIZE) == 0;
        }
        if (sizeMessage)
            (void)state->InvokeOwnerGeometryHandler();
        if (message == WM_NCDESTROY)
        {
            if (state->geometryTrackedOwner_ == handle)
                state->geometryTrackedOwner_ = nullptr;
            ::RemoveWindowSubclass(handle, GeometryOwnerProcedure, state->GeometrySubclassId());
        }
        return ::DefSubclassProc(handle, message, word, data);
    }

    bool Show(const ui::DuiLayeredOptions& options, std::unique_ptr<core::Control> content,
              ui::IPopupHost::LayoutHandler layout, ui::IPopupHost::PaintHandler paint) override
    {
        if (!content || options.bounds.Empty()) return false;
        Hide();
        options_ = options;
        host_ = std::make_shared<core::Host>();
        host_->SetRoot(std::move(content));
        if (layout) layout({0, 0, options.bounds.Width(), options.bounds.Height()});
        window_ = std::make_unique<Window>(host_);
        paint_ = std::move(paint);
        window_->SetPaintHandler([this](render::Canvas&, core::Rect) { Compose(); });
        Win32LayeredHost* state = this;
        window_->SetClosedHandler([state] {
            state->visible_ = false;
            state->StopFollowingOwner();
            state->window_.reset();
            state->host_.reset();
            state->paint_ = {};
            if (state->dismissed_)
                state->dismissed_();
        });
        if (options.dismissOnFocusLost) {
            window_->SetFocusLostHandler([state](NativeWindowHandle) {
                if (!state->visible_) return;
                state->visible_ = false;
                if (state->dismissed_) state->dismissed_();
                if (state->window_) state->window_->RequestClose();
            });
        }
        WindowOptions windowOptions;
        windowOptions.size = {options.bounds.Width(), options.bounds.Height()};
        windowOptions.position = Scale().Unscale(ScreenPosition());
        windowOptions.owner = {reinterpret_cast<std::uintptr_t>(detail::ResolveHost(owner_))};
        windowOptions.kind = WindowOptions::Kind::Popup;
        // 分层窗内容由 UpdateLayeredWindow 呈现；指针移动不重绘，改由业务显式请求
        windowOptions.repaintOnPointerMove = options.repaintOnPointerMove;
        // 先创建隐藏窗口，完成分层合成后再显示，避免不透明背景闪屏
        windowOptions.initiallyVisible = false;
        if (!window_->Create(windowOptions)) { window_.reset(); host_.reset(); return false; }
        const auto handle = reinterpret_cast<::HWND>(window_->NativeHandle().value);
        ::SetWindowLongPtrW(handle, GWL_STYLE, ::GetWindowLongPtrW(handle, GWL_STYLE) & ~WS_BORDER);
        // WS_EX_TRANSPARENT：命中测试透明，提示条不遮挡 owner 的鼠标悬停
        ::LONG_PTR exStyle = ::GetWindowLongPtrW(handle, GWL_EXSTYLE) | WS_EX_LAYERED;
        if (options.mouseTransparent)
            exStyle |= WS_EX_TRANSPARENT;
        ::SetWindowLongPtrW(handle, GWL_EXSTYLE, exStyle);
        const core::Point position = ScreenPosition();
        const core::Size size = Scale().Scale(
            core::Size{options.bounds.Width(), options.bounds.Height()});
        ::SetWindowPos(handle, nullptr, position.x, position.y, size.width, size.height,
                       SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        visible_ = true;
        UpdateOwnerTracking();
        UpdateGeometryTracking();
        if (!Compose()) {
            Hide();
            return false;
        }
        ::ShowWindow(handle, SW_SHOWNOACTIVATE);
        return true;
    }
    void Hide() override
    {
        visible_ = false;
        StopFollowingOwner();
        StopGeometryTracking();
        if (window_)
        {
            window_->SetClosedHandler({});
            window_->Close();
            window_.reset();
        }
        host_.reset();
        paint_ = {};
    }
    void RequestHide() override { visible_ = false; if (window_) window_->RequestClose(); }
    [[nodiscard]] bool Visible() const override { return visible_; }
    [[nodiscard]] ui::HostRef Reference() const override
    {
        return window_ ? window_->Reference() : ui::HostRef{};
    }
    void SetBounds(core::Rect bounds) override
    {
        if (!visible_ || !window_ || bounds.Empty())
            return;
        options_.bounds = bounds;
        if (host_ != nullptr && host_->Root() != nullptr)
            host_->Root()->SetBounds({0, 0, bounds.Width(), bounds.Height()});
        const auto handle = reinterpret_cast<::HWND>(window_->NativeHandle().value);
        const core::Point position = ScreenPosition();
        const core::Size size = Scale().Scale(core::Size{bounds.Width(), bounds.Height()});
        ::SetWindowPos(handle, nullptr, position.x, position.y, size.width, size.height,
                       SWP_NOZORDER | SWP_NOACTIVATE);
        Compose();
    }

    void RequestRepaint() override
    {
        if (!visible_ || !window_ || options_.bounds.Empty())
            return;
        Compose();
    }

    void SetDismissedHandler(std::function<void()> handler) override { dismissed_ = std::move(handler); }
    void Activate() override
    {
        if (!visible_ || !window_)
            return;
        const auto handle = reinterpret_cast<::HWND>(window_->NativeHandle().value);
        if (handle == nullptr || !::IsWindow(handle))
            return;

        // 从 NX 主窗抢回前台：必要时 AttachThreadInput
        const HWND foreground = ::GetForegroundWindow();
        const DWORD panelThread = ::GetWindowThreadProcessId(handle, nullptr);
        const DWORD foregroundThread =
            foreground != nullptr ? ::GetWindowThreadProcessId(foreground, nullptr) : 0;
        const bool attached =
            foregroundThread != 0 && foregroundThread != panelThread
            && ::AttachThreadInput(foregroundThread, panelThread, TRUE) != FALSE;

        ::SetWindowPos(handle, HWND_TOP, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        ::BringWindowToTop(handle);
        ::SetForegroundWindow(handle);
        ::SetFocus(handle);

        if (attached)
            ::AttachThreadInput(foregroundThread, panelThread, FALSE);
    }
    void SetOwnerGeometryHandler(std::function<bool()> handler) override
    {
        ownerGeometryHandler_ = std::move(handler);
        if (visible_)
            UpdateGeometryTracking();
    }
private:
    [[nodiscard]] core::DuiDpiScale Scale() const
    {
        if (window_)
            return window_->DpiScale();
        if (const auto owner = detail::ResolveHost(owner_); owner != nullptr)
            return core::DuiDpiScale(static_cast<int>(::GetDpiForWindow(owner)));
        return core::DuiDpiScale(static_cast<int>(::GetDpiForSystem()));
    }

    [[nodiscard]] core::Point ScreenPosition() const
    {
        const core::Point pixels = Scale().Scale(
            core::Point{options_.bounds.left, options_.bounds.top});
        ::POINT point{pixels.x, pixels.y};
        if (const auto owner = detail::ResolveHost(owner_); owner != nullptr)
            ::ClientToScreen(owner, &point);
        return {point.x, point.y};
    }

    bool Compose()
    {
        const core::DuiDpiScale scale = Scale();
        const core::Size pixels = scale.Scale(
            core::Size{options_.bounds.Width(), options_.bounds.Height()});
        if (!window_ || !surface_.Ensure(pixels))
            return false;
        surface_.Clear();
        auto canvas = CreateWin32Canvas(surface_.Context(), scale.Dpi());
        if (paint_)
            paint_(*canvas, {0, 0, options_.bounds.Width(), options_.bounds.Height()});
        canvas.reset();
        surface_.RepairAlphaForGdiDrawnPixels();
        return surface_.Present(reinterpret_cast<::HWND>(window_->NativeHandle().value), ScreenPosition(), options_.opacity);
    }

    ui::HostRef owner_;
    ui::DuiLayeredOptions options_;
    std::shared_ptr<core::Host> host_;
    std::unique_ptr<Window> window_;
    ui::IPopupHost::PaintHandler paint_;
    LayeredSurface surface_;
    std::function<void()> dismissed_;
    std::function<bool()> ownerGeometryHandler_;
    ::HWND followedOwner_{};
    ::HWND geometryTrackedOwner_{};
    ::RECT ownerBounds_{};
    bool hasOwnerBounds_{};
    bool visible_{};
};
}

std::unique_ptr<ui::IEmbeddedHost> Win32UiHostFactory::CreateEmbeddedHost(ui::HostRef parent)
{
    return std::make_unique<Win32EmbeddedHost>(std::move(parent));
}

std::unique_ptr<ui::IFrameHost> Win32UiHostFactory::CreateFrameHost()
{
    return std::make_unique<Win32FrameHost>();
}

std::unique_ptr<ui::DuiNativeViewHost> Win32UiHostFactory::CreateNativeViewHost(ui::HostRef parent)
{
    const auto handle = detail::ResolveHost(parent);
    if (handle == nullptr)
        return {};
    return std::make_unique<Win32NativeViewHost>(
        NativeWindowHandle{reinterpret_cast<std::uintptr_t>(handle)});
}
std::unique_ptr<ui::IPopupHost> Win32UiHostFactory::CreatePopupHost(ui::HostRef owner)
{
    return std::make_unique<Win32PopupHost>(std::move(owner));
}
std::unique_ptr<ui::ILayeredHost> Win32UiHostFactory::CreateLayeredHost(ui::HostRef owner)
{
    return std::make_unique<Win32LayeredHost>(std::move(owner));
}
} // namespace ysDui::platform::win32
