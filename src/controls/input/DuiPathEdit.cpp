#include "ysDui/controls/input/DuiPathEdit.hpp"

#include <algorithm>
#include <utility>

#include "DuiTextInputPaint.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::input {
namespace {
constexpr int DefaultBrowseWidth = 72;
constexpr int TextGap = 4;
}

class DuiPathEdit::Impl {
public:
    ui::DuiPathPicker* picker{};
    ui::DuiTextInput* input{};
    std::shared_ptr<int> inputLifetime;
    ui::DuiPathPickerMode mode{ui::DuiPathPickerMode::OpenFile};
    std::string path;
    std::string placeholder;
    std::string browseText{"Browse"};
    render::DuiTextStyle textStyle;
    std::function<void(std::string_view)> changed;
    int browseWidth{DefaultBrowseWidth};
    bool suppressInput{};
};

DuiPathEdit::DuiPathEdit() : pathEdit_(std::make_unique<Impl>()) {}
DuiPathEdit::~DuiPathEdit()
{
    pathEdit_->inputLifetime.reset();
    pathEdit_->input = nullptr;
}
void DuiPathEdit::SetPathPicker(ui::DuiPathPicker* picker) { pathEdit_->picker = picker; }
ui::DuiPathPicker* DuiPathEdit::PathPicker() const { return pathEdit_->picker; }
void DuiPathEdit::SetTextInput(ui::DuiTextInput* input)
{
    if (pathEdit_->input == input) return;
    pathEdit_->inputLifetime.reset();
    if (pathEdit_->input) { pathEdit_->input->SetChangedHandler({}); pathEdit_->input->SetFocusLostHandler({}); }
    pathEdit_->input = input;
    if (!input) return;
    input->SetBorderVisible(false);
    input->SetPlaceholder(pathEdit_->placeholder);
    pathEdit_->inputLifetime = std::make_shared<int>();
    const std::weak_ptr<int> lifetime = pathEdit_->inputLifetime;
    input->SetChangedHandler([lifetime, state = pathEdit_.get()] {
        if (!lifetime.lock()) return;
        if (state->suppressInput) return;
        state->path = state->input->Text();
        if (state->changed) state->changed(state->path);
    });
    input->SetText(pathEdit_->path);
    Layout(Bounds());
}
ui::DuiTextInput* DuiPathEdit::TextInput() const { return pathEdit_->input; }
void DuiPathEdit::SetBrowseMode(ui::DuiPathPickerMode mode) { pathEdit_->mode = mode; }
ui::DuiPathPickerMode DuiPathEdit::BrowseMode() const { return pathEdit_->mode; }
void DuiPathEdit::SetPath(std::string path, bool notify)
{
    pathEdit_->path = std::move(path);
    if (pathEdit_->input && pathEdit_->input->Text() != pathEdit_->path) { pathEdit_->suppressInput = true; pathEdit_->input->SetText(pathEdit_->path); pathEdit_->suppressInput = false; }
    if (notify && pathEdit_->changed) pathEdit_->changed(pathEdit_->path);
}
std::string DuiPathEdit::Path() const { return pathEdit_->input ? pathEdit_->input->Text() : pathEdit_->path; }
void DuiPathEdit::SetPlaceholder(std::string text) { pathEdit_->placeholder = std::move(text); if (pathEdit_->input) pathEdit_->input->SetPlaceholder(pathEdit_->placeholder); }
void DuiPathEdit::SetBrowseText(std::string text) { pathEdit_->browseText = std::move(text); }
void DuiPathEdit::SetBrowseWidth(int pixels) { pathEdit_->browseWidth = (std::max)(1, pixels); Layout(Bounds()); }
core::Rect DuiPathEdit::BrowseRect() const { const auto bounds = Bounds(); return {(std::max)(bounds.left, bounds.right - pathEdit_->browseWidth), bounds.top, bounds.right, bounds.bottom}; }
core::Rect DuiPathEdit::TextRect() const { const auto bounds = Bounds(); const auto browse = BrowseRect(); return {bounds.left + 1, bounds.top + 1, (std::max)(bounds.left + 1, browse.left - TextGap), bounds.bottom - 1}; }
core::Size DuiPathEdit::DesiredSize() const { return {240, 25}; }
void DuiPathEdit::SetPathChangedHandler(std::function<void(std::string_view)> handler) { pathEdit_->changed = std::move(handler); }
void DuiPathEdit::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    if (!pathEdit_->input) return;
    pathEdit_->input->SetBounds(TextRect());
    pathEdit_->input->SetVisible(EffectivelyVisible());
    pathEdit_->input->SetEnabled(Enabled());
}
void DuiPathEdit::Browse()
{
    if (!pathEdit_->picker) return;
    const ui::DuiPathPickerResult result = pathEdit_->picker->Browse(BrowseMode(), Path());
    if (result.accepted) SetPath(result.path, true);
}
bool DuiPathEdit::OnEvent(const core::Event& event)
{
    if (!Enabled() || event.type != core::EventType::PointerDown || !Bounds().Contains(event.position)) return false;
    if (BrowseRect().Contains(event.position)) { Browse(); return true; }
    if (pathEdit_->input) pathEdit_->input->FocusAt(event.position);
    return true;
}
void DuiPathEdit::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const auto bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty()) return;
    const auto text = TextRect();
    const auto browse = BrowseRect();
    const bool enabled = Enabled();
    canvas.FillRect(bounds, enabled ? Theme().Get(core::ThemeSlot::SurfaceBackground)
                                    : Theme().Get(core::ThemeSlot::FieldDisabledBackground));
    const core::Color border = enabled ? Theme().Get(core::ThemeSlot::FieldBorder)
                                       : Theme().Get(core::ThemeSlot::FieldDisabledBorder);
    canvas.StrokeRoundedRect(bounds, 0, border, 1.0F);
    canvas.FillRect(browse, enabled ? Theme().Get(core::ThemeSlot::InputButtonBackground)
                                    : Theme().Get(core::ThemeSlot::InputButtonDisabledBackground));
    canvas.StrokeRoundedRect(browse, 0, border, 1.0F);
    auto style = pathEdit_->textStyle;
    style.color = enabled ? Theme().Get(core::ThemeSlot::ControlText)
                          : Theme().Get(core::ThemeSlot::FieldDisabledText);
    canvas.DrawText(pathEdit_->browseText, browse, style, render::DuiTextAlignment::Center, false);
    const core::Color placeholder = enabled ? Theme().Get(core::ThemeSlot::FieldPlaceholder)
                                            : Theme().Get(core::ThemeSlot::FieldDisabledText);
    ysDui::controls::detail::PaintTextInput(
        canvas, pathEdit_->input, pathEdit_->path, pathEdit_->placeholder,
        {text.left + 6, text.top, text.right - 4, text.bottom}, style, placeholder,
        render::DuiTextAlignment::Start, false, Focused() && enabled,
        Theme().Get(core::ThemeSlot::TextSelectionBackground),
        Theme().Get(core::ThemeSlot::TextSelectionText));
}

} // namespace ysDui::controls::input
