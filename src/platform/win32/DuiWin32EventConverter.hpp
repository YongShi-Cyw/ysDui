/**
 * 文件名：DuiWin32EventConverter.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明 Win32 消息到平台无关事件的有状态转换器。
 */
#pragma once

#include <cstdint>
#include <vector>

#include "ysDui/core/DuiDpi.hpp"
#include "ysDui/core/DuiEvent.hpp"

namespace ysDui::platform::win32 {

class Win32EventConverter final
{
public:
    void SetDpiScale(core::DuiDpiScale scale);
    [[nodiscard]] std::vector<core::Event> Convert(
        std::uintptr_t window, unsigned int message,
        std::uintptr_t word, std::intptr_t data);

private:
    core::DuiDpiScale scale_;
    core::Point lastPointerPosition_;
    char16_t pendingHighSurrogate_{};
};

} // namespace ysDui::platform::win32
