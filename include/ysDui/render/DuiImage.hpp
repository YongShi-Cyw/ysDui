#pragma once

#include <memory>
#include <vector>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::render {

class DuiImageAccess;

enum class DuiImageFormat {
    Bgra8Premultiplied,
};

class DuiImage final {
public:
    DuiImage();
    ~DuiImage();
    DuiImage(const DuiImage&) = delete;
    DuiImage& operator=(const DuiImage&) = delete;
    DuiImage(DuiImage&&) noexcept;
    DuiImage& operator=(DuiImage&&) noexcept;
    /**
     * 从预乘 BGRA 像素构造不可变图像。
     * @param size 图像像素尺寸。
     * @param pixels 按行排列的预乘 BGRA 像素数据。
     * @return 数据尺寸有效时返回图像，否则返回空指针。
     */
    [[nodiscard]] static std::shared_ptr<const DuiImage> CreateBgra8Premultiplied(
        core::Size size, std::vector<unsigned char> pixels);
    /**
     * 创建保持尺寸和透明度的灰度图像副本。
     * @param image 要转换的 BGRA 预乘图像。
     * @return 图像无效时返回空指针，否则返回新的不可变图像。
     */
    [[nodiscard]] static std::shared_ptr<const DuiImage> CreateGrayscale(const DuiImage& image);
    /**
     * 按不透明度缩放预乘 BGRA（0=全透明，255=原样）
     * @param image 源图
     * @param opacity 不透明度 0~255
     * @return 新图；无效或 opacity<=0 返回空；opacity>=255 返回灰度无关的原图像素副本语义上等价复用 Create
     */
    [[nodiscard]] static std::shared_ptr<const DuiImage> CreateWithOpacity(const DuiImage& image,
                                                                           int opacity);
    /**
     * 绕图像中心顺时针旋转（屏幕 y 向下），输出包围盒扩大以完整容纳旋转结果。
     * @param image 源图（预乘 BGRA）
     * @param angleCwRadians 顺时针弧度
     * @return 旋转后的新图；无效输入返回空
     */
    [[nodiscard]] static std::shared_ptr<const DuiImage> CreateRotatedClockwise(
        const DuiImage& image, double angleCwRadians);
    /**
     * 把白底不透明栅格图转为透明底预乘图（水印旋转烘焙用）。
     * 近白像素变为全透明；其余像素按相对白色的偏离恢复前景色与覆盖度。
     * @param image 白底源图
     * @return 透明底预乘图；无效输入返回空
     */
    [[nodiscard]] static std::shared_ptr<const DuiImage> CreateFromWhiteBackground(
        const DuiImage& image);
    [[nodiscard]] core::Size Size() const;
    [[nodiscard]] DuiImageFormat Format() const;
    [[nodiscard]] bool Empty() const;

private:
    class Impl;
    DuiImage(core::Size size, DuiImageFormat format, std::vector<unsigned char> pixels);
    [[nodiscard]] const std::vector<unsigned char>& Pixels() const;
    friend class DuiImageAccess;
    std::unique_ptr<Impl> image_;
};

} // namespace ysDui::render
