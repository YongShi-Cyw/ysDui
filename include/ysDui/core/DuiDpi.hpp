#pragma once

#include <cmath>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::core {

class DuiDpiScale final {
public:
    static constexpr int DefaultDpi = 96;

    constexpr explicit DuiDpiScale(int dpi = DefaultDpi) : dpi_(dpi > 0 ? dpi : DefaultDpi) {}

    [[nodiscard]] constexpr int Dpi() const { return dpi_; }
    [[nodiscard]] constexpr double Factor() const { return static_cast<double>(dpi_) / DefaultDpi; }
    [[nodiscard]] int Scale(int logicalPixels) const {
        return static_cast<int>(std::lround(static_cast<double>(logicalPixels) * dpi_ / DefaultDpi));
    }
    [[nodiscard]] int Unscale(int devicePixels) const {
        return static_cast<int>(std::lround(static_cast<double>(devicePixels) * DefaultDpi / dpi_));
    }
    [[nodiscard]] Point Scale(Point point) const { return {Scale(point.x), Scale(point.y)}; }
    [[nodiscard]] Size Scale(Size size) const { return {Scale(size.width), Scale(size.height)}; }
    /** 四条边分别舍入，避免通过宽高累加产生坐标漂移。 */
    [[nodiscard]] Rect Scale(Rect rect) const {
        return {Scale(rect.left), Scale(rect.top), Scale(rect.right), Scale(rect.bottom)};
    }
    [[nodiscard]] Point Unscale(Point point) const { return {Unscale(point.x), Unscale(point.y)}; }
    [[nodiscard]] Size Unscale(Size size) const { return {Unscale(size.width), Unscale(size.height)}; }
    /** 四条边分别舍入，保证平台边界转换规则与 Scale(Rect) 对称。 */
    [[nodiscard]] Rect Unscale(Rect rect) const {
        return {Unscale(rect.left), Unscale(rect.top), Unscale(rect.right), Unscale(rect.bottom)};
    }
    constexpr bool operator==(const DuiDpiScale&) const = default;

private:
    int dpi_;
};

} // namespace ysDui::core
