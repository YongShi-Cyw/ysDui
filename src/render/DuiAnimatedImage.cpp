#include "ysDui/render/DuiAnimatedImage.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/render/DuiImage.hpp"

namespace ysDui::render {
namespace {
}

class DuiAnimatedImage::Impl {
public:
    std::vector<DuiImage> frames;
    std::vector<std::shared_ptr<const DuiImage>> sharedFrames;
    std::vector<int> delays;
};

DuiAnimatedImage::DuiAnimatedImage(std::vector<DuiImage> frames, std::vector<int> delays)
    : animatedImage_(std::make_unique<Impl>()) {
    animatedImage_->frames = std::move(frames);
    animatedImage_->delays = std::move(delays);
}

DuiAnimatedImage::DuiAnimatedImage(std::vector<std::shared_ptr<const DuiImage>> frames,
                                   std::vector<int> delays)
    : animatedImage_(std::make_unique<Impl>()) {
    animatedImage_->sharedFrames = std::move(frames);
    animatedImage_->delays = std::move(delays);
}

DuiAnimatedImage::~DuiAnimatedImage() = default;
DuiAnimatedImage::DuiAnimatedImage(DuiAnimatedImage&&) noexcept = default;
DuiAnimatedImage& DuiAnimatedImage::operator=(DuiAnimatedImage&&) noexcept = default;

int DuiAnimatedImage::FrameCount() const {
    return animatedImage_->sharedFrames.empty()
        ? static_cast<int>(animatedImage_->frames.size())
        : static_cast<int>(animatedImage_->sharedFrames.size());
}

core::Size DuiAnimatedImage::Size() const {
    const DuiImage* frame = Frame(0);
    return frame ? frame->Size() : core::Size{};
}

const DuiImage* DuiAnimatedImage::Frame(int index) const {
    if (index < 0 || index >= FrameCount()) {
        return nullptr;
    }
    return animatedImage_->sharedFrames.empty()
        ? &animatedImage_->frames[index]
        : animatedImage_->sharedFrames[index].get();
}

int DuiAnimatedImage::FrameDelayMilliseconds(int index) const {
    return index >= 0 && index < static_cast<int>(animatedImage_->delays.size())
               ? animatedImage_->delays[index]
               : 0;
}

int DuiAnimatedImage::TotalDurationMilliseconds() const {
    int total{};
    for (const int delay : animatedImage_->delays) {
        total += (std::max)(0, delay);
    }
    return total;
}

int DuiAnimatedImage::FrameAt(std::size_t elapsedMilliseconds) const {
    return FrameAt(elapsedMilliseconds, animatedImage_->delays.data(), FrameCount());
}

int DuiAnimatedImage::FrameAt(std::size_t elapsedMilliseconds, const int* delays, int count) {
    if (delays == nullptr || count <= 0) {
        return 0;
    }
    std::size_t total{};
    for (int index = 0; index < count; ++index) {
        total += static_cast<std::size_t>((std::max)(0, delays[index]));
    }
    if (total == 0) {
        return 0;
    }
    const std::size_t withinLoop = elapsedMilliseconds % total;
    std::size_t elapsed{};
    for (int index = 0; index < count; ++index) {
        elapsed += static_cast<std::size_t>((std::max)(0, delays[index]));
        if (withinLoop < elapsed) {
            return index;
        }
    }
    return count - 1;
}

std::shared_ptr<const DuiAnimatedImage> DuiAnimatedImage::Create(
    std::vector<std::shared_ptr<const DuiImage>> frames, std::vector<int> delays) {
    if (frames.empty() || frames.size() != delays.size()) {
        return {};
    }
    for (const auto& frame : frames) {
        if (!frame || frame->Empty()) {
            return {};
        }
    }
    return std::shared_ptr<const DuiAnimatedImage>(
        new DuiAnimatedImage(std::move(frames), std::move(delays)));
}

} // namespace ysDui::render
