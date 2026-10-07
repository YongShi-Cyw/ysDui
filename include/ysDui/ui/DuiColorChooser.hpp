/**
 * 文件名：DuiColorChooser.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明平台无关的颜色选择能力。
 */
#pragma once

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::ui {

struct DuiColorChooserResult final
{
    bool accepted{};
    core::Color color;
};

class DuiColorChooser
{
public:
    virtual ~DuiColorChooser() = default;
    /** @return 用户确认状态及选中的颜色。 */
    [[nodiscard]] virtual DuiColorChooserResult Choose(core::Color initial) = 0;
};

} // namespace ysDui::ui
