#include "ysDui/controls/window/DuiMessageBox.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

#include "ysDui/controls/window/DuiDialog.hpp"
#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"

namespace ysDui::controls::window {
namespace {
constexpr core::Size DefaultMessageBoxSize{244, 115};
constexpr int MaximumMessageBoxWidth = 640;
constexpr int TitleBarHeight = 26;
constexpr int IconSize = 28;
constexpr int ButtonHeight = 24;
constexpr int ButtonWidth = 64;
constexpr int ButtonGap = 8;
constexpr int ButtonBottomMargin = 8;
constexpr int ContentPadding = 14;
std::size_t Utf8GlyphCount(std::string_view text)
{
    std::size_t count{};
    for (std::size_t index{}; index < text.size(); ++count)
    {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        index += first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
    }
    return count;
}

core::Size MessageBoxSize(std::string_view message, int buttonCount)
{
    std::size_t longestLine{};
    int lineCount{1};
    std::size_t lineStart{};
    for (std::size_t index{}; index <= message.size(); ++index)
    {
        if (index != message.size() && message[index] != '\n')
            continue;
        longestLine = (std::max)(longestLine, Utf8GlyphCount(message.substr(lineStart, index - lineStart)));
        lineStart = index + 1;
        if (index != message.size())
            ++lineCount;
    }
    const int textWidth = static_cast<int>(longestLine) * 8;
    const int bodyWidth = ContentPadding + IconSize + 10 + textWidth + ContentPadding;
    const int buttonsWidth = ContentPadding * 2 + buttonCount * ButtonWidth
        + (std::max)(0, buttonCount - 1) * ButtonGap;
    const int width = std::clamp((std::max)({DefaultMessageBoxSize.width, bodyWidth, buttonsWidth}),
                                 DefaultMessageBoxSize.width, MaximumMessageBoxWidth);
    const int bodyHeight = (std::max)(IconSize, lineCount * 16);
    const int height = (std::max)(DefaultMessageBoxSize.height,
                                  TitleBarHeight + 10 + bodyHeight + 10 + ButtonHeight + ButtonBottomMargin);
    return {width, height};
}
}

class DuiMessageBox::Impl {
public:
    class Content final : public core::Control, public render::DuiRenderable {
    public:
        explicit Content(DuiMessageBox::Impl& owner) : owner_(owner) {}

        core::Control* HitTest(core::Point point) override
        {
            if (!EffectivelyVisible() || !Enabled() || !Bounds().Contains(point))
                return nullptr;
            SetPointerCursor(ButtonIndexAt(point) >= 0 ? core::DuiPointerCursor::Hand
                                                       : core::DuiPointerCursor::Arrow);
            return this;
        }

        bool OnEvent(const core::Event& event) override
        {
            if (event.type == core::EventType::KeyDown) {
                if (event.key == core::key::Escape) {
                    owner_.dialog.Close(owner_.closeResult);
                    return true;
                }
                if (event.key == core::key::Enter) {
                    owner_.dialog.Close(owner_.defaultResult);
                    return true;
                }
            }
            const int buttonIndex = ButtonIndexAt(event.position);
            if (event.type == core::EventType::PointerMove)
            {
                hoveredButton_ = buttonIndex;
                return Captured();
            }
            if (event.type == core::EventType::PointerDown && buttonIndex >= 0)
            {
                pressedButton_ = buttonIndex;
                SetCaptured(true);
                return true;
            }
            if (event.type == core::EventType::PointerCancel && Captured())
            {
                pressedButton_ = -1;
                SetCaptured(false);
                return true;
            }
            if (event.type == core::EventType::PointerUp && Captured())
            {
                const int pressed = pressedButton_;
                pressedButton_ = -1;
                SetCaptured(false);
                if (pressed >= 0 && pressed == buttonIndex)
                    owner_.dialog.Close(owner_.buttons[pressed].result);
                return true;
            }
            return false;
        }

