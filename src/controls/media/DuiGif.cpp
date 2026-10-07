#include "ysDui/controls/media/DuiGif.hpp"

#include <algorithm>
#include <utility>

namespace ysDui::controls::media {
namespace {
struct DuiGifCallbackState final {
    DuiGif* owner{};
};
}

class DuiGif::Impl {
public:
    std::shared_ptr<const render::DuiAnimatedImage> image;
    std::shared_ptr<DuiGifCallbackState> callbackState;
    core::AnimationClock* clock{};
    core::AnimationClock::TaskId task{};
    int frameIndex{};
    bool running{};
    bool stretch{true};
};

DuiGif::DuiGif() : gif_(std::make_unique<Impl>()) {
    gif_->callbackState = std::make_shared<DuiGifCallbackState>();
    gif_->callbackState->owner = this;
}

DuiGif::~DuiGif() {
    Stop();
    gif_->callbackState->owner = nullptr;
}

void DuiGif::SetImage(std::shared_ptr<const render::DuiAnimatedImage> image) {
    const bool wasRunning = gif_->running;
    Stop();
    gif_->image = std::move(image);
    gif_->frameIndex = 0;
    if (wasRunning) {
        Start();
    }
}

const std::shared_ptr<const render::DuiAnimatedImage>& DuiGif::Image() const {
    return gif_->image;
}

void DuiGif::SetAnimationClock(core::AnimationClock* clock) {
    const bool wasRunning = gif_->running;
    if (gif_->clock && gif_->task) {
        gif_->clock->Cancel(gif_->task);
    }
    gif_->task = {};
    gif_->clock = clock;
    if (wasRunning) {
        ScheduleNextFrame();
    }
}

void DuiGif::Start() {
    if (gif_->running || !gif_->image || gif_->image->FrameCount() == 0) {
        return;
    }
    gif_->running = true;
    ScheduleNextFrame();
}

void DuiGif::Stop() {
    gif_->running = false;
    if (gif_->clock && gif_->task) {
        gif_->clock->Cancel(gif_->task);
    }
    gif_->task = {};
}

bool DuiGif::Running() const {
    return gif_->running;
}

void DuiGif::SetFrameIndex(int index) {
    if (!gif_->image || gif_->image->FrameCount() == 0) {
        gif_->frameIndex = 0;
        return;
    }
    gif_->frameIndex = (std::clamp)(index, 0, gif_->image->FrameCount() - 1);
}

int DuiGif::FrameIndex() const {
    return gif_->frameIndex;
}

void DuiGif::SetStretch(bool stretch) {
    gif_->stretch = stretch;
}

bool DuiGif::Stretch() const {
    return gif_->stretch;
}

core::Size DuiGif::DesiredSize() const {
    return gif_->image ? gif_->image->Size() : core::Size{};
}

void DuiGif::ScheduleNextFrame() {
    if (!gif_->running || !gif_->clock || !gif_->image || gif_->image->FrameCount() == 0) {
        return;
    }
    const int delay = (std::max)(1, gif_->image->FrameDelayMilliseconds(gif_->frameIndex));
    const std::weak_ptr<DuiGifCallbackState> callbackState = gif_->callbackState;
    gif_->task = gif_->clock->Schedule(delay, [callbackState](double progress) {
        if (progress < 1.0) {
            return;
        }
        const std::shared_ptr<DuiGifCallbackState> state = callbackState.lock();
        if (state && state->owner) {
            state->owner->AdvanceFrame();
        }
    });
}

void DuiGif::AdvanceFrame() {
    gif_->task = {};
    if (!gif_->running || !gif_->image || gif_->image->FrameCount() == 0) {
        return;
    }
    gif_->frameIndex = (gif_->frameIndex + 1) % gif_->image->FrameCount();
    ScheduleNextFrame();
}

void DuiGif::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible() || !gif_->image) {
        return;
    }
    const render::DuiImage* frame = gif_->image->Frame(gif_->frameIndex);
    if (!frame || frame->Empty()) {
        return;
    }
    const core::Rect clip = core::Rect::Intersect(Bounds(), dirty);
    if (clip.Empty()) {
        return;
    }
    core::Rect destination = Bounds();
    if (!gif_->stretch) {
        const core::Size size = frame->Size();
        destination.left += (destination.Width() - size.width) / 2;
        destination.top += (destination.Height() - size.height) / 2;
        destination.right = destination.left + size.width;
        destination.bottom = destination.top + size.height;
    }
    canvas.PushClip(clip);
    canvas.DrawImage(*frame, destination);
    canvas.PopClip();
}

} // namespace ysDui::controls::media
