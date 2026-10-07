#pragma once

#include <array>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiCanvas.hpp"

namespace ysDui::render {

struct DuiNinePatchInsets final {
    int left{};
    int top{};
    int right{};
    int bottom{};
    constexpr bool operator==(const DuiNinePatchInsets&) const = default;
};

struct DuiNinePatchCell final {
    core::Rect source;
    core::Rect destination;
    constexpr bool operator==(const DuiNinePatchCell&) const = default;
};

using DuiNinePatchCells = std::array<DuiNinePatchCell, 9>;

[[nodiscard]] DuiNinePatchInsets ClampNinePatchInsets(core::Size source, DuiNinePatchInsets insets);
[[nodiscard]] DuiNinePatchCells ComputeNinePatchCells(core::Size source, core::Rect destination, DuiNinePatchInsets insets);
[[nodiscard]] DuiNinePatchCells ComputeNinePatchCells(core::Size source, core::Rect destination,
                                                       DuiNinePatchInsets sourceInsets,
                                                       DuiNinePatchInsets destinationInsets);
void DrawNinePatch(Canvas& canvas, const DuiImage& image, core::Rect destination, DuiNinePatchInsets insets);
void DrawNinePatch(Canvas& canvas, const DuiImage& image, core::Rect destination,
                   DuiNinePatchInsets sourceInsets, DuiNinePatchInsets destinationInsets);

} // namespace ysDui::render
