#pragma once

#include <memory>

#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::feedback {

class DuiBusyIndicator final : public core::Control, public render::DuiRenderable {
public:
    DuiBusyIndicator();
    ~DuiBusyIndicator() override;
    DuiBusyIndicator(const DuiBusyIndicator&) = delete;
    DuiBusyIndicator& operator=(const DuiBusyIndicator&) = delete;
    DuiBusyIndicator(DuiBusyIndicator&&) noexcept;
    DuiBusyIndicator& operator=(DuiBusyIndicator&&) noexcept;
    void SetActive(bool active);
    [[nodiscard]] bool Active() const;
    void SetAnimationClock(core::AnimationClock* clock);
    void SetPhase(int milliseconds);
    [[nodiscard]] core::Size DesiredSize() const override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    void ScheduleNextRotation();

    class Impl;
    std::unique_ptr<Impl> busy_;
};

} // namespace ysDui::controls::feedback
