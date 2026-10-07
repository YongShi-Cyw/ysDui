/**
 * 文件名：DuiImageAccess.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明渲染模块私有的像素图像访问桥接，隔离平台后端与公开图像类型。
 */
#pragma once

#include <vector>

#include "ysDui/render/DuiAnimatedImage.hpp"
#include "ysDui/render/DuiImage.hpp"

namespace ysDui::render {

/**
 * 像素图像内部访问器。
 * 用途：供渲染实现和平台后端构造、读取公开图像的私有像素数据。
 */
class DuiImageAccess final
{
public:
    /** 根据预乘 BGRA 像素创建图像。 */
    [[nodiscard]] static DuiImage Create(core::Size size, DuiImageFormat format,
        std::vector<unsigned char> pixels);

    /** 返回图像的只读像素数据。 */
    [[nodiscard]] static const std::vector<unsigned char>& Pixels(const DuiImage& image);

    /** 根据帧和延迟创建动画图像。 */
    [[nodiscard]] static DuiAnimatedImage CreateAnimated(std::vector<DuiImage> frames,
        std::vector<int> delays);
};

} // namespace ysDui::render