        void Paint(render::Canvas& canvas, core::Rect dirty) const override
        {
            const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
            if (bounds.Empty())
                return;
            const core::DuiTheme& theme = Theme();
            const core::Color accent = theme.Get(core::ThemeSlot::DialogAccent);
            canvas.FillRect(bounds, theme.Get(core::ThemeSlot::MessageBackground));
            canvas.FillRect({Bounds().left, Bounds().top, Bounds().right,
                             Bounds().top + TitleBarHeight}, accent);
            canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::DialogBorder), 1.0F);
            const render::DuiTextStyle titleStyle{theme.Get(core::ThemeSlot::DialogTitleText), {}, 10, true};
            const render::DuiTextStyle messageStyle{theme.Get(core::ThemeSlot::TextDefault), {}, 9, false};
            canvas.DrawText(owner_.title, {Bounds().left + 10, Bounds().top,
                            Bounds().right - 10, Bounds().top + TitleBarHeight},
                            titleStyle, render::DuiTextAlignment::Start, false);
            const core::Rect icon{Bounds().left + ContentPadding, Bounds().top + TitleBarHeight + 10,
                                  Bounds().left + ContentPadding + IconSize,
                                  Bounds().top + TitleBarHeight + 10 + IconSize};
            PaintIcon(canvas, icon);
            canvas.DrawText(owner_.message, {icon.right + 10, icon.top, Bounds().right - ContentPadding,
                                             Bounds().bottom - ButtonHeight - ButtonBottomMargin - 10},
                            messageStyle, render::DuiTextAlignment::Start, true);
            for (int index = 0; index < static_cast<int>(owner_.buttons.size()); ++index) {
                const core::Rect button = ButtonRect(index);
                const bool primary = owner_.buttons[index].result == owner_.defaultResult;
                core::Color fill = primary ? accent : theme.Get(core::ThemeSlot::DialogPanelBackground);
                if (index == hoveredButton_)
                    fill = primary ? theme.Get(core::ThemeSlot::DialogPrimaryHover)
                                   : theme.Get(core::ThemeSlot::DialogButtonHover);
                if (index == pressedButton_)
                    fill = theme.Get(core::ThemeSlot::DialogPrimaryPressed);
                canvas.FillRect(button, fill);
                canvas.StrokeRoundedRect(button, 0, accent, 1.0F);
                render::DuiTextStyle style{primary || index == pressedButton_
                    ? theme.Get(core::ThemeSlot::DialogTitleText) : accent, {}, 9, false};
                canvas.DrawText(owner_.buttons[index].text, button, style, render::DuiTextAlignment::Center, false);
                if (primary)
                    canvas.StrokeRoundedRect({button.left + 3, button.top + 3,
                                              button.right - 3, button.bottom - 3},
                                             0, theme.Get(core::ThemeSlot::DialogTitleText), 1.0F);
            }
        }

    private:
        [[nodiscard]] int ButtonIndexAt(core::Point point) const
        {
            for (int index{}; index < static_cast<int>(owner_.buttons.size()); ++index)
                if (ButtonRect(index).Contains(point))
                    return index;
            return -1;
        }

        [[nodiscard]] core::Rect ButtonRect(int index) const
        {
            const int totalWidth = static_cast<int>(owner_.buttons.size()) * ButtonWidth +
                                   (static_cast<int>(owner_.buttons.size()) - 1) * ButtonGap;
            const int left = Bounds().left + (Bounds().Width() - totalWidth) / 2
                + index * (ButtonWidth + ButtonGap);
            return {left, Bounds().bottom - ButtonBottomMargin - ButtonHeight, left + ButtonWidth,
                    Bounds().bottom - ButtonBottomMargin};
        }

