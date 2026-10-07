#pragma once

#include <string_view>

#include "ysDui/render/DuiTextMetrics.hpp"

namespace ysDui::render {

class DuiTextMeasurer {
public:
    virtual ~DuiTextMeasurer() = default;
    [[nodiscard]] virtual DuiTextMetrics MeasureText(std::string_view text,
                                                     const DuiTextStyle& style,
                                                     const DuiTextMeasureOptions& options) = 0;
};

} // namespace ysDui::render
