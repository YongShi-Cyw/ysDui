#include "ysDui/render/DuiImage.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ysDui::render {
class DuiImage::Impl {
public:
    core::Size size;
    DuiImageFormat format{DuiImageFormat::Bgra8Premultiplied};
    std::vector<unsigned char> pixels;
};
DuiImage::DuiImage() : image_(std::make_unique<Impl>()) {}
DuiImage::DuiImage(core::Size size, DuiImageFormat format, std::vector<unsigned char> pixels) : image_(std::make_unique<Impl>()) {
    image_->size = size;
    image_->format = format;
    image_->pixels = std::move(pixels);
}
DuiImage::~DuiImage() = default;
DuiImage::DuiImage(DuiImage&&) noexcept = default;
DuiImage& DuiImage::operator=(DuiImage&&) noexcept = default;
std::shared_ptr<const DuiImage> DuiImage::CreateBgra8Premultiplied(
    core::Size size, std::vector<unsigned char> pixels) {
    if (size.Empty() || size.width > (std::numeric_limits<int>::max)() / 4
        || pixels.size() != static_cast<std::size_t>(size.width) * size.height * 4) {
        return {};
    }
    return std::shared_ptr<const DuiImage>(new DuiImage(size, DuiImageFormat::Bgra8Premultiplied,
                                                        std::move(pixels)));
}
std::shared_ptr<const DuiImage> DuiImage::CreateGrayscale(const DuiImage& image) {
    if (image.Empty() || image.Format() != DuiImageFormat::Bgra8Premultiplied)
        return {};
    std::vector<unsigned char> pixels = image.Pixels();
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        const unsigned int luma = 29U * pixels[offset] + 150U * pixels[offset + 1]
                                  + 77U * pixels[offset + 2] + 128U;
        const unsigned char gray = static_cast<unsigned char>(luma >> 8U);
        pixels[offset] = gray;
        pixels[offset + 1] = gray;
        pixels[offset + 2] = gray;
    }
    return CreateBgra8Premultiplied(image.Size(), std::move(pixels));
}

std::shared_ptr<const DuiImage> DuiImage::CreateWithOpacity(const DuiImage& image, int opacity)
{
    if (image.Empty() || image.Format() != DuiImageFormat::Bgra8Premultiplied)
        return {};
    if (opacity <= 0)
        return {};
    if (opacity > 255)
        opacity = 255;
    if (opacity >= 255)
        return CreateBgra8Premultiplied(image.Size(), image.Pixels());

    std::vector<unsigned char> pixels = image.Pixels();
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4)
    {
        pixels[offset] = static_cast<unsigned char>((pixels[offset] * opacity) / 255);
        pixels[offset + 1] = static_cast<unsigned char>((pixels[offset + 1] * opacity) / 255);
        pixels[offset + 2] = static_cast<unsigned char>((pixels[offset + 2] * opacity) / 255);
        pixels[offset + 3] = static_cast<unsigned char>((pixels[offset + 3] * opacity) / 255);
    }
    return CreateBgra8Premultiplied(image.Size(), std::move(pixels));
}

std::shared_ptr<const DuiImage> DuiImage::CreateFromWhiteBackground(const DuiImage& image)
{
    if (image.Empty() || image.Format() != DuiImageFormat::Bgra8Premultiplied)
        return {};

    const auto& src = image.Pixels();
    std::vector<unsigned char> dst(src.size(), 0);
    for (std::size_t offset = 0; offset + 3 < src.size(); offset += 4)
    {
        const int blue = src[offset];
        const int green = src[offset + 1];
        const int red = src[offset + 2];
        // 相对白色的最大偏离作为覆盖度：纯白→0，纯黑/饱和色→高覆盖
        const int coverage = (std::max)(255 - red, (std::max)(255 - green, 255 - blue));
        if (coverage <= 0)
            continue;
        // 从「前景 * c + 白色 * (1-c)」反解前景色
        const int fgRed = (std::clamp)((red - (255 - coverage)) * 255 / coverage, 0, 255);
        const int fgGreen = (std::clamp)((green - (255 - coverage)) * 255 / coverage, 0, 255);
        const int fgBlue = (std::clamp)((blue - (255 - coverage)) * 255 / coverage, 0, 255);
        dst[offset] = static_cast<unsigned char>((fgBlue * coverage) / 255);
        dst[offset + 1] = static_cast<unsigned char>((fgGreen * coverage) / 255);
        dst[offset + 2] = static_cast<unsigned char>((fgRed * coverage) / 255);
        dst[offset + 3] = static_cast<unsigned char>(coverage);
    }
    return CreateBgra8Premultiplied(image.Size(), std::move(dst));
}

