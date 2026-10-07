#include "ysDui/platform/win32/DuiWindow.hpp"
#include "DuiWin32Utf8.hpp"
#include "DuiWin32Canvas.hpp"
#include "DuiWin32EventConverter.hpp"
#include "DuiWin32PaintBuffer.hpp"
#include "DuiWin32UiaProvider.hpp"
#include "DuiWin32HostRef.hpp"

#include <windows.h>

#include <algorithm>
#include <string>
#include <utility>

namespace ysDui::platform::win32 {
namespace {
constexpr ::UINT_PTR AnimationTimerId = 0xD001;
constexpr ::UINT AnimationTimerIntervalMilliseconds = 16;

} // namespace
class Window::Impl {
public:
    explicit Impl(std::shared_ptr<core::Host> host)
        : host(std::move(host)), binding(std::make_shared<detail::Win32HostBinding>()) {}
    std::shared_ptr<core::Host> host;
    std::shared_ptr<detail::Win32HostBinding> binding;
    std::function<void(render::Canvas&, core::Rect)> paint;
    std::function<void(core::Size)> resized;
    ::HWND handle{};
    WindowOptions::Kind kind{WindowOptions::Kind::TopLevel};
    int dpi{core::DuiDpiScale::DefaultDpi};
    ::ULONGLONG animationTick{};
    std::function<void(NativeWindowHandle)> focusLost;
    std::function<void()> closed;
    Win32PaintBuffer paintBuffer;
    std::unique_ptr<render::Canvas> canvas;
    core::Size canvasBufferSize;
    int canvasDpi{};
    Win32EventConverter eventConverter;
    bool trackingMouse{};
    bool applyingDpiBounds{};
    bool releasingMouseCapture{};
    bool movingWindow{};
    bool deferredAnimationPaint{};
    /** false：指针移动不触发整窗重绘（分层窗由业务显式 SetBounds/RequestRepaint 刷新） */
    bool repaintOnPointerMove{true};
    /** 本实例是否正在跑消息循环（Run）：仅它销毁时才结束循环。 */
    bool messageLoopActive{};

    [[nodiscard]] core::DuiDpiScale Scale() const { return core::DuiDpiScale(dpi); }

    void UpdateDpi(int value)
    {
        dpi = value > 0 ? value : core::DuiDpiScale::DefaultDpi;
        eventConverter.SetDpiScale(Scale());
        if (host)
            host->SetDpiScale(Scale());
    }

    void NotifyResize() const
    {
        if (!resized || handle == nullptr)
            return;
        ::RECT client{};
        if (::GetClientRect(handle, &client))
            resized(Scale().Unscale(core::Size{client.right - client.left,
                                               client.bottom - client.top}));
    }

    void NotifyThemeChanged()
    {
        NotifyResize();
        if (handle != nullptr)
            ::InvalidateRect(handle, nullptr, FALSE);
    }

    void TrackMouseLeave()
    {
        if (trackingMouse || handle == nullptr)
            return;
        ::TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, handle, 0};
        trackingMouse = ::TrackMouseEvent(&tracking) != FALSE;
    }

    bool DispatchNative(::UINT message, ::WPARAM word, ::LPARAM data)
    {
        if (!host)
            return false;
        bool handled{};
        for (const core::Event& event : eventConverter.Convert(
            reinterpret_cast<std::uintptr_t>(handle), message,
            static_cast<std::uintptr_t>(word), static_cast<std::intptr_t>(data)))
        {
            handled = host->Dispatch(event) || handled;
        }
        return handled;
    }

    void SetAnimationTimer(bool active) {
        if (handle == nullptr) return;
        if (active) {
            animationTick = ::GetTickCount64();
            ::SetTimer(handle, AnimationTimerId, AnimationTimerIntervalMilliseconds, nullptr);
        } else {
            ::KillTimer(handle, AnimationTimerId);
        }
    }

