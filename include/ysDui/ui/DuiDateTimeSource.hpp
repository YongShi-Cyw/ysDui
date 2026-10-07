/**
 * 文件名：DuiDateTimeSource.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明平台无关的本地日期时间来源。
 */
#pragma once

#include "ysDui/core/DuiDateTime.hpp"

namespace ysDui::ui {

class DuiDateTimeSource
{
public:
    virtual ~DuiDateTimeSource() = default;
    /** @return 当前本地日期和时间。 */
    [[nodiscard]] virtual core::DateTime LocalNow() const = 0;
};

} // namespace ysDui::ui
