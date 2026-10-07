#include "ysDui/controls/media/DuiImage.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ysDui::controls::media {
namespace {
constexpr core::Size DefaultDesiredSize{64, 64};
}

class DuiImage::Impl {
public:
    std::shared_ptr<const render::DuiImage> value;
    DuiImageScaleMode scaleMode{DuiImageScaleMode::Fit};
};

DuiImage::DuiImage() : image_(std::make_unique<Impl>()) {}
DuiImage::~DuiImage() = default;
DuiImage::DuiImage(DuiImage&&) noexcept = default;
DuiImage& DuiImage::operator=(DuiImage&&) noexcept = default;

void DuiImage::SetImage(std::shared_ptr<const render::DuiImage> image) {
    image_->value = std::move(image);
}

const std::shared_ptr<const render::DuiImage>& DuiImage::Image() const {
    return image_->value;
}

void DuiImage::SetScaleMode(DuiImageScaleMode mode) {
    image_->scaleMode = mode;
}

DuiImageScaleMode DuiImage::ScaleMode() const {
    return image_->scaleMode;
}

core::Size DuiImage::DesiredSize() const {
    if (image_->scaleMode == DuiImageScaleMode::None && image_->value && !image_->value->Empty()) {
        return image_->value->Size();
    }
    return DefaultDesiredSize;
}

DuiImageDrawRects DuiImage::ComputeDrawRects(DuiImageScaleMode mode, core::Size imageSize,
                                              core::Rect destination) {
    DuiImageDrawRects result{{0, 0, imageSize.width, imageSize.height}, destination};
    if (imageSize.Empty() || destination.Empty()) {
        return result;
    }

    const int destinationWidth = destination.Width();
    const int destinationHeight = destination.Height();
    switch (mode) {
    case DuiImageScaleMode::None:
        result.destination.right = destination.left + (std::min)(imageSize.width, destinationWidth);
        result.destination.bottom = destination.top + (std::min)(imageSize.height, destinationHeight);
        result.source.right = result.destination.Width();
        result.source.bottom = result.destination.Height();
        break;
    case DuiImageScaleMode::Stretch:
        break;
    case DuiImageScaleMode::Fit: {
        const double scale = (std::min)(static_cast<double>(destinationWidth) / imageSize.width,
                                        static_cast<double>(destinationHeight) / imageSize.height);
        const int width = static_cast<int>(std::lround(imageSize.width * scale));
        const int height = static_cast<int>(std::lround(imageSize.height * scale));
        result.destination.left = destination.left + (destinationWidth - width) / 2;
        result.destination.top = destination.top + (destinationHeight - height) / 2;
        result.destination.right = result.destination.left + width;
        result.destination.bottom = result.destination.top + height;
        break;
    }
    case DuiImageScaleMode::Fill: {
        const double scale = (std::max)(static_cast<double>(destinationWidth) / imageSize.width,
                                        static_cast<double>(destinationHeight) / imageSize.height);
        const int cropWidth = (std::min)(imageSize.width,
                                         static_cast<int>(std::lround(destinationWidth / scale)));
        const int cropHeight = (std::min)(imageSize.height,
                                          static_cast<int>(std::lround(destinationHeight / scale)));
        result.source.left = (imageSize.width - cropWidth) / 2;
        result.source.top = (imageSize.height - cropHeight) / 2;
        result.source.right = result.source.left + cropWidth;
        result.source.bottom = result.source.top + cropHeight;
        break;
    }
    }
    return result;
}

void DuiImage::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible() || !image_->value || image_->value->Empty()) {
        return;
    }
    const core::Rect clip = core::Rect::Intersect(Bounds(), dirty);
    if (clip.Empty()) {
        return;
    }
    const DuiImageDrawRects rects = ComputeDrawRects(image_->scaleMode, image_->value->Size(), Bounds());
    if (rects.source.Empty() || rects.destination.Empty()) {
        return;
    }
    canvas.PushClip(clip);
    canvas.DrawImage(*image_->value, rects.source, rects.destination);
    canvas.PopClip();
}

} // namespace ysDui::controls::media
