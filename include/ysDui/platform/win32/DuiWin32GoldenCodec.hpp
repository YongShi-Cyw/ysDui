/**
 * 文件名：DuiWin32GoldenCodec.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明 Win32 基准图像的 PNG 编解码能力。
 */
#pragma once

#include <optional>
#include <string_view>

#include "ysDui/render/DuiPixelBuffer.hpp"

namespace ysDui::platform::win32 {

/**
 * Win32 基准图像编解码器。
 * 用途：在 GDI+ 后端中读写平台无关的 BGRA 预乘像素缓冲。
 */
class DuiWin32GoldenCodec final
{
public:
    /**
     * 将像素缓冲保存为 PNG 文件。
     * @param pixels 要保存的 BGRA 预乘像素数据。
     * @param path 目标文件的 UTF-16 路径。
     * @return 写入成功时返回 true。
     */
    [[nodiscard]] static bool SavePng(const render::DuiPixelBuffer& pixels, std::string_view path);

    /**
     * 从 PNG 文件加载像素缓冲。
     * @param path 源文件的 UTF-16 路径。
     * @return 解码成功时返回 BGRA 预乘像素数据。
     */
    [[nodiscard]] static std::optional<render::DuiPixelBuffer> LoadPng(std::string_view path);
};

} // namespace ysDui::platform::win32
