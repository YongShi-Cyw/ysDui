#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"

namespace ysDui::core { class Host; }

namespace ysDui::controls::feedback {

class DuiToolTipManager final {
public:
    DuiToolTipManager();
    ~DuiToolTipManager();
    DuiToolTipManager(const DuiToolTipManager&) = delete;
    DuiToolTipManager& operator=(const DuiToolTipManager&) = delete;
    DuiToolTipManager(DuiToolTipManager&&) = delete;
    DuiToolTipManager& operator=(DuiToolTipManager&&) = delete;

    void SetAnimationClock(core::AnimationClock* clock);
    void SetHost(core::Host* host);
    void SetPopupHost(ui::IPopupHost* popup);
    void SetTextMeasurer(render::DuiTextMeasurer* measurer);
    void SetTextStyle(render::DuiTextStyle style);
    void SetDelayMilliseconds(int milliseconds);
    [[nodiscard]] int DelayMilliseconds() const;
    void Register(core::Control* control, std::string text);
    void Unregister(core::Control* control);
    [[nodiscard]] std::string TextFor(const core::Control* control) const;
    void OnHover(core::Control* control, core::Point anchor);
    void OnLeave(const core::Control* control = nullptr);
    void HideNow();
    [[nodiscard]] bool Showing() const;
    [[nodiscard]] std::string ShowingText() const;

private:
    void CancelDelay();
    void ShowPending();

    class Impl;
    std::unique_ptr<Impl> toolTip_;
};

} // namespace ysDui::controls::feedback