std::shared_ptr<const DuiImage> DuiImage::CreateRotatedClockwise(const DuiImage& image,
                                                                double angleCwRadians)
{
    if (image.Empty() || image.Format() != DuiImageFormat::Bgra8Premultiplied)
        return {};
    if (std::abs(angleCwRadians) < 1e-9)
        return CreateBgra8Premultiplied(image.Size(), image.Pixels());

    const int srcW = image.Size().width;
    const int srcH = image.Size().height;
    const double cosA = std::cos(angleCwRadians);
    const double sinA = std::sin(angleCwRadians);
    const double srcCx = (srcW - 1) * 0.5;
    const double srcCy = (srcH - 1) * 0.5;

    // 屏幕 y 向下时，绕中心顺时针（对齐 GDI+ 正角=顺时针）：
    // x' = cx + dx*cos - dy*sin，y' = cy + dx*sin + dy*cos
    auto mapForward = [&](double x, double y) {
        const double dx = x - srcCx;
        const double dy = y - srcCy;
        return std::pair<double, double>{srcCx + dx * cosA - dy * sinA,
                                         srcCy + dx * sinA + dy * cosA};
    };

    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    bool first = true;
    const double corners[4][2] = {{0.0, 0.0},
                                  {static_cast<double>(srcW - 1), 0.0},
                                  {static_cast<double>(srcW - 1), static_cast<double>(srcH - 1)},
                                  {0.0, static_cast<double>(srcH - 1)}};
    for (const auto& corner : corners)
    {
        const auto p = mapForward(corner[0], corner[1]);
        if (first)
        {
            minX = maxX = p.first;
            minY = maxY = p.second;
            first = false;
        }
        else
        {
            minX = (std::min)(minX, p.first);
            maxX = (std::max)(maxX, p.first);
            minY = (std::min)(minY, p.second);
            maxY = (std::max)(maxY, p.second);
        }
    }

    const int dstW = (std::max)(1, static_cast<int>(std::ceil(maxX - minX)) + 1);
    const int dstH = (std::max)(1, static_cast<int>(std::ceil(maxY - minY)) + 1);
    const double dstCx = (dstW - 1) * 0.5;
    const double dstCy = (dstH - 1) * 0.5;
    const auto& srcPixels = image.Pixels();
    std::vector<unsigned char> dst(static_cast<std::size_t>(dstW) * dstH * 4, 0);

    for (int y = 0; y < dstH; ++y)
    {
        for (int x = 0; x < dstW; ++x)
        {
            const double dx = static_cast<double>(x) - dstCx;
            const double dy = static_cast<double>(y) - dstCy;
            // 逆变换（顺时针的逆=逆时针）：x = cx + dx*cos + dy*sin，y = cy - dx*sin + dy*cos
            const double srcX = srcCx + dx * cosA + dy * sinA;
            const double srcY = srcCy - dx * sinA + dy * cosA;
            const int ix = static_cast<int>(std::lround(srcX));
            const int iy = static_cast<int>(std::lround(srcY));
            if (ix < 0 || iy < 0 || ix >= srcW || iy >= srcH)
                continue;
            const std::size_t srcOff =
                (static_cast<std::size_t>(iy) * srcW + static_cast<std::size_t>(ix)) * 4;
            const std::size_t dstOff =
                (static_cast<std::size_t>(y) * dstW + static_cast<std::size_t>(x)) * 4;
            dst[dstOff] = srcPixels[srcOff];
            dst[dstOff + 1] = srcPixels[srcOff + 1];
            dst[dstOff + 2] = srcPixels[srcOff + 2];
            dst[dstOff + 3] = srcPixels[srcOff + 3];
        }
    }
    return CreateBgra8Premultiplied({dstW, dstH}, std::move(dst));
}

core::Size DuiImage::Size() const { return image_->size; }
DuiImageFormat DuiImage::Format() const { return image_->format; }
bool DuiImage::Empty() const { return image_->size.Empty() || image_->pixels.empty(); }
const std::vector<unsigned char>& DuiImage::Pixels() const { return image_->pixels; }
} // namespace ysDui::render
