/**
 * 文件名：DuiTeachingTip.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：用 Flyout 承载标题、说明与操作按钮。
 */
#include "ysDui/controls/window/DuiTeachingTip.hpp"

#include <utility>

#include "ysDui/controls/basic/DuiButton.hpp"
#include "ysDui/controls/basic/DuiLabel.hpp"
#include "ysDui/controls/layout/DuiLayout.hpp"
#include "ysDui/controls/window/DuiFlyout.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::window {

class DuiTeachingTip::Impl {
public:
    DuiFlyout flyout;
    std::string title;
    std::string message;
    std::string actionText;
    std::function<void()> action;
    std::function<void()> dismissed;
};

DuiTeachingTip::DuiTeachingTip() : tip_(std::make_unique<Impl>())
{
    tip_->flyout.SetFixedSize({280, 148});
    tip_->flyout.SetLayoutHandler([](core::Control& content, core::Rect bounds)
    {
        content.Layout(bounds);
    });
}
DuiTeachingTip::~DuiTeachingTip() = default;
DuiTeachingTip::DuiTeachingTip(DuiTeachingTip&&) noexcept = default;
DuiTeachingTip& DuiTeachingTip::operator=(DuiTeachingTip&&) noexcept = default;

void DuiTeachingTip::SetTitle(std::string title) { tip_->title = std::move(title); }
const std::string& DuiTeachingTip::Title() const { return tip_->title; }
void DuiTeachingTip::SetMessage(std::string message) { tip_->message = std::move(message); }
const std::string& DuiTeachingTip::Message() const { return tip_->message; }
void DuiTeachingTip::SetActionText(std::string text) { tip_->actionText = std::move(text); }
const std::string& DuiTeachingTip::ActionText() const { return tip_->actionText; }
void DuiTeachingTip::SetActionHandler(std::function<void()> handler) { tip_->action = std::move(handler); }
void DuiTeachingTip::SetDismissedHandler(std::function<void()> handler)
{
    tip_->dismissed = std::move(handler);
    tip_->flyout.SetDismissedHandler(tip_->dismissed);
}

bool DuiTeachingTip::Show(ui::IUiHostFactory& factory, ui::HostRef owner, core::Rect anchor,
                          ui::DuiPopupPlacement placement)
{
    Impl* state = tip_.get();
    state->flyout.SetContentFactory([state]
    {
        auto column = std::make_unique<layout::DuiVBox>();
        column->SetPadding(12);
        column->SetGap(8);
        auto title = std::make_unique<basic::DuiLabel>();
        title->SetText(state->title.empty() ? "Tip" : state->title);
        render::DuiTextStyle heading;
        heading.bold = true;
        heading.pointSize = 10;
        title->SetStyle(heading);
        column->AddChild(std::move(title), layout::DuiLayoutHint{}.Fixed(22));
        auto body = std::make_unique<basic::DuiLabel>();
        body->SetText(state->message);
        body->SetWordWrap(true);
        column->AddChild(std::move(body), layout::DuiLayoutHint{}.Flexible());
        if (!state->actionText.empty())
        {
            auto action = std::make_unique<basic::DuiButton>();
            action->SetText(state->actionText);
            action->SetVariant(basic::DuiButtonVariant::Primary);
            action->SetClickHandler([state]
            {
                if (state->action)
                    state->action();
                state->flyout.Hide();
            });
            column->AddChild(std::move(action), layout::DuiLayoutHint{}.Fixed(32));
        }
        return column;
    });
    return state->flyout.Show(factory, owner, anchor, placement);
}

void DuiTeachingTip::Hide() { tip_->flyout.Hide(); }
bool DuiTeachingTip::Visible() const { return tip_->flyout.Visible(); }

} // namespace ysDui::controls::window
