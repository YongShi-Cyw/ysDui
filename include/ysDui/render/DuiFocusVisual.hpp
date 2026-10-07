#pragma once

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::core {
class DuiTheme;
}

namespace ysDui::render {
class Canvas;

void DrawFocusRing(Canvas& canvas, core::Rect bounds, core::Color color, int thickness = 2, bool withInset = true);
void DrawThemedFocusRing(Canvas& canvas, core::Rect bounds, const core::DuiTheme& theme,
                         int thickness = 2, bool withInset = true);

} // namespace ysDui::render
