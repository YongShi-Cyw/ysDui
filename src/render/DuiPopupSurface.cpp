/**
 * 文件名：DuiPopupSurface.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：弹出层底色与边框的绘制实现。
 */
#include "ysDui/render/DuiPopupSurface.hpp"

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"

namespace ysDui::render {

void PaintPopupBackground(Canvas& canvas, core::Rect bounds, const core::DuiTheme& theme)
{
    if (bounds.Empty())
        return;
    canvas.FillRect(bounds, theme.Get(core::ThemeSlot::SurfaceBackground));
}

void PaintPopupBorder(Canvas& canvas, core::Rect bounds, const core::DuiTheme& theme)
{
    if (bounds.Width() <= 0 || bounds.Height() <= 0)
        return;
    const core::Color border = theme.Get(core::ThemeSlot::PopupBorder);
    // 四边分别填充：直角处保持 1 像素实边，不受圆角描边的抗锯齿影响
    canvas.FillRect({bounds.left, bounds.top, bounds.right, bounds.top + 1}, border);
    canvas.FillRect({bounds.left, bounds.bottom - 1, bounds.right, bounds.bottom}, border);
    canvas.FillRect({bounds.left, bounds.top + 1, bounds.left + 1, bounds.bottom - 1}, border);
    canvas.FillRect({bounds.right - 1, bounds.top + 1, bounds.right, bounds.bottom - 1}, border);
}

} // namespace ysDui::render
