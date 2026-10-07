#include "ysDui/controls/input/DuiSpinBox.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <utility>

#include "DuiTextInputPaint.hpp"
#include "DuiTextInputPointer.hpp"
#include "ysDui/core/DuiTheme.hpp"
namespace ysDui::controls::input {
class DuiSpinBox::Impl { public: int minimum{}; int maximum{100}; int value{}; int step{1}; bool wrap{}; int hovered{}; int pressed{}; bool suppressInput{}; bool selecting{}; ui::DuiTextInput* textInput{}; std::shared_ptr<int> textInputLifetime; std::function<void(int)> changed; };
namespace {
constexpr int DEFAULT_WIDTH = 120;
constexpr int DEFAULT_HEIGHT = 23;
constexpr int SPIN_STRIP_WIDTH = 18;
constexpr int TEXT_HORIZONTAL_OFFSET = 2;
constexpr int TEXT_VERTICAL_OFFSET = 2;
constexpr int TEXT_POINT_SIZE = 12;
constexpr int ARROW_HALF_WIDTH = 3;
constexpr int ARROW_HALF_HEIGHT = 2;

/** 按控件高度相对默认高度等比缩放（向下取整），使内嵌于缩放画布时与节点比例一致。 */
int ScaleByHeight(int value, int height, int minimum = 1)
{
    const int h = (std::max)(1, height);
    return (std::max)(minimum, value * h / DEFAULT_HEIGHT);
}

int StripWidthFor(const core::Rect& bounds)
{
    return ScaleByHeight(SPIN_STRIP_WIDTH, bounds.Height(), 10);
}

int TextPointSizeFor(const core::Rect& bounds)
{
    return (std::clamp)(ScaleByHeight(TEXT_POINT_SIZE, bounds.Height(), 8), 8, 96);
}

core::Rect InputTextRect(const DuiSpinBox& spinBox)
{
    const core::Rect bounds = spinBox.TextRect();
    const int padX = ScaleByHeight(TEXT_HORIZONTAL_OFFSET, spinBox.Bounds().Height());
    const int padY = ScaleByHeight(TEXT_VERTICAL_OFFSET, spinBox.Bounds().Height());
    // 垂直方向按上/下对称内缩，保持文本中心与控件中心重合；
    // 仅缩底边会使文本整体偏上（文本按段落居中绘制，矩形中心即视觉中心）。
    const int top = bounds.top + padY;
    return {bounds.left + padX, top, bounds.right, (std::max)(top, bounds.bottom - padY)};
}

std::string FormatValue(int value) {
    const std::string text = std::to_string(value);
    return {text.begin(), text.end()};
}
std::string SanitizeText(std::string_view text, bool allowMinus) {
    std::string result;
    for (const char character : text) {
        if (character >= '0' && character <= '9') result += character;
        else if (character == '-' && allowMinus && result.empty()) result += character;
    }
    return result;
}
bool ParseText(std::string_view text, int& value) {
    return DuiSpinBox::TryParseInt(text, value);
}
template <typename State>
void SyncInputValue(State& state) {
    if (!state.textInput) return;
    const std::string text = FormatValue(state.value);
    if (state.textInput->Text() == text) return;
    state.suppressInput = true;
    state.textInput->SetText(text);
    state.suppressInput = false;
}
template <typename State>
void CommitInputValue(State& state, bool notify) {
    if (!state.textInput || state.suppressInput) return;
    int value{};
    if (!ParseText(state.textInput->Text(), value)) { SyncInputValue(state); return; }
    const int normalized = DuiSpinBox::ClampOrWrap(value, state.minimum, state.maximum, state.wrap);
    const bool changed = normalized != state.value;
    state.value = normalized;
    SyncInputValue(state);
    if (changed && notify && state.changed) state.changed(normalized);
}
}
DuiSpinBox::DuiSpinBox() : spinBox_(std::make_unique<Impl>()) {}
DuiSpinBox::~DuiSpinBox()
{
    if (!spinBox_) return;
    spinBox_->textInputLifetime.reset();
    spinBox_->textInput = nullptr;
}
DuiSpinBox::DuiSpinBox(DuiSpinBox&&) noexcept = default;
DuiSpinBox& DuiSpinBox::operator=(DuiSpinBox&& other) noexcept {
    if (this != &other) {
        spinBox_->textInputLifetime.reset();
        spinBox_->textInput = nullptr;
        core::Control::operator=(std::move(other));
        spinBox_ = std::move(other.spinBox_);
    }
    return *this;
}
void DuiSpinBox::SetRange(int minimum, int maximum)
{
    if (minimum > maximum)
        std::swap(minimum, maximum);
    spinBox_->minimum = minimum;
    spinBox_->maximum = maximum;
    spinBox_->value = ClampOrWrap(spinBox_->value, minimum, maximum, false);
    SyncInputValue(*spinBox_);
}

void DuiSpinBox::SetStep(int step)
{
    spinBox_->step = (std::max)(1, step);
}

void DuiSpinBox::SetWrap(bool wrap)
{
    spinBox_->wrap = wrap;
}

int DuiSpinBox::Value() const
{
    return spinBox_->value;
}

int DuiSpinBox::Minimum() const
{
    return spinBox_->minimum;
}

int DuiSpinBox::Maximum() const
{
    return spinBox_->maximum;
}
int DuiSpinBox::ClampOrWrap(int v,int a,int b,bool wrap){if(a>b)std::swap(a,b);if(!wrap)return (std::clamp)(v,a,b);const long long span=static_cast<long long>(b)-a+1; if(span<=0)return a; long long offset=(static_cast<long long>(v)-a)%span;if(offset<0)offset+=span;return static_cast<int>(a+offset);}
bool DuiSpinBox::TryParseInt(std::string_view text,int& value){if(text.empty()||text=="-")return false;const auto r=std::from_chars(text.data(),text.data()+text.size(),value);return r.ec==std::errc{}&&r.ptr==text.data()+text.size();}
void DuiSpinBox::SetValue(int v,bool notify){const int n=ClampOrWrap(v,spinBox_->minimum,spinBox_->maximum,spinBox_->wrap);const bool changed=n!=spinBox_->value;spinBox_->value=n;SyncInputValue(*spinBox_);if(changed&&notify&&spinBox_->changed)spinBox_->changed(n);} void DuiSpinBox::SetValueChangedHandler(std::function<void(int)> h){spinBox_->changed=std::move(h);}
core::Rect DuiSpinBox::UpRect() const
{
    const auto bounds = Bounds();
    const int strip = StripWidthFor(bounds);
    const int inset = ScaleByHeight(1, bounds.Height());
    return {bounds.right - strip, bounds.top + inset, bounds.right - inset,
            (bounds.top + bounds.bottom) / 2};
}

core::Rect DuiSpinBox::DownRect() const
{
    const auto bounds = Bounds();
    const int strip = StripWidthFor(bounds);
    const int inset = ScaleByHeight(1, bounds.Height());
    return {bounds.right - strip, (bounds.top + bounds.bottom) / 2,
            bounds.right - inset, bounds.bottom - inset};
}

core::Rect DuiSpinBox::TextRect() const
{
    const auto bounds = Bounds();
    const int inset = ScaleByHeight(1, bounds.Height());
    return {bounds.left + inset, bounds.top + inset, UpRect().left, bounds.bottom - inset};
}

core::Size DuiSpinBox::DesiredSize() const
{
    return {DEFAULT_WIDTH, DEFAULT_HEIGHT};
}
void DuiSpinBox::Layout(core::Rect bounds){SetBounds(bounds);if(spinBox_->textInput){spinBox_->textInput->SetBounds(InputTextRect(*this));spinBox_->textInput->SetVisible(EffectivelyVisible());spinBox_->textInput->SetEnabled(Enabled());}}
void DuiSpinBox::SetTextInput(ui::DuiTextInput* input){
    if (spinBox_->textInput == input) return;
    if (spinBox_->selecting) {
        if (spinBox_->textInput) spinBox_->textInput->EndSelection();
        spinBox_->selecting = false;
        SetCaptured(false);
    }
    spinBox_->textInputLifetime.reset();
    if (spinBox_->textInput) { spinBox_->textInput->SetChangedHandler({}); spinBox_->textInput->SetFocusLostHandler({}); }
    spinBox_->textInput = input;
    if (!input) return;
    input->SetBorderVisible(false);
    spinBox_->textInputLifetime = std::make_shared<int>();
    const std::weak_ptr<int> lifetime = spinBox_->textInputLifetime;
    input->SetChangedHandler([lifetime, state = spinBox_.get()] {
        if (!lifetime.lock()) return;
        if (state->suppressInput) return;
        const std::string text = SanitizeText(state->textInput->Text(), state->minimum < 0);
        if (text == state->textInput->Text()) return;
        state->suppressInput = true;
        state->textInput->SetText(text);
        state->suppressInput = false;
    });
    input->SetFocusLostHandler([lifetime, state = spinBox_.get()] {
        if (lifetime.lock()) CommitInputValue(*state, true);
    });
    input->SetBounds(InputTextRect(*this)); input->SetVisible(EffectivelyVisible()); input->SetEnabled(Enabled()); SyncInputValue(*spinBox_);
}
ui::DuiTextInput* DuiSpinBox::TextInput() const{return spinBox_->textInput;}
void DuiSpinBox::CommitTextInput(bool notify){CommitInputValue(*spinBox_,notify);}
bool DuiSpinBox::OnEvent(const core::Event& event)
{
    if (!Enabled()) return false;
    if (event.type == core::EventType::PointerMove)
    {
        spinBox_->hovered = UpRect().Contains(event.position) ? 1
            : DownRect().Contains(event.position) ? -1 : 0;
        SetHovered(Bounds().Contains(event.position));
        if (ysDui::controls::detail::HandleTextInputPointer(
                *this, spinBox_->textInput, TextRect(), event, spinBox_->selecting,
                core::DuiPointerCursor::Arrow))
        {
            return true;
        }
        return Hovered();
    }
    if (event.type == core::EventType::PointerLeave)
    {
        spinBox_->hovered = 0;
        SetHovered(false);
        (void)ysDui::controls::detail::HandleTextInputPointer(
            *this, spinBox_->textInput, TextRect(), event, spinBox_->selecting,
            core::DuiPointerCursor::Arrow);
        return true;
    }
    if (event.type == core::EventType::PointerDown)
    {
        if (UpRect().Contains(event.position)) { spinBox_->pressed = 1; SetCaptured(true); return true; }
        if (DownRect().Contains(event.position)) { spinBox_->pressed = -1; SetCaptured(true); return true; }
        return ysDui::controls::detail::HandleTextInputPointer(
            *this, spinBox_->textInput, TextRect(), event, spinBox_->selecting,
            core::DuiPointerCursor::Arrow);
    }
    if ((event.type == core::EventType::PointerCancel || event.type == core::EventType::PointerUp)
        && spinBox_->selecting)
    {
        return ysDui::controls::detail::HandleTextInputPointer(
            *this, spinBox_->textInput, TextRect(), event, spinBox_->selecting,
            core::DuiPointerCursor::Arrow);
    }
    if (event.type == core::EventType::PointerCancel && spinBox_->pressed)
    {
        spinBox_->pressed = 0;
        SetCaptured(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && spinBox_->pressed)
    {
        const int direction = spinBox_->pressed;
        spinBox_->pressed = 0;
        SetCaptured(false);
        if ((direction > 0 ? UpRect() : DownRect()).Contains(event.position))
        {
            CommitTextInput(false);
            SetValue(spinBox_->value + direction * spinBox_->step, true);
        }
        return true;
    }
    if (event.type == core::EventType::PointerWheel)
    {
        CommitTextInput(false);
        SetValue(spinBox_->value + (event.wheelDelta > 0 ? spinBox_->step : -spinBox_->step), true);
        return true;
    }
    return false;
}
void DuiSpinBox::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = Bounds();
    const core::Rect clipped = core::Rect::Intersect(bounds, dirty);
    if (!EffectivelyVisible() || clipped.Empty())
        return;

    const auto up = UpRect();
    const auto down = DownRect();
    canvas.FillRect(clipped, Enabled() ? Theme().Get(core::ThemeSlot::SurfaceBackground)
                                       : Theme().Get(core::ThemeSlot::FieldPlainDisabledBackground));
    if (spinBox_->hovered == 1 || spinBox_->pressed == 1)
    {
        canvas.FillRect(up, spinBox_->pressed == 1 ? Theme().Get(core::ThemeSlot::SpinButtonPressed)
                                                   : Theme().Get(core::ThemeSlot::SpinButtonHover));
    }
    if (spinBox_->hovered == -1 || spinBox_->pressed == -1)
    {
        canvas.FillRect(down, spinBox_->pressed == -1 ? Theme().Get(core::ThemeSlot::SpinButtonPressed)
                                                      : Theme().Get(core::ThemeSlot::SpinButtonHover));
    }
    const core::Color border = !Enabled() ? Theme().Get(core::ThemeSlot::FieldDisabledBorder)
        : Hovered() || spinBox_->pressed != 0 ? Theme().Get(core::ThemeSlot::FieldHoverBorder)
        : Focused() ? Theme().Get(core::ThemeSlot::FieldFocusBorder) : Theme().Get(core::ThemeSlot::FieldBorder);
    const float borderWidth = static_cast<float>(ScaleByHeight(1, bounds.Height()));
    canvas.StrokeRoundedRect(bounds, 0, border, borderWidth);
    render::DuiTextStyle style;
    style.color = Enabled() ? Theme().Get(core::ThemeSlot::SpinText)
                            : Theme().Get(core::ThemeSlot::SpinDisabledText);
    style.pointSize = TextPointSizeFor(bounds);
    ysDui::controls::detail::PaintTextInput(
        canvas, spinBox_->textInput, FormatValue(spinBox_->value), {}, InputTextRect(*this), style,
        style.color, render::DuiTextAlignment::Start, false,
        Focused() && Enabled() && spinBox_->textInput != nullptr,
        Theme().Get(core::ThemeSlot::TextSelectionBackground),
        Theme().Get(core::ThemeSlot::TextSelectionText));

    const int arrowHalfW = ScaleByHeight(ARROW_HALF_WIDTH, bounds.Height());
    const int arrowHalfH = ScaleByHeight(ARROW_HALF_HEIGHT, bounds.Height());
    const auto drawArrow = [&canvas, this, arrowHalfW, arrowHalfH](core::Rect rect, bool isUp)
    {
        const int x = (rect.left + rect.right) / 2;
        const int y = (rect.top + rect.bottom) / 2;
        render::DuiPath path;
        if (isUp)
        {
            path.MoveTo({x, y - arrowHalfH});
            path.LineTo({x - arrowHalfW, y + 1});
            path.LineTo({x + arrowHalfW, y + 1});
        }
        else
        {
            path.MoveTo({x, y + arrowHalfH});
            path.LineTo({x - arrowHalfW, y - 1});
            path.LineTo({x + arrowHalfW, y - 1});
        }
        path.Close();
        canvas.FillPath(path, Theme().Get(core::ThemeSlot::TextDefault));
    };
    drawArrow(up, true);
    drawArrow(down, false);
}
} // namespace ysDui::controls::input
