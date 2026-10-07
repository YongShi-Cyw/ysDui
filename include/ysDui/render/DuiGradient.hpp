/**
 * 文件名：DuiGradient.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的最小双色渐变值类型。
 */
#pragma once

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::render {

/**
 * 线性渐变定义。
 * 用途：描述沿水平或垂直轴从起始颜色到结束颜色的双色填充。
 */
struct DuiLinearGradient final
{
    core::Color start;
    core::Color end;
    bool vertical{true};
};

/**
 * 径向渐变定义。
 * 用途：描述从形状中心颜色向边缘颜色过渡的双色填充。
 */
struct DuiRadialGradient final
{
    core::Color center;
    core::Color edge;
};

} // namespace ysDui::render
