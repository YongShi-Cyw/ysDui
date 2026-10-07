/**
 * 文件名：test_assertions.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：确保所有构建配置中的测试断言均保持启用。
 */
#pragma once

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <utility>

#include "ysDui/core/DuiEvent.hpp"

#ifdef NDEBUG
#error "ysDui tests require assertions"
#endif

namespace ysDui::test {

inline core::Event MakeEvent(core::EventType type, core::Point position = {}, unsigned int key = {},
                             unsigned int modifiers = {}, char32_t character = {}, int wheelDelta = {},
                             core::PointerButton button = core::PointerButton::Primary, unsigned int pointerId = {},
                             core::PointerDeviceType pointerDevice = core::PointerDeviceType::Mouse,
                             std::string text = {}, int compositionCursor = {}, int dpi = 96)
{
    return {type, position, key, modifiers, character, wheelDelta, button, pointerId, pointerDevice,
            std::move(text), compositionCursor, dpi};
}

} // namespace ysDui::test
