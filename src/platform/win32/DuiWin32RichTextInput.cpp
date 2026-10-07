#include "ysDui/platform/win32/DuiWin32RichTextInput.hpp"
#include "DuiWin32RichImageOle.hpp"
#include "DuiWin32EditContextMenu.hpp"
#include "DuiImageAccess.hpp"
#include "DuiWin32Utf8.hpp"
#include "ysDui/core/DuiDpi.hpp"

#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

namespace ysDui::platform::win32 {
namespace {
std::string TextAtRange(::HWND handle, const ::CHARRANGE& range)
{
    const long length = (std::max)(0L, range.cpMax - range.cpMin);
    std::wstring nativeText(static_cast<std::size_t>(length) + 1, L'\0');
    ::TEXTRANGEW request{};
    request.chrg = range;
    request.lpstrText = nativeText.data();
    ::SendMessageW(handle, EM_GETTEXTRANGE, 0, reinterpret_cast<::LPARAM>(&request));
    nativeText.resize(static_cast<std::size_t>(length));
    return detail::WideToUtf8(nativeText);
}

std::string FormatSize(std::uint64_t value)
{
    static constexpr const char* Units[] = {"B", "KB", "MB", "GB", "TB"};
    std::uint64_t divisor = 1;
    int unit{};
    while (value / divisor >= 1024 && unit < 4) { divisor *= 1024; ++unit; }
    std::string digits = std::to_string(value / divisor);
    std::string result = digits;
    result += " ";
    result += Units[unit];
    return result;
}
}

class Win32RichTextInput::Impl {
public:
    ::HWND parent{};
    ::HWND handle{};
    core::Rect bounds;
    ui::DuiTextInputOptions options;
    std::function<void()> changed;
    std::function<void()> focusLost;
    std::function<void(std::string)> linkActivated;

    static ::LRESULT CALLBACK ParentProcedure(::HWND parent, ::UINT message, ::WPARAM word,
                                                ::LPARAM data, ::UINT_PTR, ::DWORD_PTR reference)
    {
        auto* state = reinterpret_cast<Impl*>(reference);
        if (message == WM_COMMAND && reinterpret_cast<::HWND>(data) == state->handle &&
            HIWORD(word) == EN_CHANGE && state->changed)
            state->changed();
        if (message == WM_NOTIFY) {
            const auto* notification = reinterpret_cast<const ::NMHDR*>(data);
            if (notification && notification->hwndFrom == state->handle && notification->code == EN_LINK && state->linkActivated) {
                const auto* link = reinterpret_cast<const ::ENLINK*>(data);
                if (link->msg == WM_LBUTTONUP)
                    state->linkActivated(TextAtRange(state->handle, link->chrg));
            }
        }
        return ::DefSubclassProc(parent, message, word, data);
    }

