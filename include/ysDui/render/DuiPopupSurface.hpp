/**
 * 文件名：DuiPopupSurface.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：绘制弹出层表面（底色与 1 像素边框）的共用助手。
 */
#pragma once

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::core {
class DuiTheme;
}

namespace ysDui::render {

class Canvas;

/**
 * 绘制弹出层表面底色。
 * 说明：`IPopupHost` 不提供默认背景，弹出内容必须自绘；不铺底色会让内容与
 *       宿主窗口的背景混在一起（同色相邻时看不出弹出区域）。
 * 调用时机：内容绘制之前。
 * @param bounds 需铺底色的区域（通常取弹出内容矩形与脏区的交集）
 * @param theme 取色主题
 */
void PaintPopupBackground(Canvas& canvas, core::Rect bounds, const core::DuiTheme& theme);

/**
 * 绘制弹出层 1 像素边框。
 * 调用时机：内容绘制之后，避免边框被内容覆盖。
 * @param bounds 弹出内容矩形
 * @param theme 取色主题
 */
void PaintPopupBorder(Canvas& canvas, core::Rect bounds, const core::DuiTheme& theme);

} // namespace ysDui::render
