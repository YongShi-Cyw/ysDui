/**
 * 文件名：DuiPixelBuffer.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的 BGRA 像素缓冲区。
 */
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::render {

/** 像素的内存排列方式。 */
enum class DuiPixelFormat
{
    Bgra8Premultiplied,
};

/**
 * 平台无关的连续像素缓冲区。
 * 每个像素使用四个字节，行顺序为从上到下。
 */
class DuiPixelBuffer final
{
public:
    DuiPixelBuffer();
    ~DuiPixelBuffer();
    DuiPixelBuffer(const DuiPixelBuffer&) = delete;
    DuiPixelBuffer& operator=(const DuiPixelBuffer&) = delete;
    DuiPixelBuffer(DuiPixelBuffer&&) noexcept;
    DuiPixelBuffer& operator=(DuiPixelBuffer&&) noexcept;

    /**
     * 创建 BGRA 预乘像素缓冲区。
     * @param size 像素尺寸。
     * @param pixels 每像素四字节的连续数据；长度不匹配时返回空缓冲区。
     */
    [[nodiscard]] static DuiPixelBuffer Create(core::Size size, std::vector<std::uint8_t> pixels);
    [[nodiscard]] core::Size Size() const;
    [[nodiscard]] DuiPixelFormat Format() const;
    [[nodiscard]] bool Empty() const;
    [[nodiscard]] const std::vector<std::uint8_t>& Bytes() const;

private:
    class Impl;
    explicit DuiPixelBuffer(core::Size size, std::vector<std::uint8_t> pixels);
    std::unique_ptr<Impl> buffer_;
};

} // namespace ysDui::render
