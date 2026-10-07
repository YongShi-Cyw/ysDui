#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::render {

class DuiImage;
class DuiImageAccess;

class DuiAnimatedImage final {
public:
    ~DuiAnimatedImage();
    DuiAnimatedImage(const DuiAnimatedImage&) = delete;
    DuiAnimatedImage& operator=(const DuiAnimatedImage&) = delete;
    DuiAnimatedImage(DuiAnimatedImage&&) noexcept;
    DuiAnimatedImage& operator=(DuiAnimatedImage&&) noexcept;

    [[nodiscard]] int FrameCount() const;
    [[nodiscard]] core::Size Size() const;
    [[nodiscard]] const DuiImage* Frame(int index) const;
    [[nodiscard]] int FrameDelayMilliseconds(int index) const;
    [[nodiscard]] int TotalDurationMilliseconds() const;
    [[nodiscard]] int FrameAt(std::size_t elapsedMilliseconds) const;
    [[nodiscard]] static int FrameAt(std::size_t elapsedMilliseconds, const int* delays,
                                     int count);
    [[nodiscard]] static std::shared_ptr<const DuiAnimatedImage> Create(
        std::vector<std::shared_ptr<const DuiImage>> frames, std::vector<int> delays);

private:
    class Impl;
    DuiAnimatedImage(std::vector<DuiImage> frames, std::vector<int> delays);
    DuiAnimatedImage(std::vector<std::shared_ptr<const DuiImage>> frames, std::vector<int> delays);
    friend class DuiImageAccess;
    std::unique_ptr<Impl> animatedImage_;
};

} // namespace ysDui::render
