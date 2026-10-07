#pragma once

#include <vector>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::render {

enum class DuiPathCommandType { MoveTo, LineTo, Close };

struct DuiPathCommand final {
    DuiPathCommandType type{};
    core::Point point{};
};

class DuiPath final {
public:
    void MoveTo(core::Point point) { commands_.push_back({DuiPathCommandType::MoveTo, point}); }
    void LineTo(core::Point point) { commands_.push_back({DuiPathCommandType::LineTo, point}); }
    void Close() { commands_.push_back({DuiPathCommandType::Close, {}}); }
    [[nodiscard]] const std::vector<DuiPathCommand>& Commands() const { return commands_; }

private:
    std::vector<DuiPathCommand> commands_;
};

} // namespace ysDui::render
