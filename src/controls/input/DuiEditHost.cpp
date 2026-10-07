#include "ysDui/controls/input/DuiEditHost.hpp"

#include <algorithm>
#include <utility>

#include "DuiTextInputPaint.hpp"
#include "DuiTextInputPointer.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::input {
namespace {
constexpr int DEFAULT_HEIGHT = 25;
constexpr int TEXT_POINT_SIZE = 12;
constexpr int TEXT_HORIZONTAL_PAD = 8;
constexpr int TEXT_VERTICAL_PAD = 3;

int ScaleByHeight(int value, int height, int minimum = 1)
{
    const int h = (std::max)(1, height);
    return (std::max)(minimum, value * h / DEFAULT_HEIGHT);
}

core::Rect NativeInputBounds(core::Rect bounds)
{
    return {bounds.left + 5, bounds.top + 3,
            (std::max)(bounds.left + 5, bounds.right - 5),
            (std::max)(bounds.top + 3, bounds.bottom - 3)};
}
}

class DuiEditHost::Impl {
public:
    ui::DuiTextInput* input{};
    std::shared_ptr<int> inputLifetime;
    ui::DuiTextInputOptions options;
    std::string text;
    std::string placeholder;
    std::function<void(std::string_view)> changed;
    std::function<void()> submitted;
    bool selecting{};
};
DuiEditHost::DuiEditHost() : editHost_(std::make_unique<Impl>())
{
    SetPointerCursor(core::DuiPointerCursor::IBeam);
}
DuiEditHost::~DuiEditHost()
{
    editHost_->inputLifetime.reset();
    editHost_->input = nullptr;
}
void DuiEditHost::SetTextInput(ui::DuiTextInput* input)
{
    if (editHost_->input == input) return;
    if (editHost_->selecting)
    {
        if (editHost_->input) editHost_->input->EndSelection();
        editHost_->selecting = false;
        SetCaptured(false);
    }
    editHost_->inputLifetime.reset();
    if (editHost_->input) { editHost_->input->SetChangedHandler({}); editHost_->input->SetFocusLostHandler({}); editHost_->input->SetSubmitHandler({}); }
    editHost_->input = input;
    if (!input) return;
    input->SetOptions(editHost_->options); input->SetText(editHost_->text); input->SetPlaceholder(editHost_->placeholder);
    input->SetBounds(NativeInputBounds(Bounds())); input->SetVisible(Visible()); input->SetEnabled(Enabled()); input->SetBorderVisible(false);
    editHost_->inputLifetime = std::make_shared<int>();
    const std::weak_ptr<int> lifetime = editHost_->inputLifetime;
    input->SetChangedHandler([lifetime, state = editHost_.get()] {
        if (!lifetime.lock()) return;
        state->text = state->input->Text();
        if (state->changed) state->changed(state->text);
    });
    input->SetSubmitHandler([lifetime, state = editHost_.get()] {
        if (!lifetime.lock()) return;
        if (state->submitted) state->submitted();
    });
}
void DuiEditHost::SetText(std::string text, bool notify)
{
    if (editHost_->text == text) return;
    editHost_->text = std::move(text); if (editHost_->input) editHost_->input->SetText(editHost_->text);
    if (notify && editHost_->changed) editHost_->changed(editHost_->text);
}
const std::string& DuiEditHost::Text() const { return editHost_->text; }
void DuiEditHost::SetPlaceholder(std::string text) { editHost_->placeholder = std::move(text); if (editHost_->input) editHost_->input->SetPlaceholder(editHost_->placeholder); }
void DuiEditHost::SetOptions(ui::DuiTextInputOptions options) { editHost_->options = options; if (editHost_->input) editHost_->input->SetOptions(options); }
ui::DuiTextInputOptions DuiEditHost::Options() const { return editHost_->options; }
void DuiEditHost::SetTextChangedHandler(std::function<void(std::string_view)> handler) { editHost_->changed = std::move(handler); }
void DuiEditHost::SetSubmitHandler(std::function<void()> handler) { editHost_->submitted = std::move(handler); }
core::Size DuiEditHost::DesiredSize() const { return {160, editHost_->options.multiline ? 80 : 25}; }
void DuiEditHost::Layout(core::Rect bounds) { SetBounds(bounds); if (editHost_->input) editHost_->input->SetBounds(NativeInputBounds(bounds)); }
void DuiEditHost::SetVisible(bool visible) { core::Control::SetVisible(visible); if (editHost_->input) editHost_->input->SetVisible(visible); }
void DuiEditHost::SetEnabled(bool enabled) { core::Control::SetEnabled(enabled); if (editHost_->input) editHost_->input->SetEnabled(enabled); }
core::DuiAccessibilityData DuiEditHost::CreateAccessibilityData() const
{
    const std::string value = editHost_->options.password ? std::string{} : editHost_->text;
    core::DuiAccessibilityData data{
        core::DuiAccessibilityRole::Edit, editHost_->placeholder, value, {}, true, {}};
    data.patterns = core::DuiAccessibilityPattern::Value;
    data.readOnly = editHost_->options.readOnly;
    return data;
}
bool DuiEditHost::PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                             std::string_view value)
{
    if (!Enabled() || editHost_->options.readOnly
        || action != core::DuiAccessibilityAction::SetValue)
    {
        return false;
    }
    SetText(std::string(value), true);
    return true;
}
bool DuiEditHost::OnEvent(const core::Event& event)
{
    if (!Enabled()) return false;
    // 未绑定原生输入时（Canvas 回退）由本控件判定 Enter 提交，语义与平台的 SubmitsOnReturn 一致
    if (editHost_->input == nullptr && event.type == core::EventType::KeyDown
        && event.key == core::key::Enter && !editHost_->options.readOnly)
    {
        const bool submits = !editHost_->options.multiline
            || (editHost_->options.submitOnEnter && (event.modifiers & core::modifier::Shift) == 0);
        if (submits)
        {
            if (editHost_->submitted) editHost_->submitted();
            return true;
        }
    }
    if (ysDui::controls::detail::HandleTextInputPointer(
            *this, editHost_->input, Bounds(), event, editHost_->selecting,
            core::DuiPointerCursor::Arrow))
    {
        return true;
    }
    return event.type == core::EventType::PointerDown && Bounds().Contains(event.position);
}
void DuiEditHost::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = Bounds();
    const core::Rect clipped = core::Rect::Intersect(bounds, dirty);
    if (!EffectivelyVisible() || clipped.Empty()) return;
    const bool enabled = Enabled();
    canvas.FillRect(clipped, enabled ? Theme().Get(core::ThemeSlot::SurfaceBackground)
                                     : Theme().Get(core::ThemeSlot::FieldDisabledBackground));
    canvas.StrokeRoundedRect(bounds, 0, Focused() ? Theme().Get(core::ThemeSlot::BrandPrimary)
                                                  : enabled ? Theme().Get(core::ThemeSlot::FieldBorder)
                                                            : Theme().Get(core::ThemeSlot::FieldDisabledBorder),
                             static_cast<float>(ScaleByHeight(1, bounds.Height())));
    render::DuiTextStyle style;
    style.color = enabled ? Theme().Get(core::ThemeSlot::FieldText)
                          : Theme().Get(core::ThemeSlot::FieldDisabledText);
    style.pointSize = (std::clamp)(ScaleByHeight(TEXT_POINT_SIZE, bounds.Height(), 8), 8, 96);
    const int padX = ScaleByHeight(TEXT_HORIZONTAL_PAD, bounds.Height());
    const int padY = ScaleByHeight(TEXT_VERTICAL_PAD, bounds.Height());
    const core::Color placeholder = enabled ? Theme().Get(core::ThemeSlot::FieldPlaceholder)
                                            : Theme().Get(core::ThemeSlot::FieldDisabledText);
    ysDui::controls::detail::PaintTextInput(
        canvas, editHost_->input, editHost_->text, editHost_->placeholder,
        {bounds.left + padX, bounds.top + padY, bounds.right - padX, bounds.bottom - padY}, style,
        placeholder, render::DuiTextAlignment::Start,
        editHost_->options.multiline && editHost_->options.wordWrap,
        Focused() && enabled && !editHost_->options.readOnly && !editHost_->options.multiline,
        Theme().Get(core::ThemeSlot::TextSelectionBackground),
        Theme().Get(core::ThemeSlot::TextSelectionText),
        editHost_->options.password);
}
} // namespace ysDui::controls::input