    [[nodiscard]] ::HCURSOR CursorAt(core::Point point) const
    {
        core::DuiPointerCursor cursor = core::DuiPointerCursor::Arrow;
        if (host) {
            if (const core::Control* control = host->HitTest(point))
                cursor = control->PointerCursor();
        }
        switch (cursor) {
        case core::DuiPointerCursor::Hand:
            return ::LoadCursorW(nullptr, IDC_HAND);
        case core::DuiPointerCursor::ResizeHorizontal:
            return ::LoadCursorW(nullptr, IDC_SIZEWE);
        case core::DuiPointerCursor::ResizeVertical:
            return ::LoadCursorW(nullptr, IDC_SIZENS);
        case core::DuiPointerCursor::Move:
            return ::LoadCursorW(nullptr, IDC_SIZEALL);
        case core::DuiPointerCursor::ResizeDownRight:
            return ::LoadCursorW(nullptr, IDC_SIZENWSE);
        case core::DuiPointerCursor::IBeam:
            return ::LoadCursorW(nullptr, IDC_IBEAM);
        default:
            return ::LoadCursorW(nullptr, IDC_ARROW);
        }
    }

    static ::LRESULT CALLBACK Procedure(::HWND handle, ::UINT message, ::WPARAM word, ::LPARAM data) {
        auto* self = reinterpret_cast<Impl*>(::GetWindowLongPtrW(handle, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Impl*>(reinterpret_cast<::CREATESTRUCTW*>(data)->lpCreateParams);
            ::SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->handle = handle;
            self->binding->SetHandle(handle);
        }
        if (self == nullptr) { return ::DefWindowProcW(handle, message, word, data); }
        if (message == WM_GETDLGCODE) {
            // 子窗口（EmbeddedHost 嵌入宿主）声明需要全部键：宿主对话框（NX 等）
            // 询问时不得把 Tab / Enter / Esc / 方向键等当作对话框导航键抢走，
            // 否则在编辑面板里按快捷键会触发宿主对话框的动作（如确认导致窗口关闭）
            if ((::GetWindowLongPtrW(handle, GWL_STYLE) & WS_CHILD) != 0)
                return DLGC_WANTALLKEYS;
            return ::DefWindowProcW(handle, message, word, data);
        }
        if (message == WM_GETOBJECT) { return ReturnUiaProvider(handle, self->host, word, data); }
        if (message == WM_SETCURSOR && reinterpret_cast<::HWND>(word) == handle && LOWORD(data) == HTCLIENT) {
            ::POINT point{};
            if (::GetCursorPos(&point) && ::ScreenToClient(handle, &point))
                ::SetCursor(self->CursorAt(
                    self->Scale().Unscale(core::Point{point.x, point.y})));
            else
                ::SetCursor(::LoadCursorW(nullptr, IDC_ARROW));
            return TRUE;
        }
        if (message == WM_ERASEBKGND && self->paint) { return 1; }
        if (message == WM_PAINT) {
            if (self->host)
                self->host->PrepareFrame();
            ::PAINTSTRUCT state{};
            ::HDC context = ::BeginPaint(handle, &state);
            ::RECT client{};
            ::GetClientRect(handle, &client);
            const int width = client.right - client.left;
            const int height = client.bottom - client.top;
            const auto nativeContext = reinterpret_cast<std::uintptr_t>(context);
            if (self->paint && self->paintBuffer.Prepare(nativeContext, width, height)) {
                const core::Size bufferSize{width, height};
                if (!self->canvas || self->canvasBufferSize != bufferSize || self->canvasDpi != self->dpi)
                {
                    self->canvas = CreateWin32Canvas(self->paintBuffer.Context(), self->dpi);
                    self->canvasBufferSize = bufferSize;
                    self->canvasDpi = self->dpi;
                }
                const core::Rect dirtyPixels{state.rcPaint.left, state.rcPaint.top,
                                             state.rcPaint.right, state.rcPaint.bottom};
                const core::Rect dirty = self->Scale().Unscale(dirtyPixels);
                self->paintBuffer.Clear(dirtyPixels);
                self->canvas->PushClip(dirty);
                self->paint(*self->canvas, dirty);
                self->canvas->PopClip();
                self->paintBuffer.Present(nativeContext, dirtyPixels);
            } else if (self->paint) {
                auto canvas = CreateWin32Canvas(nativeContext, self->dpi);
                self->paint(*canvas, self->Scale().Unscale(core::Rect{
                    state.rcPaint.left, state.rcPaint.top,
                    state.rcPaint.right, state.rcPaint.bottom}));
            }
            ::EndPaint(handle, &state);
            return 0;
        }
        if (message == WM_DPICHANGED) {
            self->UpdateDpi(static_cast<int>(LOWORD(word)));
            (void)self->DispatchNative(message, word, data);
            const auto* suggested = reinterpret_cast<const ::RECT*>(data);
            if (suggested != nullptr)
            {
                self->applyingDpiBounds = true;
                ::SetWindowPos(handle, nullptr, suggested->left, suggested->top,
                               suggested->right - suggested->left,
                               suggested->bottom - suggested->top,
                               SWP_NOZORDER | SWP_NOACTIVATE);
                self->applyingDpiBounds = false;
            }
            self->NotifyResize();
            ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_SIZE) {
            if (word != SIZE_MINIMIZED && !self->applyingDpiBounds)
                self->NotifyResize();
            if (word != SIZE_MINIMIZED)
                ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_WINDOWPOSCHANGED) {
            const auto* windowPos = reinterpret_cast<const ::WINDOWPOS*>(data);
            const ::LRESULT result = ::DefWindowProcW(handle, message, word, data);
            // 窗口移动后强制整窗重绘，避免移出屏幕时 CopyBits 残影在回到可见区后仍保留。
            if (windowPos != nullptr
                && (windowPos->flags & SWP_NOMOVE) == 0
                && (windowPos->flags & SWP_NOREDRAW) == 0)
            {
                ::InvalidateRect(handle, nullptr, FALSE);
            }
            return result;
        }
        if (message == WM_DISPLAYCHANGE) {
            self->paintBuffer.Reset();
            self->canvas.reset();
            self->canvasBufferSize = {};
            self->canvasDpi = 0;
            ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_ENTERSIZEMOVE) {
            self->movingWindow = true;
            if (self->host) {
                self->host->Dispatch({core::EventType::PointerCancel});
                self->host->Dispatch({core::EventType::PointerLeave});
            }
            return 0;
        }
        if (message == WM_EXITSIZEMOVE) {
            self->movingWindow = false;
            if (self->deferredAnimationPaint) {
                self->deferredAnimationPaint = false;
                ::InvalidateRect(handle, nullptr, FALSE);
            }
            return 0;
        }
        if (message == WM_TIMER && word == AnimationTimerId) {
            if (self->host && self->host->Clock().HasScheduledTasks()) {
                const ::ULONGLONG now = ::GetTickCount64();
                const int elapsed = static_cast<int>((std::min)(now - self->animationTick, static_cast<::ULONGLONG>(1000)));
                self->animationTick = now;
                self->host->Clock().Advance(elapsed);
                if (self->movingWindow)
                    self->deferredAnimationPaint = true;
                else
                    ::InvalidateRect(handle, nullptr, FALSE);
            }
            return 0;
        }
        if (message == WM_MOUSEMOVE) {
            self->TrackMouseLeave();
            // 标题栏拖拽（SC_MOVE）期间不派发指针移动，避免内容区出现假悬停。
            if (!self->movingWindow)
                (void)self->DispatchNative(message, word, data);
            // 分层窗等内容自绘的场景：移动鼠标不重绘，避免每次移动都整窗合成（悬停高亮由业务显式请求）
            if (self->repaintOnPointerMove)
                ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_MOUSELEAVE) {
            self->trackingMouse = false;
            if (::GetCapture() != handle)
                (void)self->DispatchNative(message, word, data);
            ::SetCursor(::LoadCursorW(nullptr, IDC_ARROW));
            ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_LBUTTONDOWN) {
            if (self->DispatchNative(message, word, data)
                && self->host != nullptr
                && self->host->CapturedControl() != nullptr)
                ::SetCapture(handle);
            // 子窗口（EmbeddedHost 嵌入宿主）点击后需自行取得焦点，否则收不到 WM_KEYDOWN，
            // 面板内的快捷键（Delete / WASD）将完全无效
            if ((::GetWindowLongPtrW(handle, GWL_STYLE) & WS_CHILD) != 0
                && ::GetFocus() != handle)
                ::SetFocus(handle);
            ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_LBUTTONUP) {
            (void)self->DispatchNative(message, word, data);
            if (::GetCapture() == handle) {
                self->releasingMouseCapture = true;
                ::ReleaseCapture();
                self->releasingMouseCapture = false;
            }
            ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_CAPTURECHANGED) {
            if (!self->releasingMouseCapture)
                (void)self->DispatchNative(message, word, data);
            ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN
            || message == WM_RBUTTONUP || message == WM_MBUTTONDOWN
            || message == WM_MBUTTONUP || message == WM_MOUSEWHEEL)
        {
            // 中/右键首次按下时激活窗口，避免 NX 视口抢占中键（需先左键点面板才能响应）。
            if (message == WM_MBUTTONDOWN || message == WM_RBUTTONDOWN)
            {
                ::SetForegroundWindow(handle);
                ::SetFocus(handle);
            }
            (void)self->DispatchNative(message, word, data);
            ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_POINTERUPDATE || message == WM_POINTERDOWN
            || message == WM_POINTERUP || message == WM_POINTERLEAVE
            || message == WM_POINTERCAPTURECHANGED || message == WM_POINTERWHEEL)
        {
            (void)self->DispatchNative(message, word, data);
            ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN
            || message == WM_KEYUP || message == WM_SYSKEYUP
            || message == WM_CHAR || (message == WM_UNICHAR && word != UNICODE_NOCHAR)
            || message == WM_IME_STARTCOMPOSITION || message == WM_IME_COMPOSITION
            || message == WM_IME_ENDCOMPOSITION)
        {
            (void)self->DispatchNative(message, word, data);
            ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_UNICHAR && word == UNICODE_NOCHAR)
            return TRUE;
        if (message == WM_SETFOCUS) {
            (void)self->DispatchNative(message, word, data);
            return 0;
        }
        if (message == WM_KILLFOCUS) {
            (void)self->DispatchNative(message, word, data);
            if (self->focusLost) self->focusLost({static_cast<std::uintptr_t>(word)});
            return 0;
        }
        if (message == WM_CLOSE) { ::DestroyWindow(handle); return 0; }
        // 只有正在跑消息循环的那个窗口才结束循环：否则关闭一个次要窗口（如停靠管理器的
        // 悬浮窗）会连带把主窗口一起关掉——PostQuitMessage 是进程级的。
        if (message == WM_DESTROY) { if (self->messageLoopActive) ::PostQuitMessage(0); return 0; }
        if (message == WM_NCDESTROY) {
            const auto closed = self->closed;
            self->handle = nullptr;
            self->binding->SetHandle(nullptr);
            const ::LRESULT result = ::DefWindowProcW(handle, message, word, data);
            if (closed)
                closed();
            return result;
        }
        return ::DefWindowProcW(handle, message, word, data);
    }
};

Window::Window(std::shared_ptr<core::Host> host) : impl_(std::make_unique<Impl>(std::move(host))) {}
Window::~Window() { if (impl_) Close(); }
Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

bool Window::Create(const WindowOptions& options) {
    const ::HINSTANCE instance = ::GetModuleHandleW(nullptr);
    const ::WNDCLASSW definition{CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, Impl::Procedure, 0, 0, instance,
                                 nullptr, ::LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"ysDui.v2.Window"};
    ::RegisterClassW(&definition);
    impl_->kind = options.kind;
    impl_->repaintOnPointerMove = options.repaintOnPointerMove;
    const bool popup = options.kind == WindowOptions::Kind::Popup;
    const auto owner = reinterpret_cast<::HWND>(options.owner.value);
    const int initialDpi = owner != nullptr && ::IsWindow(owner)
        ? static_cast<int>(::GetDpiForWindow(owner))
        : static_cast<int>(::GetDpiForSystem());
    impl_->UpdateDpi(initialDpi);
    const core::DuiDpiScale scale = impl_->Scale();
    const core::Rect logicalBounds{options.position.x, options.position.y,
                                   options.position.x + options.size.width,
                                   options.position.y + options.size.height};
    const core::Rect pixelBounds = scale.Scale(logicalBounds);
    const std::wstring title = detail::Utf8ToWide(options.title);
    const ::DWORD popupStyle = WS_POPUP | WS_BORDER | WS_CLIPCHILDREN
        | (options.resizable ? WS_THICKFRAME : 0);
    const ::DWORD windowStyle = popup ? popupStyle : WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    impl_->handle = ::CreateWindowExW(popup ? WS_EX_TOOLWINDOW : 0, definition.lpszClassName,
        title.c_str(), windowStyle,
        popup ? pixelBounds.left : CW_USEDEFAULT, popup ? pixelBounds.top : CW_USEDEFAULT,
        pixelBounds.Width(), pixelBounds.Height(), owner, nullptr, instance, impl_.get());
    if (impl_->handle != nullptr) {
        impl_->binding->SetHandle(impl_->handle);
        // 分层宿主等需先合成再显示，避免默认背景闪一下
        if (options.initiallyVisible)
            ::ShowWindow(impl_->handle, SW_SHOW);
        impl_->UpdateDpi(static_cast<int>(::GetDpiForWindow(impl_->handle)));
        if (impl_->host) {
            auto* state = impl_.get();
            impl_->host->Clock().SetActivityChangedHandler([state](bool active) { state->SetAnimationTimer(active); });
            impl_->host->SetThemeChangedHandler([state] { state->NotifyThemeChanged(); });
            // 跨线程投递需要唤醒消息循环：否则后台任务要等到下一次输入事件才执行（界面表现为卡住不动）。
            // 该回调可能由投递线程触发，InvalidateRect 允许跨线程对同进程窗口调用。
            impl_->host->Dispatcher().SetPendingHandler([state]
            {
                if (state->handle != nullptr)
                    ::InvalidateRect(state->handle, nullptr, FALSE);
            });
        }
    }
    return impl_->handle != nullptr;
}

int Window::Run() {
    if (!impl_) return 0;
    // 标记本实例正在跑消息循环：仅它销毁时结束循环（WM_DESTROY 据此判断）
    impl_->messageLoopActive = true;
    ::MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    impl_->messageLoopActive = false;
    return static_cast<int>(message.wParam);
}
void Window::Close() {
    if (!impl_) return;
    if (impl_->host) {
        impl_->host->Clock().SetActivityChangedHandler({});
        impl_->host->SetThemeChangedHandler({});
        // 先解除唤醒回调（它捕获本 Impl，窗口销毁后不得再调用），再丢弃挂起任务
        impl_->host->Dispatcher().SetPendingHandler({});
        impl_->host->Dispatcher().Clear();
    }
    if (impl_->handle != nullptr) { ::KillTimer(impl_->handle, AnimationTimerId); ::DestroyWindow(impl_->handle); impl_->handle = nullptr; }
    impl_->binding->SetHandle(nullptr);
}
void Window::RequestClose() { if (impl_ && impl_->handle != nullptr) ::PostMessageW(impl_->handle, WM_CLOSE, 0, 0); }
NativeWindowHandle Window::NativeHandle() const {
    return {reinterpret_cast<std::uintptr_t>(impl_->handle)};
}
ui::HostRef Window::Reference() const { return detail::CreateHostRef(impl_->binding); }
core::DuiDpiScale Window::DpiScale() const { return core::DuiDpiScale(impl_->dpi); }
void Window::SetPaintHandler(std::function<void(render::Canvas&, core::Rect)> handler) { impl_->paint = std::move(handler); }
void Window::SetResizeHandler(std::function<void(core::Size)> handler) { impl_->resized = std::move(handler); }
void Window::SetFocusLostHandler(std::function<void(NativeWindowHandle)> handler) { impl_->focusLost = std::move(handler); }
void Window::SetClosedHandler(std::function<void()> handler) { impl_->closed = std::move(handler); }

} // namespace ysDui::platform::win32
