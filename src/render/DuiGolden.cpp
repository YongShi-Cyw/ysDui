/**
 * 文件名：DuiGolden.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现平台无关的基准图像比较。
 */
#include "ysDui/render/DuiGolden.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace ysDui::render {
namespace {
int Delta(const std::uint8_t* left, const std::uint8_t* right)
{
    return (std::max)({std::abs(static_cast<int>(left[0]) - right[0]),
                       std::abs(static_cast<int>(left[1]) - right[1]),
                       std::abs(static_cast<int>(left[2]) - right[2])});
}
}

DuiGoldenDiff CompareGolden(const DuiPixelBuffer& actual, const DuiPixelBuffer& golden, int tolerance)
{
    DuiGoldenDiff result;
    if (actual.Size() != golden.Size() || actual.Empty() || golden.Empty()) {
        result.sizeMismatch = true;
        return result;
    }
    result.totalPixels = actual.Size().width * actual.Size().height;
    tolerance = (std::max)(0, tolerance);
    const auto& left = actual.Bytes();
    const auto& right = golden.Bytes();
    for (std::size_t offset = 0; offset < left.size(); offset += 4) {
        const int delta = Delta(left.data() + offset, right.data() + offset);
        result.maximumChannelDelta = (std::max)(result.maximumChannelDelta, delta);
        if (delta > tolerance)
            ++result.differingPixels;
    }
    return result;
}

DuiPixelBuffer CreateGoldenDiff(const DuiPixelBuffer& actual, const DuiPixelBuffer& golden, int tolerance)
{
    const DuiGoldenDiff summary = CompareGolden(actual, golden, tolerance);
    if (summary.sizeMismatch)
        return {};
    tolerance = (std::max)(0, tolerance);
    std::vector<std::uint8_t> pixels(actual.Bytes().size(), 255);
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        if (Delta(actual.Bytes().data() + offset, golden.Bytes().data() + offset) > tolerance) {
            pixels[offset] = 0;
            pixels[offset + 1] = 0;
        }
    }
    return DuiPixelBuffer::Create(actual.Size(), std::move(pixels));
}
} // namespace ysDui::render
