#pragma once

#include <string_view>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::render {

struct DuiTextMetrics final {
    core::Size size{};
    int lineCount{};
    int baseline{};
};

struct DuiTextMeasureOptions final {
    int maximumWidth{};
    bool wordWrap{};
};

} // namespace ysDui::render
