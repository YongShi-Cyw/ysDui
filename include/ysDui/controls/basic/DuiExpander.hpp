#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::basic {

struct DuiExpanderPadding final {
    int left{8};
    int top{4};
    int right{8};
    int bottom{8};
};

class DuiExpander final : public core::Control, public render::DuiRenderable {
public:
    DuiExpander();
    ~DuiExpander() override;
    DuiExpander(const DuiExpander&) = delete;
    DuiExpander& operator=(const DuiExpander&) = delete;
    DuiExpander(DuiExpander&&) = delete;
    DuiExpander& operator=(DuiExpander&&) = delete;

    void SetTitle(std::string title);
    [[nodiscard]] const std::string& Title() const;
    void SetExpanded(bool expanded, bool animate = false);
    [[nodiscard]] bool Expanded() const;
    void SetAnimationClock(core::AnimationClock* clock);
    void SetAnimationDuration(int milliseconds);
    [[nodiscard]] int AnimationDuration() const;
    void SetExpandedChangedHandler(std::function<void(bool)> handler);
    /**
     * 设置期望尺寸变化回调，用于通知父布局在展开动画期间重新排列子控件。
     * @param handler 尺寸改变后调用的回调；传入空回调可取消订阅。
     */
    void SetDesiredSizeChangedHandler(std::function<void()> handler);
    void SetContent(std::unique_ptr<core::Control> content);
    [[nodiscard]] core::Control* Content() const;
    void SetContentSize(core::Size size);
    [[nodiscard]] core::Size ContentSize() const;
    void SetTitleStripHeight(int pixels);
    [[nodiscard]] int TitleStripHeight() const;
    void SetPadding(DuiExpanderPadding padding);
    [[nodiscard]] DuiExpanderPadding Padding() const;
    void SetTitleStyle(render::DuiTextStyle style);
    void SetTitleBackgroundColor(core::Color color);
    void SetBorderColor(core::Color color);
    [[nodiscard]] core::Rect TitleRect() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;
    bool PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                    std::string_view value) override;

private:
    class Impl;
    void NotifyDesiredSizeChanged();
    std::unique_ptr<Impl> expander_;
};

} // namespace ysDui::controls::basic
