#pragma once

#include <string>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::core {

namespace key {
constexpr unsigned int Backspace = 8;
constexpr unsigned int Tab = 9;
constexpr unsigned int Enter = 13;
constexpr unsigned int Escape = 27;
constexpr unsigned int Space = 32;
constexpr unsigned int Delete = 127;
constexpr unsigned int Function1 = 0x100;
constexpr unsigned int Function2 = Function1 + 1;
constexpr unsigned int Shift = 0x120;
constexpr unsigned int Control = 0x121;
constexpr unsigned int Alt = 0x122;
constexpr unsigned int Meta = 0x123;
constexpr unsigned int Left = 0x124;
constexpr unsigned int Right = 0x125;
constexpr unsigned int Home = 0x126;
constexpr unsigned int End = 0x127;
constexpr unsigned int Up = 0x128;
constexpr unsigned int Down = 0x129;
constexpr unsigned int PageUp = 0x12A;
constexpr unsigned int PageDown = 0x12B;
}

namespace modifier {
constexpr unsigned int Shift = 1U << 0;
constexpr unsigned int Control = 1U << 1;
constexpr unsigned int Alt = 1U << 2;
constexpr unsigned int Meta = 1U << 3;
}

enum class EventType {
    PointerMove,
    PointerDown,
    PointerUp,
    PointerDoubleClick,
    PointerWheel,
    PointerLeave,
    PointerCancel,
    KeyDown,
    KeyUp,
    TextInput,
    CompositionStart,
    CompositionUpdate,
    CompositionEnd,
    FocusGained,
    FocusLost,
    DpiChanged,
};

enum class PointerButton {
    Primary,
    Secondary,
    Middle,
};

enum class PointerDeviceType {
    Mouse,
    Touch,
    Pen,
    Unknown,
};

struct Event final {
    EventType type{};
    Point position{};
    unsigned int key{};
    unsigned int modifiers{};
    char32_t character{};
    int wheelDelta{};
    PointerButton button{PointerButton::Primary};
    unsigned int pointerId{};
    PointerDeviceType pointerDevice{PointerDeviceType::Mouse};
    std::string text;
    int compositionCursor{};
    int dpi{96};
};

} // namespace ysDui::core
