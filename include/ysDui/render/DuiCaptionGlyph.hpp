/**
 * 文件名：DuiCaptionGlyph.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：声明窗口标题栏按钮图标的生成接口（与 DuiDialog 共用同一套矢量图标）。
 */
#pragma once

#include <memory>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::core {
struct Color;
}

namespace ysDui::render {

class DuiImage;

/** 标题栏按钮图标种类。 */
enum class DuiCaptionGlyph
{
    Minimize, // 最小化：横线。
    Maximize, // 最大化：方框。
    Restore,  // 由最大化还原：双层方框。
    Close,    // 关闭：叉。
};

/**
 * 生成标题栏按钮图标，按给定颜色着色（图标为 16x16 的已解码位图）。
 * 用途：让使用方（DuiDialog、平台层自定义标题栏）复用同一套矢量图标与着色逻辑。
 * @param glyph 图标种类
 * @param color 着色；图标的透明区域保持透明
 * @return 生成的位图；光栅化失败时返回空指针
 */
[[nodiscard]] std::shared_ptr<const DuiImage> CreateCaptionGlyphImage(DuiCaptionGlyph glyph,
                                                                     core::Color color);

} // namespace ysDui::render
