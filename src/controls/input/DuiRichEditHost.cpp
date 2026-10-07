#include "ysDui/controls/input/DuiRichEditHost.hpp"

#include <utility>

#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"

namespace ysDui::controls::input {

class DuiRichEditHost::Impl {
public:
    ui::DuiRichTextInput* input{};
    std::shared_ptr<int> inputLifetime;
    ui::DuiTextInputOptions options{true, true};
    std::string text;
    std::string placeholder;
    ui::DuiTextRange selection;
    bool automaticLinkDetection{};
    std::function<void(std::string_view)> changed;
    std::function<void(std::string_view)> linkActivated;
};

DuiRichEditHost::DuiRichEditHost() : richEditHost_(std::make_unique<Impl>()) {}
DuiRichEditHost::~DuiRichEditHost()
{
    if (!richEditHost_) return;
    richEditHost_->inputLifetime.reset();
    richEditHost_->input = nullptr;
}
DuiRichEditHost::DuiRichEditHost(DuiRichEditHost&&) noexcept = default;
DuiRichEditHost& DuiRichEditHost::operator=(DuiRichEditHost&&) noexcept = default;

void DuiRichEditHost::SetRichTextInput(ui::DuiRichTextInput* input)
{
    if (richEditHost_->input == input)
        return;
    richEditHost_->inputLifetime.reset();
    if (richEditHost_->input) {
        richEditHost_->input->SetChangedHandler({});
        richEditHost_->input->SetFocusLostHandler({});
        richEditHost_->input->SetLinkActivatedHandler({});
    }
    richEditHost_->input = input;
    if (!input)
        return;
    input->SetOptions(richEditHost_->options);
    input->SetText(richEditHost_->text);
    input->SetSelection(richEditHost_->selection);
    input->SetPlaceholder(richEditHost_->placeholder);
    input->SetAutomaticLinkDetection(richEditHost_->automaticLinkDetection);
    input->SetBounds(Bounds());
    input->SetVisible(Visible());
    input->SetEnabled(Enabled());
    input->SetBorderVisible(false);
    richEditHost_->inputLifetime = std::make_shared<int>();
    const std::weak_ptr<int> lifetime = richEditHost_->inputLifetime;
    input->SetChangedHandler([lifetime, state = richEditHost_.get()] {
        if (!lifetime.lock()) return;
        state->text = state->input->Text();
        state->selection = state->input->Selection();
        if (state->changed)
            state->changed(state->text);
    });
    input->SetLinkActivatedHandler([lifetime, state = richEditHost_.get()](std::string link) {
        if (!lifetime.lock()) return;
        if (state->linkActivated)
            state->linkActivated(link);
    });
}

void DuiRichEditHost::SetText(std::string text, bool notify)
{
    if (richEditHost_->text == text)
        return;
    richEditHost_->text = std::move(text);
    if (richEditHost_->input)
        richEditHost_->input->SetText(richEditHost_->text);
    if (notify && richEditHost_->changed)
        richEditHost_->changed(richEditHost_->text);
}

const std::string& DuiRichEditHost::Text() const { return richEditHost_->text; }
void DuiRichEditHost::SetPlaceholder(std::string text) { richEditHost_->placeholder = std::move(text); if (richEditHost_->input) richEditHost_->input->SetPlaceholder(richEditHost_->placeholder); }
void DuiRichEditHost::SetOptions(ui::DuiTextInputOptions options) { richEditHost_->options = options; if (richEditHost_->input) richEditHost_->input->SetOptions(options); }
ui::DuiTextInputOptions DuiRichEditHost::Options() const { return richEditHost_->options; }
void DuiRichEditHost::SetSelection(ui::DuiTextRange range) { richEditHost_->selection = range; if (richEditHost_->input) richEditHost_->input->SetSelection(range); }
ui::DuiTextRange DuiRichEditHost::Selection() const { return richEditHost_->input ? richEditHost_->input->Selection() : richEditHost_->selection; }
void DuiRichEditHost::SelectAll() { if (richEditHost_->input) richEditHost_->input->SelectAll(); }
void DuiRichEditHost::ReplaceSelection(std::string text) { if (richEditHost_->input) richEditHost_->input->ReplaceSelection(std::move(text)); }
void DuiRichEditHost::AppendText(std::string text) { if (richEditHost_->input) richEditHost_->input->AppendText(std::move(text)); }
bool DuiRichEditHost::CanUndo() const { return richEditHost_->input && richEditHost_->input->CanUndo(); }
void DuiRichEditHost::Undo() { if (richEditHost_->input) richEditHost_->input->Undo(); }
void DuiRichEditHost::Cut() { if (richEditHost_->input) richEditHost_->input->Cut(); }
void DuiRichEditHost::Copy() { if (richEditHost_->input) richEditHost_->input->Copy(); }
void DuiRichEditHost::Paste() { if (richEditHost_->input) richEditHost_->input->Paste(); }
void DuiRichEditHost::ClearSelection() { if (richEditHost_->input) richEditHost_->input->ClearSelection(); }
void DuiRichEditHost::SetSelectionFormat(ui::DuiRichTextFormat format) { if (richEditHost_->input) richEditHost_->input->SetSelectionFormat(format); }
void DuiRichEditHost::SetAutomaticLinkDetection(bool enabled) { richEditHost_->automaticLinkDetection = enabled; if (richEditHost_->input) richEditHost_->input->SetAutomaticLinkDetection(enabled); }
bool DuiRichEditHost::InsertImage(const render::DuiImage& image, core::Size maximumSize) { return richEditHost_->input && richEditHost_->input->InsertImage(image, maximumSize); }
void DuiRichEditHost::InsertQuoteBlock(std::string sender, std::string body) { if (richEditHost_->input) richEditHost_->input->InsertQuoteBlock(std::move(sender), std::move(body)); }
void DuiRichEditHost::InsertFileCard(std::string fileName, std::uint64_t sizeBytes) { if (richEditHost_->input) richEditHost_->input->InsertFileCard(std::move(fileName), sizeBytes); }
void DuiRichEditHost::SetTextChangedHandler(std::function<void(std::string_view)> handler) { richEditHost_->changed = std::move(handler); }
void DuiRichEditHost::SetLinkActivatedHandler(std::function<void(std::string_view)> handler) { richEditHost_->linkActivated = std::move(handler); }
core::Size DuiRichEditHost::DesiredSize() const { return {200, richEditHost_->options.multiline ? 120 : 25}; }
void DuiRichEditHost::Layout(core::Rect bounds) { SetBounds(bounds); if (richEditHost_->input) richEditHost_->input->SetBounds(bounds); }
void DuiRichEditHost::SetVisible(bool visible) { core::Control::SetVisible(visible); if (richEditHost_->input) richEditHost_->input->SetVisible(visible); }
void DuiRichEditHost::SetEnabled(bool enabled) { core::Control::SetEnabled(enabled); if (richEditHost_->input) richEditHost_->input->SetEnabled(enabled); }
bool DuiRichEditHost::OnEvent(const core::Event& event) { if (Enabled() && event.type == core::EventType::PointerDown && Bounds().Contains(event.position)) { if (richEditHost_->input) richEditHost_->input->Focus(); return true; } return false; }
void DuiRichEditHost::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const auto bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty()) return;
    const bool enabled = Enabled();
    canvas.FillRect(bounds, enabled ? Theme().Get(core::ThemeSlot::SurfaceBackground)
                                    : Theme().Get(core::ThemeSlot::FieldDisabledBackground));
    canvas.StrokeRoundedRect(bounds, 0, Focused() ? Theme().Get(core::ThemeSlot::BrandPrimary)
                                                  : enabled ? Theme().Get(core::ThemeSlot::FieldBorder)
                                                            : Theme().Get(core::ThemeSlot::FieldDisabledBorder), 1.0F);
    if (richEditHost_->input) return;
    const bool placeholder = richEditHost_->text.empty();
    const std::string& text = placeholder ? richEditHost_->placeholder : richEditHost_->text;
    render::DuiTextStyle style;
    style.color = !enabled ? Theme().Get(core::ThemeSlot::FieldDisabledText)
                           : placeholder ? Theme().Get(core::ThemeSlot::FieldPlaceholder)
                                         : Theme().Get(core::ThemeSlot::FieldText);
    canvas.DrawText(text, {bounds.left + 8, bounds.top + 5, bounds.right - 8, bounds.bottom - 5}, style,
                    render::DuiTextAlignment::Start, richEditHost_->options.multiline && richEditHost_->options.wordWrap);
}

} // namespace ysDui::controls::input
