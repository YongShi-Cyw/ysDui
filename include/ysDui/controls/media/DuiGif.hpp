#pragma once

#include <memory>

#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiAnimatedImage.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::media {

class DuiGif final : public core::Control, public render::DuiRenderable {
public:
    DuiGif();
    ~DuiGif() override;
    DuiGif(const DuiGif&) = delete;
    DuiGif& operator=(const DuiGif&) = delete;
    DuiGif(DuiGif&&) = delete;
    DuiGif& operator=(DuiGif&&) = delete;

    void SetImage(std::shared_ptr<const render::DuiAnimatedImage> image);
    [[nodiscard]] const std::shared_ptr<const render::DuiAnimatedImage>& Image() const;
    void SetAnimationClock(core::AnimationClock* clock);
    void Start();
    void Stop();
    [[nodiscard]] bool Running() const;
    void SetFrameIndex(int index);
    [[nodiscard]] int FrameIndex() const;
    void SetStretch(bool stretch);
    [[nodiscard]] bool Stretch() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    void ScheduleNextFrame();
    void AdvanceFrame();

    class Impl;
    std::unique_ptr<Impl> gif_;
};

} // namespace ysDui::controls::media
