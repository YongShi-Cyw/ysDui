#include "ysDui/platform/win32/DuiWin32TextInput.hpp"
#include "DuiWin32Utf8.hpp"
#include "DuiWin32EditContextMenu.hpp"
#include "ysDui/core/DuiDpi.hpp"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <string_view>

namespace ysDui::platform::win32 {
namespace {
constexpr int InputPointSize = 10;
constexpr int SingleLineVerticalPadding = 4;
constexpr int SingleLineVerticalOffset = 1;

::HFONT CreateInputFont(::HWND parent)
{
    int dpi = 96;
    if (::HDC context = ::GetDC(parent); context != nullptr)
    {
        dpi = ::GetDeviceCaps(context, LOGPIXELSY);
        ::ReleaseDC(parent, context);
    }
    ::LOGFONTW font{};
    font.lfHeight = -::MulDiv(InputPointSize, dpi, 72);
    font.lfQuality = CLEARTYPE_QUALITY;
    constexpr std::wstring_view fontFamily = L"Segoe UI";
    std::copy(fontFamily.begin(), fontFamily.end(), font.lfFaceName);
    return ::CreateFontIndirectW(&font);
}
}

class Win32TextInput::Impl {
public:
    ::HWND parent{};
    ::HWND handle{};
    ::HFONT font{};
    core::Rect bounds;
    ui::DuiTextInputOptions options;
    std::function<void()> changed;
    std::function<void()> focusLost;
    std::function<void()> submitted;
    std::function<void()> cancelled;
    bool visible{};
    int selectionAnchor{};
    bool selecting{};
    ::ULONGLONG caretEpoch{};

    [[nodiscard]] ::UINT_PTR CaretTimerId() const noexcept
    {
        return reinterpret_cast<::UINT_PTR>(this);
    }

    void StopCaretBlink() const
    {
        if (handle != nullptr)
            ::KillTimer(handle, CaretTimerId());
    }

    void RestartCaretBlink()
    {
        caretEpoch = ::GetTickCount64();
        StopCaretBlink();
        if (handle == nullptr || !visible || ::GetFocus() != handle)
            return;
        const ::UINT interval = ::GetCaretBlinkTime();
        if (interval != INFINITE && interval > 0)
            (void)::SetTimer(handle, CaretTimerId(), interval, nullptr);
    }

    [[nodiscard]] bool CaretVisible() const
    {
        if (handle == nullptr || !visible || ::GetFocus() != handle)
            return false;
        const ::UINT interval = ::GetCaretBlinkTime();
        if (interval == INFINITE || interval == 0)
            return true;
        return ((::GetTickCount64() - caretEpoch) / interval) % 2U == 0U;
    }

    /** 将平台无关的控件坐标换算为原生输入窗口的客户区像素坐标。 */
    [[nodiscard]] ::POINT NativePointFrom(core::Point position) const
    {
        if (handle == nullptr)
            return {0, 0};
        const int logicalX = std::clamp(position.x - bounds.left, 0,
                                        (std::max)(0, bounds.Width() - 1));
        const int logicalY = options.multiline
            ? std::clamp(position.y - bounds.top, 0, (std::max)(0, bounds.Height() - 1))
            : 0;
        return {std::clamp(Scale().Scale(logicalX), 0, 32767),
                std::clamp(Scale().Scale(logicalY), 0, 32767)};
    }

    [[nodiscard]] int NativeIndexFromPoint(core::Point position) const
    {
        if (handle == nullptr)
            return 0;
        const ::POINT point = NativePointFrom(position);
        const ::LRESULT hit = ::SendMessageW(
            handle, EM_CHARFROMPOS, 0,
            MAKELPARAM(static_cast<::WORD>(point.x), static_cast<::WORD>(point.y)));
        return static_cast<int>(LOWORD(hit));
    }

