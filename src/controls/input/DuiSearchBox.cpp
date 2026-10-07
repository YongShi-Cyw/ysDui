#include "ysDui/controls/input/DuiSearchBox.hpp"

#include <algorithm>
#include <utility>

#include "DuiTextInputPaint.hpp"
#include "DuiTextInputPointer.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::input {
class DuiSearchBox::Impl {
public:
    ui::DuiTextInput* input{};
    std::shared_ptr<int> inputLifetime;
    std::string text;
    std::string placeholder;
    std::function<void(std::string_view)> changed;
    int glyphWidth{24};
    int clearWidth{22};
    bool readOnly{};
    int maxLength{};
    bool suppress{};
    core::Rect bounds;
    bool selecting{};
};

namespace {
constexpr int BorderWidth = 1;
constexpr int HorizontalMargin = 4;
constexpr int VerticalMargin = 2;

template <typename State>
void LayoutInput(State& state) {
    if (!state.input) return;
    const int clearWidth = state.text.empty() ? 0 : state.clearWidth;
    const int left = state.bounds.left + BorderWidth + HorizontalMargin + state.glyphWidth;
    const int top = state.bounds.top + BorderWidth + VerticalMargin;
    const int right = (std::max)(left, state.bounds.right - BorderWidth - HorizontalMargin - clearWidth);
    const int bottom = (std::max)(top, state.bounds.bottom - BorderWidth - VerticalMargin);
    state.input->SetBounds({left, top, right, bottom});
}
}

