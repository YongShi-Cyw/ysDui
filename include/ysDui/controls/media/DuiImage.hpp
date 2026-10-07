#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::media {

enum class DuiImageScaleMode {
    None,
    Stretch,
    Fit,
    Fill,
};

struct DuiImageDrawRects final {
    core::Rect source;
    core::Rect destination;
};

class DuiImage final : public core::Control, public render::DuiRenderable {
public:
    DuiImage();
    ~DuiImage() override;
    DuiImage(const DuiImage&) = delete;
    DuiImage& operator=(const DuiImage&) = delete;
    DuiImage(DuiImage&&) noexcept;
    DuiImage& operator=(DuiImage&&) noexcept;

    void SetImage(std::shared_ptr<const render::DuiImage> image);
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& Image() const;
    void SetScaleMode(DuiImageScaleMode mode);
    [[nodiscard]] DuiImageScaleMode ScaleMode() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    [[nodiscard]] static DuiImageDrawRects ComputeDrawRects(DuiImageScaleMode mode,
                                                             core::Size imageSize,
                                                             core::Rect destination);
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> image_;
};

} // namespace ysDui::controls::media
