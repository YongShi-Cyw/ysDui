/**
 * 文件名：win32_event_converter_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：验证 Win32 消息转换的 DPI、键盘、指针和 UTF-8 文本语义。
 */
#define NOMINMAX
#include <windows.h>

#include <cassert>
#include <cstdint>

#include "DuiWin32EventConverter.hpp"

int main()
{
    ::HWND window = ::CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
        10, 20, 200, 100, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    assert(window != nullptr);

    ysDui::platform::win32::Win32EventConverter converter;
    converter.SetDpiScale(ysDui::core::DuiDpiScale(192));
    auto events = converter.Convert(reinterpret_cast<std::uintptr_t>(window),
                                    WM_MOUSEMOVE, 0, MAKELPARAM(20, 40));
    assert(events.size() == 1);
    assert(events.front().type == ysDui::core::EventType::PointerMove);
    assert((events.front().position == ysDui::core::Point{10, 20}));
    assert(events.front().pointerDevice == ysDui::core::PointerDeviceType::Mouse);

    events = converter.Convert(reinterpret_cast<std::uintptr_t>(window), WM_MOUSELEAVE, 0, 0);
    assert(events.size() == 1
        && events.front().type == ysDui::core::EventType::PointerLeave);
    assert((events.front().position == ysDui::core::Point{10, 20}));
    events = converter.Convert(reinterpret_cast<std::uintptr_t>(window), WM_CAPTURECHANGED, 0, 0);
    assert(events.size() == 1
        && events.front().type == ysDui::core::EventType::PointerCancel);

    events = converter.Convert(reinterpret_cast<std::uintptr_t>(window), WM_KEYDOWN, VK_LEFT, 0);
    assert(events.size() == 1 && events.front().type == ysDui::core::EventType::KeyDown
        && events.front().key == ysDui::core::key::Left);
    events = converter.Convert(reinterpret_cast<std::uintptr_t>(window), WM_KEYUP, VK_LEFT, 0);
    assert(events.size() == 1 && events.front().type == ysDui::core::EventType::KeyUp
        && events.front().key == ysDui::core::key::Left);

    events = converter.Convert(reinterpret_cast<std::uintptr_t>(window), WM_CHAR, 0xd83d, 0);
    assert(events.empty());
    events = converter.Convert(reinterpret_cast<std::uintptr_t>(window), WM_CHAR, 0xde00, 0);
    assert(events.size() == 1 && events.front().type == ysDui::core::EventType::TextInput);
    assert(events.front().character == U'\U0001f600');
    assert(events.front().text == "\xf0\x9f\x98\x80");

    events = converter.Convert(reinterpret_cast<std::uintptr_t>(window),
                               WM_IME_STARTCOMPOSITION, 0, 0);
    assert(events.size() == 1
        && events.front().type == ysDui::core::EventType::CompositionStart);
    events = converter.Convert(reinterpret_cast<std::uintptr_t>(window),
                               WM_IME_ENDCOMPOSITION, 0, 0);
    assert(events.size() == 1
        && events.front().type == ysDui::core::EventType::CompositionEnd);

    events = converter.Convert(reinterpret_cast<std::uintptr_t>(window),
                               WM_DPICHANGED, MAKEWPARAM(144, 144), 0);
    assert(events.size() == 1 && events.front().type == ysDui::core::EventType::DpiChanged
        && events.front().dpi == 144);

    assert(::DestroyWindow(window));
}
