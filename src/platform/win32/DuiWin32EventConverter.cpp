/**
 * 文件名：DuiWin32EventConverter.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：实现键盘、指针、UTF-16 和 IME 消息到 core::Event 的转换。
 */
#include "DuiWin32EventConverter.hpp"

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <imm.h>

#include <string>

#include "DuiWin32Utf8.hpp"

namespace ysDui::platform::win32 {
namespace {

unsigned int KeyCode(::WPARAM word)
{
    switch (word)
    {
    case VK_BACK: return core::key::Backspace;
    case VK_TAB: return core::key::Tab;
    case VK_RETURN: return core::key::Enter;
    case VK_ESCAPE: return core::key::Escape;
    case VK_SPACE: return core::key::Space;
    case VK_DELETE: return core::key::Delete;
    case VK_LEFT: return core::key::Left;
    case VK_RIGHT: return core::key::Right;
    case VK_HOME: return core::key::Home;
    case VK_END: return core::key::End;
    case VK_UP: return core::key::Up;
    case VK_DOWN: return core::key::Down;
    case VK_PRIOR: return core::key::PageUp;
    case VK_NEXT: return core::key::PageDown;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT: return core::key::Shift;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL: return core::key::Control;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU: return core::key::Alt;
    case VK_LWIN:
    case VK_RWIN: return core::key::Meta;
    default:
        if (word >= VK_F1 && word <= VK_F24)
            return core::key::Function1 + static_cast<unsigned int>(word - VK_F1);
        return static_cast<unsigned int>(word);
    }
}

unsigned int Modifiers(::WPARAM mouseState = 0)
{
    unsigned int result{};
    if ((mouseState & MK_SHIFT) != 0 || ::GetKeyState(VK_SHIFT) < 0)
        result |= core::modifier::Shift;
    if ((mouseState & MK_CONTROL) != 0 || ::GetKeyState(VK_CONTROL) < 0)
        result |= core::modifier::Control;
    if (::GetKeyState(VK_MENU) < 0)
        result |= core::modifier::Alt;
    if (::GetKeyState(VK_LWIN) < 0 || ::GetKeyState(VK_RWIN) < 0)
        result |= core::modifier::Meta;
    return result;
}

core::PointerDeviceType DeviceType(::UINT32 pointerId)
{
    ::POINTER_INPUT_TYPE type{};
    if (!::GetPointerType(pointerId, &type))
        return core::PointerDeviceType::Unknown;
    switch (type)
    {
    case PT_MOUSE: return core::PointerDeviceType::Mouse;
    case PT_TOUCH: return core::PointerDeviceType::Touch;
    case PT_PEN: return core::PointerDeviceType::Pen;
    default: return core::PointerDeviceType::Unknown;
    }
}

std::string Utf8FromCodePoint(char32_t codePoint)
{
    if (codePoint > 0x10ffff || (codePoint >= 0xd800 && codePoint <= 0xdfff))
        codePoint = 0xfffd;
    std::string result;
    if (codePoint <= 0x7f)
        result.push_back(static_cast<char>(codePoint));
    else if (codePoint <= 0x7ff)
    {
        result.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
    else if (codePoint <= 0xffff)
    {
        result.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
    else
    {
        result.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
    return result;
}

std::string CompositionText(::HWND window, ::DWORD index)
{
    const ::HIMC context = ::ImmGetContext(window);
    if (context == nullptr)
        return {};
    const ::LONG byteCount = ::ImmGetCompositionStringW(context, index, nullptr, 0);
    std::wstring text;
    if (byteCount > 0)
    {
        text.resize(static_cast<std::size_t>(byteCount) / sizeof(wchar_t));
        if (::ImmGetCompositionStringW(context, index, text.data(),
                                       static_cast<::DWORD>(byteCount)) < 0)
        {
            text.clear();
        }
    }
    ::ImmReleaseContext(window, context);
    return detail::WideToUtf8(text);
}

int CompositionCursor(::HWND window)
{
    const ::HIMC context = ::ImmGetContext(window);
    if (context == nullptr)
        return 0;
    const ::LONG cursor = ::ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0);
    ::ImmReleaseContext(window, context);
    return cursor >= 0 ? static_cast<int>(cursor) : 0;
}

core::Event PointerEvent(core::EventType type, core::Point position,
                         core::PointerButton button, unsigned int pointerId,
                         core::PointerDeviceType device, unsigned int modifiers)
{
    core::Event event;
    event.type = type;
    event.position = position;
    event.button = button;
    event.pointerId = pointerId;
    event.pointerDevice = device;
    event.modifiers = modifiers;
    return event;
}

} // namespace

void Win32EventConverter::SetDpiScale(core::DuiDpiScale scale)
{
    scale_ = scale;
}

std::vector<core::Event> Win32EventConverter::Convert(
    std::uintptr_t windowValue, unsigned int message,
    std::uintptr_t wordValue, std::intptr_t dataValue)
{
    const auto window = reinterpret_cast<::HWND>(windowValue);
    const auto word = static_cast<::WPARAM>(wordValue);
    const auto data = static_cast<::LPARAM>(dataValue);
    std::vector<core::Event> result;

    if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP
        || message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN
        || message == WM_RBUTTONUP || message == WM_MBUTTONDOWN || message == WM_MBUTTONUP)
    {
        const core::Point pixels{GET_X_LPARAM(data), GET_Y_LPARAM(data)};
        lastPointerPosition_ = scale_.Unscale(pixels);
        core::EventType type = core::EventType::PointerMove;
        if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
            type = core::EventType::PointerDown;
        else if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
            type = core::EventType::PointerUp;
        else if (message == WM_LBUTTONDBLCLK)
            type = core::EventType::PointerDoubleClick;
        const core::PointerButton button = message == WM_RBUTTONDOWN || message == WM_RBUTTONUP
            ? core::PointerButton::Secondary
            : message == WM_MBUTTONDOWN || message == WM_MBUTTONUP
                ? core::PointerButton::Middle : core::PointerButton::Primary;
        result.push_back(PointerEvent(type, lastPointerPosition_, button, 0,
                                      core::PointerDeviceType::Mouse, Modifiers(word)));
        return result;
    }

    if (message == WM_MOUSEWHEEL)
    {
        ::POINT point{GET_X_LPARAM(data), GET_Y_LPARAM(data)};
        ::ScreenToClient(window, &point);
        lastPointerPosition_ = scale_.Unscale(core::Point{point.x, point.y});
        core::Event event = PointerEvent(core::EventType::PointerWheel, lastPointerPosition_,
            core::PointerButton::Primary, 0, core::PointerDeviceType::Mouse, Modifiers(word));
        event.wheelDelta = GET_WHEEL_DELTA_WPARAM(word);
        result.push_back(std::move(event));
        return result;
    }

    if (message == WM_MOUSELEAVE || message == WM_CAPTURECHANGED)
    {
        result.push_back(PointerEvent(
            message == WM_MOUSELEAVE ? core::EventType::PointerLeave
                                     : core::EventType::PointerCancel,
            lastPointerPosition_, core::PointerButton::Primary, 0,
            core::PointerDeviceType::Mouse, Modifiers()));
        return result;
    }

    if (message == WM_POINTERUPDATE || message == WM_POINTERDOWN || message == WM_POINTERUP
        || message == WM_POINTERLEAVE || message == WM_POINTERCAPTURECHANGED
        || message == WM_POINTERWHEEL)
    {
        const ::UINT32 pointerId = GET_POINTERID_WPARAM(word);
        if (message == WM_POINTERCAPTURECHANGED)
        {
            result.push_back(PointerEvent(core::EventType::PointerCancel,
                lastPointerPosition_, core::PointerButton::Primary, pointerId,
                DeviceType(pointerId), Modifiers()));
            return result;
        }
        ::POINTER_INFO info{};
        if (!::GetPointerInfo(pointerId, &info))
        {
            if (message == WM_POINTERLEAVE)
            {
                result.push_back(PointerEvent(core::EventType::PointerLeave,
                    lastPointerPosition_, core::PointerButton::Primary, pointerId,
                    DeviceType(pointerId), Modifiers()));
            }
            return result;
        }
        ::POINT point = info.ptPixelLocation;
        ::ScreenToClient(window, &point);
        lastPointerPosition_ = scale_.Unscale(core::Point{point.x, point.y});
        core::EventType type = core::EventType::PointerMove;
        if (message == WM_POINTERDOWN) type = core::EventType::PointerDown;
        else if (message == WM_POINTERUP) type = core::EventType::PointerUp;
        else if (message == WM_POINTERLEAVE) type = core::EventType::PointerLeave;
        else if (message == WM_POINTERWHEEL) type = core::EventType::PointerWheel;
        const core::PointerButton button = (info.pointerFlags & POINTER_FLAG_SECONDBUTTON) != 0
            ? core::PointerButton::Secondary : core::PointerButton::Primary;
        core::Event event = PointerEvent(type, lastPointerPosition_, button, pointerId,
                                         DeviceType(pointerId), Modifiers());
        if (message == WM_POINTERWHEEL)
            event.wheelDelta = GET_WHEEL_DELTA_WPARAM(word);
        result.push_back(std::move(event));
        return result;
    }

    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN
        || message == WM_KEYUP || message == WM_SYSKEYUP)
    {
        core::Event event;
        event.type = message == WM_KEYDOWN || message == WM_SYSKEYDOWN
            ? core::EventType::KeyDown : core::EventType::KeyUp;
        event.key = KeyCode(word);
        event.modifiers = Modifiers();
        result.push_back(std::move(event));
        return result;
    }

