#include "ysDui/render/DuiFocusVisual.hpp"

#include <algorithm>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"

namespace ysDui::render {

void DrawFocusRing(Canvas& canvas, core::Rect bounds, core::Color color, int thickness, bool withInset)
{
    thickness = (std::max)(1, thickness);
    if (bounds.Empty())
        return;
    canvas.StrokeRoundedRect(bounds, 0, color, static_cast<float>(thickness));
    if (withInset && bounds.Width() > 2 && bounds.Height() > 2)
        canvas.StrokeRoundedRect({bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.bottom - 1}, 0,
                                 {255, 255, 255, 255}, 1.0F);
}

void DrawThemedFocusRing(Canvas& canvas, core::Rect bounds, const core::DuiTheme& theme, int thickness, bool withInset)
{
    // 焦点线框由主题开关控制，默认不绘制（见 DuiTheme::SetFocusRingVisible）
    if (!theme.FocusRingVisible())
        return;
    DrawFocusRing(canvas, bounds, theme.Get(core::ThemeSlot::BrandPrimary), thickness, withInset);
}

} // namespace ysDui::render
