#pragma once

#include <string>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::render {

enum class DuiTextAlignment {
    Start,
    Center,
    End,
};

struct DuiTextStyle final {
    core::Color color{20, 20, 20, 255};
    std::string family;
    int pointSize{9};
    bool bold{};
    bool italic{};
    bool underline{};
};

} // namespace ysDui::render
