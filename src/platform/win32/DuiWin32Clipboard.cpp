#include "ysDui/platform/win32/DuiWin32Clipboard.hpp"

#include "DuiWin32Utf8.hpp"

#include <windows.h>

#include <cstring>
#include <optional>
#include <utility>

namespace ysDui::platform::win32 {
class Win32Clipboard::Impl
{
};

Win32Clipboard::Win32Clipboard()
    : impl_(std::make_unique<Impl>())
{
}

Win32Clipboard::~Win32Clipboard() = default;
Win32Clipboard::Win32Clipboard(Win32Clipboard&&) noexcept = default;
Win32Clipboard& Win32Clipboard::operator=(Win32Clipboard&&) noexcept = default;

bool Win32Clipboard::SetText(std::string text)
{
    const std::wstring nativeText = detail::Utf8ToWide(text);
    if (!::OpenClipboard(nullptr))
        return false;

    struct CloseClipboard final
    {
        ~CloseClipboard() { ::CloseClipboard(); }
    } closeClipboard;

    if (!::EmptyClipboard())
        return false;

    const std::size_t bytes = (nativeText.size() + 1) * sizeof(wchar_t);
    ::HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr)
        return false;

    void* destination = ::GlobalLock(memory);
    if (destination == nullptr)
    {
        ::GlobalFree(memory);
        return false;
    }
    std::memcpy(destination, nativeText.c_str(), bytes);
    ::GlobalUnlock(memory);
    if (::SetClipboardData(CF_UNICODETEXT, memory) == nullptr)
    {
        ::GlobalFree(memory);
        return false;
    }
    return true;
}

std::optional<std::string> Win32Clipboard::GetText() const
{
    if (!::OpenClipboard(nullptr))
        return std::nullopt;
    struct CloseClipboard final
    {
        ~CloseClipboard() { ::CloseClipboard(); }
    } closeClipboard;
    if (!::IsClipboardFormatAvailable(CF_UNICODETEXT))
        return std::nullopt;
    const ::HANDLE handle = ::GetClipboardData(CF_UNICODETEXT);
    if (handle == nullptr)
        return std::nullopt;
    const auto* text = static_cast<const wchar_t*>(::GlobalLock(handle));
    if (text == nullptr)
        return std::nullopt;
    const std::wstring value(text);
    ::GlobalUnlock(handle);
    return detail::WideToUtf8(value);
}
} // namespace ysDui::platform::win32