    [[nodiscard]] ui::DuiTextSelection Utf8Selection() const
    {
        if (handle == nullptr)
            return {};
        DWORD selectionStart{};
        DWORD selectionEnd{};
        ::SendMessageW(handle, EM_GETSEL, reinterpret_cast<::WPARAM>(&selectionStart),
                       reinterpret_cast<::LPARAM>(&selectionEnd));
        const int length = ::GetWindowTextLengthW(handle);
        std::wstring nativeText(static_cast<std::size_t>(length) + 1, L'\0');
        ::GetWindowTextW(handle, nativeText.data(), length + 1);
        nativeText.resize(static_cast<std::size_t>(length));
        const auto utf8Offset = [&nativeText](DWORD position)
        {
            const std::size_t count = (std::min)(static_cast<std::size_t>(position),
                                                  nativeText.size());
            return detail::WideToUtf8(std::wstring_view(nativeText).substr(0, count)).size();
        };
        const std::size_t start = utf8Offset(selectionStart);
        const std::size_t end = utf8Offset(selectionEnd);
        return {(std::min)(start, end), (std::max)(start, end)};
    }

    [[nodiscard]] core::DuiDpiScale Scale() const
    {
        const int dpi = parent != nullptr ? static_cast<int>(::GetDpiForWindow(parent))
                                          : core::DuiDpiScale::DefaultDpi;
        return core::DuiDpiScale(dpi);
    }

    [[nodiscard]] int PreferredSingleLineHeight() const
    {
        if (handle == nullptr)
            return bounds.Height();
        ::HDC context = ::GetDC(handle);
        if (context == nullptr)
            return bounds.Height();
        const ::HGDIOBJ previous = font != nullptr ? ::SelectObject(context, font) : nullptr;
        ::TEXTMETRICW metrics{};
        const bool measured = ::GetTextMetricsW(context, &metrics) != FALSE;
        if (previous != nullptr)
            ::SelectObject(context, previous);
        ::ReleaseDC(handle, context);
        return measured ? Scale().Unscale(metrics.tmHeight + metrics.tmExternalLeading)
                              + SingleLineVerticalPadding
                        : bounds.Height();
    }

