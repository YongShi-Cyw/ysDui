/**
 * 文件名：DuiWatermark.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：实现水印控件的属性存取与矩形/六边形平铺绘制（含旋转瓦片烘焙）。
 */
#include "ysDui/controls/basic/DuiWatermark.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ysDui::controls::basic {
namespace {

/** 相邻水印块的默认水平与垂直间距（DIP）。 */
constexpr int kDefaultGap = 100;
/** 水印块内文字行之间、文字与图片之间的默认行间距（DIP）。 */
constexpr int kDefaultLineSpace = 16;
/** 默认旋转角（度），对齐参考实现。 */
constexpr int kDefaultRotate = -22;
/** 角度转弧度。 */
constexpr double kPi = 3.14159265358979323846;

/** 按整体不透明度缩放单个颜色通道，结果截断到 [0,255]。 */
unsigned char ScaleChannel(unsigned char channel, double alpha)
{
    const double scaled = static_cast<double>(channel) * alpha;
    return static_cast<unsigned char>((std::clamp)(std::lround(scaled), 0L, 255L));
}

} // namespace

class DuiWatermark::Impl
{
public:
    /** 依据 alpha 与 isGrayScale 重建绘制用图片缓存，避免每帧重复生成位图。 */
    void RefreshImage()
    {
        derivedImage.reset();
        InvalidateTileCache();
        if (alpha <= 0.0 || image.image == nullptr || image.image->Empty())
            return;

        std::shared_ptr<const render::DuiImage> derived = image.image;
        if (image.isGrayScale)
        {
            std::shared_ptr<const render::DuiImage> gray = render::DuiImage::CreateGrayscale(*derived);
            if (gray != nullptr)
                derived = std::move(gray);
        }
        if (alpha < 1.0)
        {
            std::shared_ptr<const render::DuiImage> dimmed = render::DuiImage::CreateWithOpacity(
                *derived, static_cast<int>(std::lround(alpha * 255.0)));
            if (dimmed != nullptr)
                derived = std::move(dimmed);
        }
        derivedImage = std::move(derived);
    }

    void InvalidateTileCache() const
    {
        rotatedTile.reset();
        cacheUprightSize = {};
        cacheRotate = 0;
    }

    double alpha{1.0};
    std::vector<DuiWatermarkText> texts;
    DuiWatermarkImage image;
    DuiWatermarkLayout layout{DuiWatermarkLayout::Rectangular};
    bool repeat{true};
    core::Size gap{kDefaultGap, kDefaultGap};
    core::Point offset{};
    bool offsetOverride{};
    core::Size contentSize{};
    int lineSpace{kDefaultLineSpace};
    int rotate{kDefaultRotate};
    std::shared_ptr<const render::DuiImage> derivedImage;
    // 旋转瓦片缓存（Paint 为 const，故 mutable）
    mutable std::shared_ptr<const render::DuiImage> rotatedTile;
    mutable core::Size cacheUprightSize{};
    mutable int cacheRotate{};
};

DuiWatermark::DuiWatermark() : watermark_(std::make_unique<Impl>()) {}
DuiWatermark::~DuiWatermark() = default;
DuiWatermark::DuiWatermark(DuiWatermark&&) noexcept = default;
DuiWatermark& DuiWatermark::operator=(DuiWatermark&&) noexcept = default;

void DuiWatermark::SetAlpha(double alpha)
{
    watermark_->alpha = (std::clamp)(alpha, 0.0, 1.0);
    watermark_->RefreshImage();
}
double DuiWatermark::Alpha() const { return watermark_->alpha; }

void DuiWatermark::SetText(DuiWatermarkText text)
{
    watermark_->texts.assign(1, std::move(text));
    watermark_->InvalidateTileCache();
}
void DuiWatermark::SetTexts(std::vector<DuiWatermarkText> texts)
{
    watermark_->texts = std::move(texts);
    watermark_->InvalidateTileCache();
}
const std::vector<DuiWatermarkText>& DuiWatermark::Texts() const { return watermark_->texts; }

void DuiWatermark::SetImage(DuiWatermarkImage image)
{
    watermark_->image = std::move(image);
    watermark_->RefreshImage();
}
const DuiWatermarkImage& DuiWatermark::Image() const { return watermark_->image; }

void DuiWatermark::SetTileLayout(DuiWatermarkLayout layout) { watermark_->layout = layout; }
DuiWatermarkLayout DuiWatermark::TileLayout() const { return watermark_->layout; }

void DuiWatermark::SetRepeat(bool repeat) { watermark_->repeat = repeat; }
bool DuiWatermark::Repeat() const { return watermark_->repeat; }

