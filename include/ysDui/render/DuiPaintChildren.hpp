#pragma once

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::render {

inline void PaintChildren(const core::Control& parent, Canvas& canvas, core::Rect dirty) {
    for (const auto& child : parent.Children()) {
        if (!child->EffectivelyVisible())
            continue;
        if (const auto* renderable = dynamic_cast<const DuiRenderable*>(child.get()))
            renderable->Paint(canvas, dirty);
    }
}

} // namespace ysDui::render