    void ApplyBounds() const
    {
        if (handle == nullptr)
            return;
        core::Rect nativeBounds = bounds;
        if (!options.multiline && bounds.Height() > 0)
        {
            const int height = (std::min)(bounds.Height(), PreferredSingleLineHeight());
            nativeBounds.top = bounds.top + (bounds.Height() - height + 1) / 2
                + SingleLineVerticalOffset;
            nativeBounds.top = (std::min)(nativeBounds.top, bounds.bottom - height);
            nativeBounds.bottom = nativeBounds.top + height;
        }
        nativeBounds = Scale().Scale(nativeBounds);
        ::SetWindowPos(handle, nullptr, nativeBounds.left, nativeBounds.top,
                       (std::max)(0, nativeBounds.Width()), (std::max)(0, nativeBounds.Height()),
                       SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
    }

    /**
     * @return 本次 Enter 是否应触发提交而非插入换行。
     * 单行输入始终提交（既有行为）；多行输入仅在 submitOnEnter 开启且未按 Shift 时提交，
     * 于是 Shift+Enter 保留为换行——这是聊天输入框的通行约定。
     */
    [[nodiscard]] static bool SubmitsOnReturn(const Impl& state)
    {
        if (!state.options.multiline)
            return true;
        return state.options.submitOnEnter && (::GetKeyState(VK_SHIFT) & 0x8000) == 0;
    }

    static ::LRESULT CALLBACK ParentProcedure(::HWND parent, ::UINT message, ::WPARAM word,
                                                ::LPARAM data, ::UINT_PTR, ::DWORD_PTR reference) {
        auto* state = reinterpret_cast<Impl*>(reference);
        if (message == WM_COMMAND && reinterpret_cast<::HWND>(data) == state->handle &&
            HIWORD(word) == EN_CHANGE && state->changed) {
            const ::HWND redrawParent = state->parent;
            const auto changed = state->changed;
            changed();
            if (::IsWindow(redrawParent))
                ::InvalidateRect(redrawParent, nullptr, FALSE);
        }
        if ((message == WM_CTLCOLOREDIT || message == WM_CTLCOLORSTATIC)
            && reinterpret_cast<::HWND>(data) == state->handle) {
            auto context = reinterpret_cast<::HDC>(word);
            const bool enabled = ::IsWindowEnabled(state->handle) != FALSE;
            const ::COLORREF background = enabled ? RGB(255, 255, 255) : ::GetSysColor(COLOR_3DFACE);
            ::SetBkColor(context, background);
            ::SetTextColor(context, enabled ? RGB(20, 20, 20) : ::GetSysColor(COLOR_GRAYTEXT));
            return reinterpret_cast<::LRESULT>(enabled ? ::GetStockObject(WHITE_BRUSH)
                                                       : ::GetSysColorBrush(COLOR_3DFACE));
        }
        return ::DefSubclassProc(parent, message, word, data);
    }

    static ::LRESULT CALLBACK Procedure(::HWND handle, ::UINT message, ::WPARAM word, ::LPARAM data, ::UINT_PTR, ::DWORD_PTR reference) {
        auto* state = reinterpret_cast<Impl*>(reference);
        const ::HWND parent = state->parent;
        if (message == WM_KEYDOWN && word == VK_RETURN && SubmitsOnReturn(*state))
        {
            const auto submitted = state->submitted;
            if (submitted)
                submitted();
            return 0;
        }
        if (message == WM_CHAR && word == L'\r' && SubmitsOnReturn(*state))
            return 0;
        if (message == WM_KEYDOWN && word == VK_ESCAPE && state->cancelled)
        {
            const auto cancelled = state->cancelled;
            cancelled();
            return 0;
        }
        if (message == WM_CHAR && word == 27 && state->cancelled)
            return 0;
        if (message == WM_TIMER && word == state->CaretTimerId())
        {
            if (::IsWindow(parent))
            {
                const core::Rect pixels = state->Scale().Scale(state->bounds);
                const ::RECT dirty{pixels.left, pixels.top, pixels.right, pixels.bottom};
                ::InvalidateRect(parent, &dirty, FALSE);
            }
            return 0;
        }
        if (message == WM_KILLFOCUS)
            state->StopCaretBlink();
        else if ((message == WM_KEYDOWN || message == WM_CHAR || message == WM_LBUTTONDOWN
                  || message == EM_SETSEL || message == WM_IME_STARTCOMPOSITION
                  || message == WM_IME_COMPOSITION || message == WM_IME_ENDCOMPOSITION)
                 && ::GetFocus() == handle)
        {
            state->RestartCaretBlink();
        }
        if (message == WM_KILLFOCUS && state->focusLost)
        {
            const auto focusLost = state->focusLost;
            focusLost();
        }
        if (message == WM_CONTEXTMENU) {
            DWORD selectionStart{};
            DWORD selectionEnd{};
            ::SendMessageW(handle, EM_GETSEL, reinterpret_cast<::WPARAM>(&selectionStart), reinterpret_cast<::LPARAM>(&selectionEnd));
            controls::input::DuiEditContextState menuState;
            menuState.readOnly = state->options.readOnly;
            menuState.password = state->options.password;
            menuState.hasSelection = selectionStart != selectionEnd;
            menuState.hasText = ::GetWindowTextLengthW(handle) > 0;
            menuState.clipboardHasText = ::IsClipboardFormatAvailable(CF_UNICODETEXT) || ::IsClipboardFormatAvailable(CF_TEXT);
            const ::POINT point{static_cast<int>(static_cast<short>(LOWORD(data))), static_cast<int>(static_cast<short>(HIWORD(data)))};
            if (ShowWin32EditContextMenu(handle, point, menuState))
                return 0;
        }
        const ::LRESULT result = ::DefSubclassProc(handle, message, word, data);
        if (message == WM_SETFOCUS)
            state->RestartCaretBlink();
        if ((message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_KEYUP
             || message == WM_IME_STARTCOMPOSITION || message == WM_IME_COMPOSITION
             || message == WM_IME_ENDCOMPOSITION) && ::IsWindow(parent))
        {
            ::InvalidateRect(parent, nullptr, FALSE);
        }
        return result;
    }
};
Win32TextInput::Win32TextInput(NativeWindowHandle parent) : impl_(std::make_unique<Impl>()) {
    impl_->parent = reinterpret_cast<::HWND>(parent.value);
    impl_->handle = ::CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0,
                                     impl_->parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (impl_->handle) {
        impl_->font = CreateInputFont(impl_->parent);
        ::SendMessageW(impl_->handle, WM_SETFONT,
                       reinterpret_cast<::WPARAM>(impl_->font != nullptr
                           ? impl_->font : ::GetStockObject(DEFAULT_GUI_FONT)), FALSE);
        ::SetWindowSubclass(impl_->handle, Impl::Procedure, 1, reinterpret_cast<::DWORD_PTR>(impl_.get()));
        ::SetWindowSubclass(impl_->parent, Impl::ParentProcedure, reinterpret_cast<::UINT_PTR>(impl_.get()),
                            reinterpret_cast<::DWORD_PTR>(impl_.get()));
    }
}
Win32TextInput::~Win32TextInput() {
    impl_->StopCaretBlink();
    if (impl_->parent && ::IsWindow(impl_->parent)) {
        ::RemoveWindowSubclass(impl_->parent, Impl::ParentProcedure, reinterpret_cast<::UINT_PTR>(impl_.get()));
    }
    if (impl_->handle && ::IsWindow(impl_->handle)) ::DestroyWindow(impl_->handle);
    if (impl_->font != nullptr) ::DeleteObject(impl_->font);
}
Win32TextInput::Win32TextInput(Win32TextInput&&) noexcept = default;
Win32TextInput& Win32TextInput::operator=(Win32TextInput&&) noexcept = default;
void Win32TextInput::SetBounds(core::Rect bounds) {
    impl_->bounds = bounds;
    impl_->ApplyBounds();
}
core::Rect Win32TextInput::Bounds() const { return impl_->bounds; }
void Win32TextInput::SetVisible(bool visible) {
    impl_->visible = visible;
    if (!visible && impl_->handle && ::GetFocus() == impl_->handle && ::IsWindow(impl_->parent))
        ::SetFocus(impl_->parent);
}
void Win32TextInput::SetEnabled(bool enabled) {
    if (impl_->handle && (::IsWindowEnabled(impl_->handle) != FALSE) != enabled)
        ::EnableWindow(impl_->handle, enabled);
}
void Win32TextInput::SetBorderVisible(bool visible) {
    if (!impl_->handle) return;
    ::LONG_PTR style = ::GetWindowLongPtrW(impl_->handle, GWL_STYLE);
    ::LONG_PTR extendedStyle = ::GetWindowLongPtrW(impl_->handle, GWL_EXSTYLE);
    const ::LONG_PTR nextStyle = visible ? style | WS_BORDER : style & ~static_cast<::LONG_PTR>(WS_BORDER);
    const ::LONG_PTR nextExtendedStyle = visible
        ? extendedStyle : extendedStyle & ~static_cast<::LONG_PTR>(WS_EX_CLIENTEDGE);
    if (style == nextStyle && extendedStyle == nextExtendedStyle) return;
    ::SetWindowLongPtrW(impl_->handle, GWL_STYLE, nextStyle);
    ::SetWindowLongPtrW(impl_->handle, GWL_EXSTYLE, nextExtendedStyle);
    ::SetWindowPos(impl_->handle, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
void Win32TextInput::SetOptions(const ui::DuiTextInputOptions& options) {
    if (!impl_->handle) return;
    impl_->options = options;
    ::LONG_PTR style = ::GetWindowLongPtrW(impl_->handle, GWL_STYLE);
    style = options.multiline ? style | ES_MULTILINE : style & ~static_cast<::LONG_PTR>(ES_MULTILINE);
    style = options.wordWrap ? style & ~static_cast<::LONG_PTR>(ES_AUTOHSCROLL) : style | ES_AUTOHSCROLL;
    ::SetWindowLongPtrW(impl_->handle, GWL_STYLE, style);
    ::SendMessageW(impl_->handle, EM_SETREADONLY, options.readOnly, 0);
    ::SendMessageW(impl_->handle, EM_SETLIMITTEXT, options.maxLength > 0 ? options.maxLength : 0, 0);
    ::SendMessageW(impl_->handle, EM_SETPASSWORDCHAR, options.password ? 0x25CF : 0, 0);
    impl_->ApplyBounds();
    ::InvalidateRect(impl_->handle, nullptr, TRUE);
}
void Win32TextInput::SetPlaceholder(std::string text) {
    if (impl_->handle) {
        const std::wstring placeholder = detail::Utf8ToWide(text);
        ::SendMessageW(impl_->handle, EM_SETCUEBANNER, 0, reinterpret_cast<::LPARAM>(placeholder.c_str()));
    }
}
void Win32TextInput::SetText(std::string text) {
    if (!impl_->handle)
        return;
    const std::wstring nativeText = detail::Utf8ToWide(text);
    ::SetWindowTextW(impl_->handle, nativeText.c_str());
    ::SendMessageW(impl_->handle, EM_SETSEL, nativeText.size(), nativeText.size());
}
std::string Win32TextInput::Text() const {
    if (!impl_->handle)
        return {};
    const int length = ::GetWindowTextLengthW(impl_->handle);
    std::wstring nativeText(static_cast<std::size_t>(length) + 1, L'\0');
    ::GetWindowTextW(impl_->handle, nativeText.data(), length + 1);
    nativeText.resize(static_cast<std::size_t>(length));
    return detail::WideToUtf8(nativeText);
}
std::size_t Win32TextInput::CaretPosition() const
{
    return impl_->Utf8Selection().end;
}
bool Win32TextInput::CaretVisible() const { return impl_->CaretVisible(); }
ui::DuiTextSelection Win32TextInput::TextSelection() const { return impl_->Utf8Selection(); }
void Win32TextInput::Focus()
{
    if (impl_->handle && impl_->visible && ::IsWindowEnabled(impl_->handle))
        ::SetFocus(impl_->handle);
}
void Win32TextInput::FocusAt(core::Point position)
{
    Focus();
    if (!impl_->handle || ::GetFocus() != impl_->handle)
        return;
    const int index = impl_->NativeIndexFromPoint(position);
    ::SendMessageW(impl_->handle, EM_SETSEL, index, index);
    if (::IsWindow(impl_->parent))
        ::InvalidateRect(impl_->parent, nullptr, FALSE);
}
void Win32TextInput::BeginSelection(core::Point position)
{
    Focus();
    if (!impl_->handle || ::GetFocus() != impl_->handle)
        return;
    impl_->selectionAnchor = impl_->NativeIndexFromPoint(position);
    impl_->selecting = true;
    ::SendMessageW(impl_->handle, EM_SETSEL, impl_->selectionAnchor, impl_->selectionAnchor);
    if (::IsWindow(impl_->parent))
        ::InvalidateRect(impl_->parent, nullptr, FALSE);
}
void Win32TextInput::UpdateSelection(core::Point position)
{
    if (!impl_->handle || !impl_->selecting)
        return;
    ::SendMessageW(impl_->handle, EM_SETSEL, impl_->selectionAnchor,
                   impl_->NativeIndexFromPoint(position));
    if (::IsWindow(impl_->parent))
        ::InvalidateRect(impl_->parent, nullptr, FALSE);
}
void Win32TextInput::EndSelection() { impl_->selecting = false; }
void Win32TextInput::SelectWordAt(core::Point position)
{
    Focus();
    if (!impl_->handle || ::GetFocus() != impl_->handle)
        return;
    // 原生 EDIT 为隐藏输入会话（不可命中测试），自身收不到鼠标消息，
    // 故转发一次完整双击序列，复用其默认选词规则选中 position 所在的单词。
    const ::POINT point = impl_->NativePointFrom(position);
    const ::LPARAM packed = MAKELPARAM(static_cast<::WORD>(point.x), static_cast<::WORD>(point.y));
    ::SendMessageW(impl_->handle, WM_LBUTTONDOWN, MK_LBUTTON, packed);
    ::SendMessageW(impl_->handle, WM_LBUTTONUP, 0, packed);
    ::SendMessageW(impl_->handle, WM_LBUTTONDBLCLK, MK_LBUTTON, packed);
    ::SendMessageW(impl_->handle, WM_LBUTTONUP, 0, packed);
    if (::IsWindow(impl_->parent))
        ::InvalidateRect(impl_->parent, nullptr, FALSE);
}
void Win32TextInput::SetChangedHandler(std::function<void()> handler) { impl_->changed = std::move(handler); }
void Win32TextInput::SetFocusLostHandler(std::function<void()> handler) { impl_->focusLost = std::move(handler); }
void Win32TextInput::SetSubmitHandler(std::function<void()> handler) { impl_->submitted = std::move(handler); }
void Win32TextInput::SetCancelHandler(std::function<void()> handler) { impl_->cancelled = std::move(handler); }
} // namespace ysDui::platform::win32