void DuiWatermark::SetGap(core::Size gap)
{
    watermark_->gap = {(std::max)(0, gap.width), (std::max)(0, gap.height)};
}
core::Size DuiWatermark::Gap() const { return watermark_->gap; }

void DuiWatermark::SetOffset(core::Point offset)
{
    watermark_->offset = {(std::max)(0, offset.x), (std::max)(0, offset.y)};
    watermark_->offsetOverride = true;
}
core::Point DuiWatermark::Offset() const
{
    if (watermark_->offsetOverride)
        return watermark_->offset;
    return {watermark_->gap.width / 2, watermark_->gap.height / 2};
}

void DuiWatermark::SetContentSize(core::Size size)
{
    watermark_->contentSize = {(std::max)(0, size.width), (std::max)(0, size.height)};
    watermark_->InvalidateTileCache();
}
core::Size DuiWatermark::ContentSize() const { return watermark_->contentSize; }

void DuiWatermark::SetLineSpace(int pixels)
{
    watermark_->lineSpace = (std::max)(0, pixels);
    watermark_->InvalidateTileCache();
}
int DuiWatermark::LineSpace() const { return watermark_->lineSpace; }

void DuiWatermark::SetRotate(int degrees)
{
    if (watermark_->rotate == degrees)
        return;
    watermark_->rotate = degrees;
    watermark_->InvalidateTileCache();
}
int DuiWatermark::Rotate() const { return watermark_->rotate; }

core::Control* DuiWatermark::HitTest(core::Point)
{
    // 覆盖层不参与命中，避免遮挡下层内容的指针输入
    return nullptr;
}

