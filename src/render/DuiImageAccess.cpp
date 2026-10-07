/**
 * 文件名：DuiImageAccess.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现渲染模块私有的像素图像访问桥接。
 */
#include "DuiImageAccess.hpp"

#include <utility>

namespace ysDui::render {

DuiImage DuiImageAccess::Create(core::Size size, DuiImageFormat format, std::vector<unsigned char> pixels)
{
    return DuiImage(size, format, std::move(pixels));
}

const std::vector<unsigned char>& DuiImageAccess::Pixels(const DuiImage& image)
{
    return image.Pixels();
}

DuiAnimatedImage DuiImageAccess::CreateAnimated(std::vector<DuiImage> frames, std::vector<int> delays)
{
    return DuiAnimatedImage(std::move(frames), std::move(delays));
}

} // namespace ysDui::render
