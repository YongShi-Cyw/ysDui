/**
 * 文件名：DuiGolden.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的渲染基准图像比较接口。
 */
#pragma once

#include "ysDui/render/DuiPixelBuffer.hpp"

namespace ysDui::render {

/** 基准图像比较统计数据。 */
struct DuiGoldenDiff final
{
    int differingPixels{};
    int totalPixels{};
    int maximumChannelDelta{};
    bool sizeMismatch{};
};

/**
 * 比较两张像素图像。
 * @param tolerance 单通道允许的最大差异，负值按零处理。
 */
[[nodiscard]] DuiGoldenDiff CompareGolden(const DuiPixelBuffer& actual,
                                          const DuiPixelBuffer& golden, int tolerance = 2);
/** 根据比较规则生成白底红色差异图。尺寸不匹配时返回空缓冲区。 */
[[nodiscard]] DuiPixelBuffer CreateGoldenDiff(const DuiPixelBuffer& actual,
                                               const DuiPixelBuffer& golden, int tolerance = 2);

} // namespace ysDui::render