void DuiWatermark::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::Rect bounds = Bounds();
    if (bounds.Empty())
        return;

    // 单条文字水印的测量结果：文本指针、绘制样式与实测尺寸。
    struct TileLine final
    {
        const std::string* text{};
        render::DuiTextStyle style;
        core::Size size{};
    };

    std::vector<TileLine> lines;
    lines.reserve(watermark_->texts.size());
    int maxFontAlpha = 0;
    for (const DuiWatermarkText& item : watermark_->texts)
    {
        if (item.text.empty())
            continue;
        TileLine line;
        line.text = &item.text;
        line.style.color = item.fontColor;
        line.style.color.alpha = ScaleChannel(item.fontColor.alpha, watermark_->alpha);
        maxFontAlpha = (std::max)(maxFontAlpha, static_cast<int>(line.style.color.alpha));
        line.style.family = item.fontFamily;
        line.style.pointSize = (std::max)(1, item.pointSize);
        line.style.bold = item.bold;
        line.size = canvas.MeasureText(item.text, line.style, {}).size;
        lines.push_back(std::move(line));
    }

    const render::DuiImage* image = watermark_->derivedImage.get();
    const core::Size imageSize = image != nullptr ? image->Size() : core::Size{};

    // 单块水印的内容尺寸：文字行自上而下堆叠，图片接在最后一行下方。
    int contentWidth{};
    int contentHeight{};
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        if (index > 0)
            contentHeight += watermark_->lineSpace;
        contentHeight += lines[index].size.height;
        contentWidth = (std::max)(contentWidth, lines[index].size.width);
    }
    if (!imageSize.Empty())
    {
        if (!lines.empty())
            contentHeight += watermark_->lineSpace;
        contentHeight += imageSize.height;
        contentWidth = (std::max)(contentWidth, imageSize.width);
    }
    if (contentWidth <= 0 || contentHeight <= 0)
        return;

    // 单块水印的直立绘制尺寸：显式内容尺寸优先，否则按实测内容自适应。
    const core::Size uprightSize{
        watermark_->contentSize.width > 0 ? watermark_->contentSize.width : contentWidth,
        watermark_->contentSize.height > 0 ? watermark_->contentSize.height : contentHeight};

    /** 在直立瓦片矩形内居中绘制文字行与图片。 */
    const auto paintUprightTile = [&](render::Canvas& target, int left, int top, core::Size tileSize,
                                      bool opaqueText)
    {
        int y = top + (tileSize.height - contentHeight) / 2;
        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            render::DuiTextStyle style = lines[index].style;
            if (opaqueText)
                style.color.alpha = 255;
            target.DrawText(*lines[index].text, {left, y, left + tileSize.width, y + lines[index].size.height},
                            style, render::DuiTextAlignment::Center, false);
            y += lines[index].size.height;
            if (index + 1 < lines.size())
                y += watermark_->lineSpace;
        }
        if (image != nullptr)
        {
            if (!lines.empty())
                y += watermark_->lineSpace;
            const int imageLeft = left + (tileSize.width - imageSize.width) / 2;
            target.DrawImage(*image, {imageLeft, y, imageLeft + imageSize.width, y + imageSize.height});
        }
    };

    // 需要旋转时：离屏烘焙直立瓦片 → 抠白底 → 旋转；纯图片可直接旋转派生图。
    const render::DuiImage* paintImage = nullptr;
    core::Size paintSize = uprightSize;
    const bool wantRotate = watermark_->rotate != 0;
    if (wantRotate)
    {
        if (watermark_->rotatedTile == nullptr || watermark_->cacheUprightSize != uprightSize
            || watermark_->cacheRotate != watermark_->rotate)
        {
            watermark_->rotatedTile.reset();
            const double radians =
                static_cast<double>(watermark_->rotate) * kPi / 180.0;
            std::shared_ptr<const render::DuiImage> upright;
            if (lines.empty() && image != nullptr)
            {
                // 纯图片：无需白底烘焙，直接旋转已带透明度的派生图
                if (uprightSize.width == imageSize.width && uprightSize.height == imageSize.height)
                    upright = watermark_->derivedImage;
                else
                {
                    upright = canvas.Rasterize(uprightSize, [&](render::Canvas& tileCanvas, core::Rect)
                    {
                        paintUprightTile(tileCanvas, 0, 0, uprightSize, false);
                    });
                }
            }
            else
            {
                // 含文字：白底不透明栅格化，再抠白得到透明预乘图
                auto baked = canvas.Rasterize(uprightSize, [&](render::Canvas& tileCanvas, core::Rect bounds)
                {
                    tileCanvas.FillRect(bounds, {255, 255, 255, 255});
                    paintUprightTile(tileCanvas, 0, 0, uprightSize, true);
                });
                if (baked != nullptr)
                {
                    upright = render::DuiImage::CreateFromWhiteBackground(*baked);
                    // 文字 alpha 在烘焙时被拉满，这里用各行最大有效 alpha 还原整体淡度
                    if (upright != nullptr && maxFontAlpha > 0 && maxFontAlpha < 255)
                        upright = render::DuiImage::CreateWithOpacity(*upright, maxFontAlpha);
                }
            }
            if (upright != nullptr && !upright->Empty())
            {
                watermark_->rotatedTile = render::DuiImage::CreateRotatedClockwise(*upright, radians);
                watermark_->cacheUprightSize = uprightSize;
                watermark_->cacheRotate = watermark_->rotate;
            }
        }
        if (watermark_->rotatedTile != nullptr && !watermark_->rotatedTile->Empty())
        {
            paintImage = watermark_->rotatedTile.get();
            paintSize = paintImage->Size();
        }
    }

    const core::Size gap{(std::max)(0, watermark_->gap.width), (std::max)(0, watermark_->gap.height)};
    const int stepX = paintSize.width + gap.width;
    const int stepY = paintSize.height + gap.height;
    if (stepX <= 0 || stepY <= 0)
        return;

    const core::Point offset = Offset();
    const bool useRotatedImage = paintImage != nullptr;

    /** 绘制一块最终瓦片（旋转图或直立矢量）。 */
    const auto paintTile = [&](int left, int top)
    {
        if (useRotatedImage)
        {
            canvas.DrawImage(*paintImage, {left, top, left + paintSize.width, top + paintSize.height});
            return;
        }
        paintUprightTile(canvas, left, top, uprightSize, false);
    };

    // 水印块可能有一圈落在 Bounds 之外，裁剪后再绘制，避免覆盖相邻控件。
    canvas.PushClip(bounds);

    if (!watermark_->repeat)
    {
        paintTile(bounds.left + offset.x, bounds.top + offset.y);
    }
    else
    {
        // 网格相对 Bounds 起点按 offset 对齐，并回退到不超过左上边缘的位置，保证四边被覆盖。
        const int firstLeft = bounds.left + offset.x - ((offset.x + stepX - 1) / stepX) * stepX;
        const int firstTop = bounds.top + offset.y - ((offset.y + stepY - 1) / stepY) * stepY;
        int row{};
        for (int top = firstTop; top < bounds.bottom; top += stepY, ++row)
        {
            // 六边形排布：奇数行整体右移半个水平步长，错开相邻行。
            const int rowShift = watermark_->layout == DuiWatermarkLayout::Hexagonal && (row % 2) != 0
                ? stepX / 2
                : 0;
            for (int left = firstLeft; left < bounds.right; left += stepX)
            {
                const core::Rect tile{left + rowShift, top, left + rowShift + paintSize.width,
                                      top + paintSize.height};
                if (core::Rect::Intersect(tile, dirty).Empty())
                    continue;
                paintTile(tile.left, tile.top);
            }
        }
    }

    canvas.PopClip();
}

} // namespace ysDui::controls::basic
