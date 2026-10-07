/**
 * 文件名：DuiImageDecoder.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明 Win32 后端对 PNG/JPEG/GIF/BMP 等编码图像的解码入口，供控件 SetImage 显示。
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiAnimatedImage.hpp"

namespace ysDui::platform::win32 {

/**
 * 图像解码器。
 * 用途：将磁盘文件或内存中的编码图像转为 ysDui 可绘制的预乘 BGRA 图像。
 * 典型用法：配合 controls::media::DuiImage::SetImage 显示；
 * NX 侧可用 UGUI_load_high_quality_bitmap 取出 BMP 字节后再调用 LoadSharedBytes。
 */
class DuiImageDecoder final {
public:
    /**
     * 从磁盘文件解码单帧图像。
     * @param path UTF-8 文件路径。
     * @return 解码成功时返回图像，否则返回空值。
     */
    [[nodiscard]] static std::optional<render::DuiImage> DecodeFile(std::string_view path);
    /**
     * 从内存中的编码图像数据解码单帧图像。
     * @param bytes PNG、JPEG、GIF 或 BMP 的完整编码字节。
     * @return 解码成功时返回预乘 BGRA 图像，否则返回空值。
     */
    [[nodiscard]] static std::optional<render::DuiImage> DecodeBytes(std::span<const std::uint8_t> bytes);
    /**
     * 从磁盘文件解码并包装为可共享图像（供 SetImage 显示）。
     * @param path UTF-8 文件路径。
     * @return 成功返回非空 shared_ptr，失败返回空指针。
     */
    [[nodiscard]] static std::shared_ptr<const render::DuiImage> LoadSharedFile(std::string_view path);
    /**
     * 从编码字节解码并包装为可共享图像（供 SetImage 显示）。
     * @param bytes PNG、JPEG、GIF 或 BMP 的完整编码字节（含 UGUI 返回的 BMP 缓冲）。
     * @return 成功返回非空 shared_ptr，失败返回空指针。
     */
    [[nodiscard]] static std::shared_ptr<const render::DuiImage> LoadSharedBytes(
        std::span<const std::uint8_t> bytes);
    /**
     * 从编码字节解码，并将近白像素（NX BMP 常用白底）置为全透明。
     * @param bytes PNG/JPEG/GIF/BMP 完整编码字节（含 UGUI 返回缓冲）
     * @param whiteThreshold RGB 均 ≥ 该值时视为透明背景，默认 245
     * @return 成功返回非空 shared_ptr，失败返回空指针
     */
    [[nodiscard]] static std::shared_ptr<const render::DuiImage> LoadSharedBytesNearWhiteTransparent(
        std::span<const std::uint8_t> bytes, int whiteThreshold = 245);
    [[nodiscard]] static std::optional<render::DuiAnimatedImage> DecodeAnimatedFile(
        std::string_view path);
    /**
     * 从内存中的编码图像数据解码动画图像。
     * @param bytes GIF 等多帧图像的完整编码字节。
     * @return 解码成功时返回动画帧，否则返回空值。
     */
    [[nodiscard]] static std::optional<render::DuiAnimatedImage> DecodeAnimatedBytes(
        std::span<const std::uint8_t> bytes);
};

} // namespace ysDui::platform::win32