DuiSearchBox::DuiSearchBox() : searchBox_(std::make_unique<Impl>())
{
    SetPointerCursor(core::DuiPointerCursor::Arrow);
}
DuiSearchBox::~DuiSearchBox()
{
    if (!searchBox_) return;
    searchBox_->inputLifetime.reset();
    searchBox_->input = nullptr;
}
DuiSearchBox::DuiSearchBox(DuiSearchBox&&) noexcept = default;
DuiSearchBox& DuiSearchBox::operator=(DuiSearchBox&& other) noexcept {
    if (this != &other) {
        searchBox_->inputLifetime.reset();
        searchBox_->input = nullptr;
        core::Control::operator=(std::move(other));
        searchBox_ = std::move(other.searchBox_);
    }
    return *this;
}
void DuiSearchBox::SetTextInput(ui::DuiTextInput* input) {
    if (searchBox_->input == input) return;
    if (searchBox_->selecting) {
        if (searchBox_->input) searchBox_->input->EndSelection();
        searchBox_->selecting = false;
        SetCaptured(false);
    }
    searchBox_->inputLifetime.reset();
    if (searchBox_->input)
    {
        searchBox_->input->SetChangedHandler({});
        searchBox_->input->SetCancelHandler({});
    }
    searchBox_->input = input;
    if (!input) return;
    input->SetBorderVisible(false);
    input->SetOptions({false, false, false, searchBox_->readOnly, searchBox_->maxLength});
    input->SetPlaceholder(searchBox_->placeholder);
    searchBox_->inputLifetime = std::make_shared<int>();
    const std::weak_ptr<int> lifetime = searchBox_->inputLifetime;
    input->SetChangedHandler([lifetime, state = searchBox_.get()] {
        if (!lifetime.lock()) return;
        if (state->suppress) return;
        state->text = state->input->Text();
        LayoutInput(*state);
        if (state->changed) state->changed(state->text);
    });
    // Esc：清空搜索内容（Win32 原生 EDIT 焦点路径）
    input->SetCancelHandler([lifetime, this] {
        if (!lifetime.lock())
            return;
        if (!Text().empty())
            SetText({}, true);
        Layout(Bounds());
    });
    Layout(Bounds());
    SetText(searchBox_->text);
}
ui::DuiTextInput* DuiSearchBox::TextInput() const { return searchBox_->input; }
void DuiSearchBox::SetText(std::string text, bool notify) {
    searchBox_->text = std::move(text);
    if (searchBox_->input && searchBox_->input->Text() != searchBox_->text) {
        searchBox_->suppress = true; searchBox_->input->SetText(searchBox_->text); searchBox_->suppress = false;
    }
    LayoutInput(*searchBox_);
    if (notify && searchBox_->changed) searchBox_->changed(searchBox_->text);
}
std::string DuiSearchBox::Text() const { return searchBox_->input ? searchBox_->input->Text() : searchBox_->text; }
void DuiSearchBox::SetPlaceholder(std::string text) { searchBox_->placeholder = std::move(text); if (searchBox_->input) searchBox_->input->SetPlaceholder(searchBox_->placeholder); }
void DuiSearchBox::SetReadOnly(bool value) { searchBox_->readOnly = value; if (searchBox_->input) searchBox_->input->SetOptions({false, false, false, value, searchBox_->maxLength}); }
void DuiSearchBox::SetMaxLength(int length) { searchBox_->maxLength = (std::max)(0, length); if (searchBox_->input) searchBox_->input->SetOptions({false, false, false, searchBox_->readOnly, searchBox_->maxLength}); }
void DuiSearchBox::SetGlyphStripWidth(int width) { searchBox_->glyphWidth = (std::max)(0, width); Layout(Bounds()); }
void DuiSearchBox::SetClearStripWidth(int width) { searchBox_->clearWidth = (std::max)(14, width); Layout(Bounds()); }
bool DuiSearchBox::ClearShowing() const { return !Text().empty(); }
core::Rect DuiSearchBox::ClearRect() const { const auto bounds = Bounds(); return ClearShowing() ? core::Rect{bounds.right - searchBox_->clearWidth, bounds.top, bounds.right, bounds.bottom} : core::Rect{}; }
void DuiSearchBox::SetTextChangedHandler(std::function<void(std::string_view)> handler) { searchBox_->changed = std::move(handler); }
void DuiSearchBox::Layout(core::Rect bounds) {
    SetBounds(bounds);
    searchBox_->bounds = bounds;
    if (!searchBox_->input) return;
    LayoutInput(*searchBox_);
    searchBox_->input->SetVisible(EffectivelyVisible()); searchBox_->input->SetEnabled(Enabled());
}
core::Size DuiSearchBox::DesiredSize() const { return {180, 28}; }
bool DuiSearchBox::OnEvent(const core::Event& event) {
    if (!Enabled()) return false;
    if (event.type == core::EventType::KeyDown && event.key == core::key::Escape)
    {
        if (!Text().empty())
            SetText({}, true);
        Layout(Bounds());
        return true;
    }
    const core::Rect clear = ClearRect();
    const core::DuiPointerCursor outsideCursor = clear.Contains(event.position)
        ? core::DuiPointerCursor::Hand : core::DuiPointerCursor::Arrow;
    const core::Rect textBounds = searchBox_->input != nullptr
        ? searchBox_->input->Bounds()
        : core::Rect{Bounds().left + searchBox_->glyphWidth, Bounds().top,
                     ClearShowing() ? clear.left : Bounds().right, Bounds().bottom};
    if (ysDui::controls::detail::HandleTextInputPointer(
            *this, searchBox_->input, textBounds, event, searchBox_->selecting, outsideCursor))
    {
        return true;
    }
    if (event.type != core::EventType::PointerDown) return false;
    if (clear.Contains(event.position)) { SetText({}, true); Layout(Bounds()); return true; }
    if (Bounds().Contains(event.position)) return true;
    return false;
}
void DuiSearchBox::Paint(render::Canvas& canvas, core::Rect dirty) const {
    const core::Rect bounds = Bounds();
    const core::Rect clipped = core::Rect::Intersect(bounds, dirty);
    if (!EffectivelyVisible() || clipped.Empty()) return;
    canvas.FillRect(clipped, Enabled() ? Theme().Get(core::ThemeSlot::SurfaceBackground)
                                       : Theme().Get(core::ThemeSlot::FieldPlainDisabledBackground));
    const core::Color border = !Enabled() ? Theme().Get(core::ThemeSlot::FieldDisabledBorder)
        : Focused() ? Theme().Get(core::ThemeSlot::FieldFocusBorder) : Theme().Get(core::ThemeSlot::FieldBorder);
    canvas.StrokeRoundedRect(bounds, 0, border, 1.0f);
    const int centerX = bounds.left + searchBox_->glyphWidth / 2;
    const int centerY = (bounds.top + bounds.bottom) / 2;
    canvas.StrokeArc({centerX - 6, centerY - 7, centerX + 4, centerY + 3}, 0.0f, 360.0f,
                     Theme().Get(core::ThemeSlot::SearchGlyph), 1.5f);
    render::DuiPath handle; handle.MoveTo({centerX + 3, centerY + 2}); handle.LineTo({centerX + 8, centerY + 7});
    canvas.StrokePath(handle, Theme().Get(core::ThemeSlot::SearchGlyph), 1.5f);
    if (ClearShowing()) { render::DuiPath cross; const auto clear = ClearRect(); const int x = (clear.left + clear.right) / 2; const int y = (clear.top + clear.bottom) / 2; cross.MoveTo({x - 5, y - 5}); cross.LineTo({x + 5, y + 5}); cross.MoveTo({x + 5, y - 5}); cross.LineTo({x - 5, y + 5}); canvas.StrokePath(cross, Theme().Get(core::ThemeSlot::SearchClear), 1.5f); }
    render::DuiTextStyle style;
    style.color = Enabled() ? Theme().Get(core::ThemeSlot::ControlText)
                            : Theme().Get(core::ThemeSlot::FieldDisabledText);
    const core::Color placeholder = Enabled() ? Theme().Get(core::ThemeSlot::SearchPlaceholder)
                                              : Theme().Get(core::ThemeSlot::FieldDisabledText);
    const int right = ClearShowing() ? ClearRect().left - 2 : bounds.right - 4;
    ysDui::controls::detail::PaintTextInput(
        canvas, searchBox_->input, searchBox_->text, searchBox_->placeholder,
        {bounds.left + searchBox_->glyphWidth, bounds.top, right, bounds.bottom}, style,
        placeholder, render::DuiTextAlignment::Start, false,
        Focused() && Enabled() && !searchBox_->readOnly,
        Theme().Get(core::ThemeSlot::TextSelectionBackground),
        Theme().Get(core::ThemeSlot::TextSelectionText));
}
} // namespace ysDui::controls::input
