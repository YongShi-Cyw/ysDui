/**
 * 文件名：DuiSvg.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的 SVG 光栅化接口。
 */
#pragma once

#include <optional>
#include <string_view>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiPixelBuffer.hpp"

namespace ysDui::render {

/**
 * 将 UTF-8 SVG 文本光栅化为预乘 BGRA 像素缓冲区。
 * @param svgUtf8 UTF-8 编码的 SVG 文本，可包含 UTF-8 BOM。
 * @param targetSize 目标像素尺寸；任一边为零时按 SVG 内在宽高等比推导。
 * @return 解析和光栅化成功时返回像素缓冲区，否则返回空值。
 */
[[nodiscard]] std::optional<DuiPixelBuffer> RasterizeSvg(std::string_view svgUtf8,
                                                          core::Size targetSize = {});

/**
 * 读取 UTF-8 路径指向的 SVG 文件并进行光栅化。
 * @param path SVG 文件的 UTF-8 路径。
 * @param targetSize 目标像素尺寸；任一边为零时按 SVG 内在宽高等比推导。
 * @return 读取、解析和光栅化成功时返回像素缓冲区，否则返回空值。
 */
[[nodiscard]] std::optional<DuiPixelBuffer> RasterizeSvgFile(std::string_view path,
                                                              core::Size targetSize = {});

} // namespace ysDui::render