    if (message == WM_CHAR)
    {
        const char16_t unit = static_cast<char16_t>(word);
        if (unit >= 0xd800 && unit <= 0xdbff)
        {
            pendingHighSurrogate_ = unit;
            return result;
        }
        char32_t codePoint = unit;
        if (unit >= 0xdc00 && unit <= 0xdfff && pendingHighSurrogate_ != 0)
        {
            codePoint = 0x10000 + ((pendingHighSurrogate_ - 0xd800) << 10)
                + (unit - 0xdc00);
        }
        pendingHighSurrogate_ = 0;
        core::Event event;
        event.type = core::EventType::TextInput;
        event.character = codePoint;
        event.text = Utf8FromCodePoint(codePoint);
        result.push_back(std::move(event));
        return result;
    }

    if (message == WM_UNICHAR && word != UNICODE_NOCHAR)
    {
        core::Event event;
        event.type = core::EventType::TextInput;
        event.character = static_cast<char32_t>(word);
        event.text = Utf8FromCodePoint(event.character);
        result.push_back(std::move(event));
        return result;
    }

    if (message == WM_IME_STARTCOMPOSITION || message == WM_IME_ENDCOMPOSITION)
    {
        core::Event event;
        event.type = message == WM_IME_STARTCOMPOSITION
            ? core::EventType::CompositionStart : core::EventType::CompositionEnd;
        result.push_back(std::move(event));
        return result;
    }

    if (message == WM_IME_COMPOSITION)
    {
        if ((data & GCS_RESULTSTR) != 0)
        {
            core::Event event;
            event.type = core::EventType::TextInput;
            event.text = CompositionText(window, GCS_RESULTSTR);
            if (!event.text.empty()) result.push_back(std::move(event));
        }
        if ((data & GCS_COMPSTR) != 0)
        {
            core::Event event;
            event.type = core::EventType::CompositionUpdate;
            event.text = CompositionText(window, GCS_COMPSTR);
            event.compositionCursor = CompositionCursor(window);
            result.push_back(std::move(event));
        }
        return result;
    }

    if (message == WM_SETFOCUS || message == WM_KILLFOCUS)
    {
        core::Event event;
        event.type = message == WM_SETFOCUS
            ? core::EventType::FocusGained : core::EventType::FocusLost;
        result.push_back(std::move(event));
        return result;
    }

    if (message == WM_DPICHANGED)
    {
        core::Event event;
        event.type = core::EventType::DpiChanged;
        event.dpi = static_cast<int>(LOWORD(word));
        result.push_back(std::move(event));
    }
    return result;
}

} // namespace ysDui::platform::win32
