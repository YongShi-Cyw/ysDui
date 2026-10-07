#pragma once

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiCanvas.hpp"

namespace ysDui::render {

class DuiRenderable {
public:
    virtual ~DuiRenderable() = default;
    virtual void Paint(Canvas& canvas, core::Rect dirty) const = 0;
};

} // namespace ysDui::render