    static ::LRESULT CALLBACK Procedure(::HWND handle, ::UINT message, ::WPARAM word,
                                         ::LPARAM data, ::UINT_PTR, ::DWORD_PTR reference)
    {
        auto* state = reinterpret_cast<Impl*>(reference);
        if (message == WM_KILLFOCUS && state->focusLost)
            state->focusLost();
        if (message == WM_CONTEXTMENU) {
            ::CHARRANGE selection{};
            ::SendMessageW(handle, EM_EXGETSEL, 0, reinterpret_cast<::LPARAM>(&selection));
            controls::input::DuiEditContextState menuState;
            menuState.readOnly = state->options.readOnly;
            menuState.password = state->options.password;
            menuState.hasSelection = selection.cpMin != selection.cpMax;
            menuState.hasText = ::GetWindowTextLengthW(handle) > 0;
            menuState.clipboardHasText = ::IsClipboardFormatAvailable(CF_UNICODETEXT) || ::IsClipboardFormatAvailable(CF_TEXT);
            const ::POINT point{static_cast<int>(static_cast<short>(LOWORD(data))), static_cast<int>(static_cast<short>(HIWORD(data)))};
            if (ShowWin32EditContextMenu(handle, point, menuState))
                return 0;
        }
        return ::DefSubclassProc(handle, message, word, data);
    }
};

Win32RichTextInput::Win32RichTextInput(NativeWindowHandle parent) : impl_(std::make_unique<Impl>())
{
    static const ::HMODULE richEditModule = ::LoadLibraryW(L"Msftedit.dll");
    impl_->parent = reinterpret_cast<::HWND>(parent.value);
    if (!richEditModule || !impl_->parent)
        return;
    impl_->handle = ::CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
                                       0, 0, 0, 0, impl_->parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (!impl_->handle)
        return;
    ::SetWindowSubclass(impl_->handle, Impl::Procedure, 1, reinterpret_cast<::DWORD_PTR>(impl_.get()));
    ::SetWindowSubclass(impl_->parent, Impl::ParentProcedure, reinterpret_cast<::UINT_PTR>(impl_.get()),
                        reinterpret_cast<::DWORD_PTR>(impl_.get()));
    ::SendMessageW(impl_->handle, EM_SETEVENTMASK, 0, ::SendMessageW(impl_->handle, EM_GETEVENTMASK, 0, 0) | ENM_LINK);
}

Win32RichTextInput::~Win32RichTextInput()
{
    if (impl_->parent && ::IsWindow(impl_->parent))
        ::RemoveWindowSubclass(impl_->parent, Impl::ParentProcedure, reinterpret_cast<::UINT_PTR>(impl_.get()));
    if (impl_->handle && ::IsWindow(impl_->handle))
        ::DestroyWindow(impl_->handle);
}

Win32RichTextInput::Win32RichTextInput(Win32RichTextInput&&) noexcept = default;
Win32RichTextInput& Win32RichTextInput::operator=(Win32RichTextInput&&) noexcept = default;
void Win32RichTextInput::SetBounds(core::Rect bounds)
{
    impl_->bounds = bounds;
    if (!impl_->handle)
        return;
    const int dpi = impl_->parent != nullptr ? static_cast<int>(::GetDpiForWindow(impl_->parent))
                                             : core::DuiDpiScale::DefaultDpi;
    const core::Rect pixels = core::DuiDpiScale(dpi).Scale(bounds);
    ::SetWindowPos(impl_->handle, nullptr, pixels.left, pixels.top,
                   pixels.Width(), pixels.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
}
core::Rect Win32RichTextInput::Bounds() const { return impl_->bounds; }
void Win32RichTextInput::SetVisible(bool visible) { if (impl_->handle) ::ShowWindow(impl_->handle, visible ? SW_SHOW : SW_HIDE); }
void Win32RichTextInput::SetEnabled(bool enabled) { if (impl_->handle) ::EnableWindow(impl_->handle, enabled); }
void Win32RichTextInput::SetBorderVisible(bool visible)
{
    if (!impl_->handle)
        return;
    ::LONG_PTR style = ::GetWindowLongPtrW(impl_->handle, GWL_STYLE);
    style = visible ? style | WS_BORDER : style & ~static_cast<::LONG_PTR>(WS_BORDER);
    ::SetWindowLongPtrW(impl_->handle, GWL_STYLE, style);
    ::SetWindowPos(impl_->handle, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
void Win32RichTextInput::SetOptions(const ui::DuiTextInputOptions& options)
{
    if (!impl_->handle)
        return;
    impl_->options = options;
    ::LONG_PTR style = ::GetWindowLongPtrW(impl_->handle, GWL_STYLE);
    style = options.multiline ? style | ES_MULTILINE : style & ~static_cast<::LONG_PTR>(ES_MULTILINE);
    style = options.wordWrap ? style & ~static_cast<::LONG_PTR>(ES_AUTOHSCROLL) : style | ES_AUTOHSCROLL;
    ::SetWindowLongPtrW(impl_->handle, GWL_STYLE, style);
    ::SendMessageW(impl_->handle, EM_SETREADONLY, options.readOnly, 0);
    ::SendMessageW(impl_->handle, EM_EXLIMITTEXT, 0, options.maxLength > 0 ? options.maxLength : 0x7ffffffe);
    ::SendMessageW(impl_->handle, EM_SETPASSWORDCHAR, options.password ? 0x25CF : 0, 0);
    ::InvalidateRect(impl_->handle, nullptr, TRUE);
}
void Win32RichTextInput::SetPlaceholder(std::string text) {
    if (!impl_->handle)
        return;
    const std::wstring placeholder = detail::Utf8ToWide(text);
    ::SendMessageW(impl_->handle, EM_SETCUEBANNER, 0, reinterpret_cast<::LPARAM>(placeholder.c_str()));
}
void Win32RichTextInput::SetText(std::string text) {
    if (!impl_->handle)
        return;
    const std::wstring nativeText = detail::Utf8ToWide(text);
    ::SetWindowTextW(impl_->handle, nativeText.c_str());
}
std::string Win32RichTextInput::Text() const
{
    if (!impl_->handle)
        return {};
    const int length = ::GetWindowTextLengthW(impl_->handle);
    std::wstring nativeText(static_cast<std::size_t>(length) + 1, L'\0');
    ::GetWindowTextW(impl_->handle, nativeText.data(), length + 1);
    nativeText.resize(static_cast<std::size_t>(length));
    return detail::WideToUtf8(nativeText);
}
void Win32RichTextInput::Focus() { if (impl_->handle) ::SetFocus(impl_->handle); }
void Win32RichTextInput::SetChangedHandler(std::function<void()> handler) { impl_->changed = std::move(handler); }
void Win32RichTextInput::SetFocusLostHandler(std::function<void()> handler) { impl_->focusLost = std::move(handler); }
void Win32RichTextInput::SetSelection(ui::DuiTextRange range)
{
    if (!impl_->handle)
        return;
    const long length = static_cast<long>(Text().size());
    const long start = std::clamp(static_cast<long>(range.start), 0L, length);
    const long end = std::clamp(static_cast<long>(range.end), start, length);
    ::CHARRANGE value{start, end};
    ::SendMessageW(impl_->handle, EM_EXSETSEL, 0, reinterpret_cast<::LPARAM>(&value));
}
ui::DuiTextRange Win32RichTextInput::Selection() const
{
    if (!impl_->handle)
        return {};
    ::CHARRANGE range{};
    ::SendMessageW(impl_->handle, EM_EXGETSEL, 0, reinterpret_cast<::LPARAM>(&range));
    return {static_cast<int>(range.cpMin), static_cast<int>(range.cpMax)};
}
void Win32RichTextInput::SelectAll() { if (impl_->handle) ::SendMessageW(impl_->handle, EM_SETSEL, 0, -1); }
void Win32RichTextInput::ReplaceSelection(std::string text)
{
    if (!impl_->handle)
        return;
    const std::wstring nativeText = detail::Utf8ToWide(text);
    ::SendMessageW(impl_->handle, EM_REPLACESEL, TRUE, reinterpret_cast<::LPARAM>(nativeText.c_str()));
}
void Win32RichTextInput::AppendText(std::string text) { if (impl_->handle) { ::SendMessageW(impl_->handle, EM_SETSEL, static_cast<::WPARAM>(-1), -1); ReplaceSelection(std::move(text)); } }
bool Win32RichTextInput::CanUndo() const { return impl_->handle && ::SendMessageW(impl_->handle, EM_CANUNDO, 0, 0) != 0; }
void Win32RichTextInput::Undo() { if (impl_->handle) ::SendMessageW(impl_->handle, EM_UNDO, 0, 0); }
void Win32RichTextInput::Cut() { if (impl_->handle) ::SendMessageW(impl_->handle, WM_CUT, 0, 0); }
void Win32RichTextInput::Copy() { if (impl_->handle) ::SendMessageW(impl_->handle, WM_COPY, 0, 0); }
void Win32RichTextInput::Paste() { if (impl_->handle) ::SendMessageW(impl_->handle, WM_PASTE, 0, 0); }
void Win32RichTextInput::ClearSelection() { if (impl_->handle) ::SendMessageW(impl_->handle, WM_CLEAR, 0, 0); }
void Win32RichTextInput::SetSelectionFormat(ui::DuiRichTextFormat format)
{
    if (!impl_->handle)
        return;
    ::CHARFORMAT2W value{};
    value.cbSize = sizeof(value);
    value.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_COLOR;
    value.dwEffects = (format.bold ? CFE_BOLD : 0) | (format.italic ? CFE_ITALIC : 0) | (format.underline ? CFE_UNDERLINE : 0);
    value.crTextColor = RGB(format.color.red, format.color.green, format.color.blue);
    ::SendMessageW(impl_->handle, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<::LPARAM>(&value));
}
void Win32RichTextInput::SetAutomaticLinkDetection(bool enabled) { if (impl_->handle) ::SendMessageW(impl_->handle, EM_AUTOURLDETECT, enabled, 0); }
void Win32RichTextInput::SetLinkActivatedHandler(std::function<void(std::string)> handler) { impl_->linkActivated = std::move(handler); }
bool Win32RichTextInput::InsertImage(const render::DuiImage& image, core::Size maximumSize)
{
    if (!impl_->handle || image.Empty() || image.Format() != render::DuiImageFormat::Bgra8Premultiplied)
        return false;
    const core::Size source = image.Size();
    int width = source.width;
    int height = source.height;
    if (maximumSize.width > 0 && width > maximumSize.width) { height = height * maximumSize.width / width; width = maximumSize.width; }
    if (maximumSize.height > 0 && height > maximumSize.height) { width = width * maximumSize.height / height; height = maximumSize.height; }
    ::BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = source.width;
    info.bmiHeader.biHeight = -source.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits{};
    HBITMAP bitmap = ::CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits)
        return false;
    const auto& pixels = render::DuiImageAccess::Pixels(image);
    std::memcpy(bits, pixels.data(), pixels.size());
    if (width != source.width || height != source.height) {
        HBITMAP scaled = static_cast<HBITMAP>(::CopyImage(bitmap, IMAGE_BITMAP, width, height, LR_CREATEDIBSECTION));
        ::DeleteObject(bitmap);
        bitmap = scaled;
        if (!bitmap)
            return false;
    }
    return InsertRichImageOle(impl_->handle, bitmap);
}
void Win32RichTextInput::InsertQuoteBlock(std::string sender, std::string body)
{
    SetSelectionFormat({{110, 110, 110, 255}, true, false, false});
    ReplaceSelection("\u2503  " + sender + "\r\n");
    SetSelectionFormat({{110, 110, 110, 255}, false, true, false});
    std::size_t start{};
    while (start <= body.size()) {
        const std::size_t end = body.find('\n', start);
        ReplaceSelection("\u2503  " + body.substr(start, end - start) + "\r\n");
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    SetSelectionFormat({{30, 30, 30, 255}, false, false, false});
}
void Win32RichTextInput::InsertFileCard(std::string fileName, std::uint64_t sizeBytes)
{
    SetSelectionFormat({{70, 70, 70, 255}, false, false, true});
    ReplaceSelection("[file]  " + fileName + "   " + FormatSize(sizeBytes) + "\r\n");
    SetSelectionFormat({{30, 30, 30, 255}, false, false, false});
}

} // namespace ysDui::platform::win32