        void PaintIcon(render::Canvas& canvas, core::Rect bounds) const
        {
            const int centerX = (bounds.left + bounds.right) / 2;
            const int centerY = (bounds.top + bounds.bottom) / 2;
            if (owner_.type == DuiMessageBoxType::Warning)
            {
                render::DuiPath warning;
                warning.MoveTo({centerX, bounds.top});
                warning.LineTo({bounds.right, bounds.bottom});
                warning.LineTo({bounds.left, bounds.bottom});
                warning.Close();
                canvas.FillPath(warning, Theme().Get(core::ThemeSlot::MessageWarningFill));
                canvas.DrawText("!", bounds,
                                {Theme().Get(core::ThemeSlot::MessageWarningText), {}, 12, true},
                                render::DuiTextAlignment::Center, false);
                return;
            }

            const core::Color fill = owner_.type == DuiMessageBoxType::Error
                ? Theme().Get(core::ThemeSlot::MessageErrorFill)
                : Theme().Get(core::ThemeSlot::MessageInformationFill);
            canvas.FillEllipse(bounds, fill);
            if (owner_.type == DuiMessageBoxType::Error)
            {
                render::DuiPath cross;
                cross.MoveTo({centerX - 6, centerY - 6});
                cross.LineTo({centerX + 6, centerY + 6});
                cross.MoveTo({centerX + 6, centerY - 6});
                cross.LineTo({centerX - 6, centerY + 6});
                canvas.StrokePath(cross, Theme().Get(core::ThemeSlot::DialogTitleText), 2.0F);
                return;
            }
            canvas.DrawText(owner_.type == DuiMessageBoxType::Information ? "i" : "?", bounds,
                            {Theme().Get(core::ThemeSlot::DialogTitleText), {}, 12, true},
                            render::DuiTextAlignment::Center, false);
        }

        DuiMessageBox::Impl& owner_;
        int hoveredButton_{-1};
        int pressedButton_{-1};
    };

    DuiDialog dialog;
    std::string title;
    std::string message;
    std::vector<DuiMessageBoxButton> buttons{{"确认(O)", DuiDialog::IdOk}};
    std::function<void(int)> closed;
    DuiMessageBoxType type{DuiMessageBoxType::Information};
    int defaultResult{DuiDialog::IdOk};
    int closeResult{DuiDialog::IdCancel};
};

DuiMessageBox::DuiMessageBox() : impl_(std::make_unique<Impl>()) {}
DuiMessageBox::~DuiMessageBox() = default;
DuiMessageBox::DuiMessageBox(DuiMessageBox&&) noexcept = default;
DuiMessageBox& DuiMessageBox::operator=(DuiMessageBox&&) noexcept = default;
void DuiMessageBox::SetTitle(std::string value) { impl_->title = std::move(value); }
void DuiMessageBox::SetMessage(std::string value) { impl_->message = std::move(value); }
void DuiMessageBox::SetType(DuiMessageBoxType value) { impl_->type = value; }
void DuiMessageBox::SetButtons(std::vector<DuiMessageBoxButton> value)
{
    if (value.empty() || value.size() > 3)
        return;
    impl_->buttons = std::move(value);
    impl_->defaultResult = impl_->buttons.front().result;
}
void DuiMessageBox::SetDefaultButton(int result)
{
    if (std::any_of(impl_->buttons.begin(), impl_->buttons.end(), [result](const auto& button) { return button.result == result; }))
        impl_->defaultResult = result;
}
void DuiMessageBox::SetCloseResult(int result) { impl_->closeResult = result; }
void DuiMessageBox::SetClosedHandler(std::function<void(int)> handler) { impl_->closed = std::move(handler); }

bool DuiMessageBox::Show(ui::IUiHostFactory& factory, ui::HostRef owner, core::Rect anchor)
{
    auto content = std::make_unique<Impl::Content>(*impl_);
    impl_->dialog.SetCloseResult(impl_->closeResult);
    impl_->dialog.SetClosedHandler([state = impl_.get()](int result) {
        if (state->closed)
            state->closed(result);
    });
    ui::DuiPopupOptions options;
    options.anchor = anchor;
    options.size = MessageBoxSize(impl_->message, static_cast<int>(impl_->buttons.size()));
    options.placement = ui::DuiPopupPlacement::CenterOwner;
    options.dismissOnFocusLost = false;
    options.modality = ui::DuiPopupModality::OwnerModal;
    return impl_->dialog.Show(factory, owner, options, std::move(content));
}

void DuiMessageBox::Close(int result) { impl_->dialog.Close(result); }
int DuiMessageBox::Result() const { return impl_->dialog.Result(); }
bool DuiMessageBox::Visible() const { return impl_->dialog.Visible(); }

} // namespace ysDui::controls::window
