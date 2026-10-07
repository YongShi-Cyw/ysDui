#pragma once

#include <algorithm>

namespace ysDui::core {

/** 核心与渲染公开几何统一使用 96 DPI 下的逻辑像素（DIP）。 */
struct Point final {
    int x{};
    int y{};
    constexpr bool operator==(const Point&) const = default;
};

struct Size final {
    int width{};
    int height{};
    constexpr bool Empty() const { return width <= 0 || height <= 0; }
    constexpr bool operator==(const Size&) const = default;
};

struct Rect final {
    int left{};
    int top{};
    int right{};
    int bottom{};

    constexpr int Width() const { return right - left; }
    constexpr int Height() const { return bottom - top; }
    constexpr bool Empty() const { return Width() <= 0 || Height() <= 0; }
    constexpr bool Contains(Point point) const {
        return point.x >= left && point.x < right && point.y >= top && point.y < bottom;
    }
    constexpr bool operator==(const Rect&) const = default;

    static constexpr Rect Intersect(Rect first, Rect second) {
        const Rect result{std::max(first.left, second.left), std::max(first.top, second.top),
                          std::min(first.right, second.right), std::min(first.bottom, second.bottom)};
        return result.Empty() ? Rect{} : result;
    }
};

struct Color final {
    unsigned char red{};
    unsigned char green{};
    unsigned char blue{};
    unsigned char alpha{255};
    constexpr bool operator==(const Color&) const = default;
};

} // namespace ysDui::core
