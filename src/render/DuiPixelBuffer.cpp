/**
 * 文件名：DuiPixelBuffer.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现平台无关的像素缓冲区。
 */
#include "ysDui/render/DuiPixelBuffer.hpp"

#include <memory>
#include <utility>

namespace ysDui::render {
class DuiPixelBuffer::Impl final
{
public:
    core::Size size;
    std::vector<std::uint8_t> pixels;
};

DuiPixelBuffer::DuiPixelBuffer() : buffer_(std::make_unique<Impl>()) {}
DuiPixelBuffer::DuiPixelBuffer(core::Size size, std::vector<std::uint8_t> pixels) : DuiPixelBuffer()
{
    buffer_->size = size;
    buffer_->pixels = std::move(pixels);
}
DuiPixelBuffer::~DuiPixelBuffer() = default;
DuiPixelBuffer::DuiPixelBuffer(DuiPixelBuffer&&) noexcept = default;
DuiPixelBuffer& DuiPixelBuffer::operator=(DuiPixelBuffer&&) noexcept = default;
DuiPixelBuffer DuiPixelBuffer::Create(core::Size size, std::vector<std::uint8_t> pixels)
{
    const std::size_t required = size.Empty() ? 0 : static_cast<std::size_t>(size.width) * size.height * 4;
    return pixels.size() == required ? DuiPixelBuffer(size, std::move(pixels)) : DuiPixelBuffer();
}
core::Size DuiPixelBuffer::Size() const { return buffer_->size; }
DuiPixelFormat DuiPixelBuffer::Format() const { return DuiPixelFormat::Bgra8Premultiplied; }
bool DuiPixelBuffer::Empty() const { return buffer_->size.Empty() || buffer_->pixels.empty(); }
const std::vector<std::uint8_t>& DuiPixelBuffer::Bytes() const { return buffer_->pixels; }
} // namespace ysDui::render
