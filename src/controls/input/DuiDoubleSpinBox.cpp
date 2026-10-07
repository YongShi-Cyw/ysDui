#include "ysDui/controls/input/DuiDoubleSpinBox.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#include "DuiTextInputPaint.hpp"
#include "DuiTextInputPointer.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::input {
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

core::Rect InputTextRect(const DuiDoubleSpinBox& spinBox)
{
    const core::Rect bounds = spinBox.TextRect();
    const int padX = ScaleByHeight(TEXT_HORIZONTAL_OFFSET, spinBox.Bounds().Height());
    const int padY = ScaleByHeight(TEXT_VERTICAL_OFFSET, spinBox.Bounds().Height());
    // 垂直方向按上/下对称内缩，保持文本中心与控件中心重合；
    // 仅缩底边会使文本整体偏上（文本按段落居中绘制，矩形中心即视觉中心）。
    const int top = bounds.top + padY;
    return {bounds.left + padX, top, bounds.right, (std::max)(top, bounds.bottom - padY)};
}

bool NearlyEqual(double first, double second) {
    const double scale = (std::max)(std::abs(first), std::abs(second));
    return std::abs(first - second) <= 1e-9 * (std::max)(1.0, scale);
}

template <typename State>
double Quantize(double value, const State& state) {
    const double scale = std::pow(10.0, state.decimals);
    return std::round(value * scale) / scale;
}

template <typename State>
std::string FormatValue(const State& state) {
    char text[128]{};
    std::snprintf(text, sizeof(text), "%.*f", state.decimals, state.value);
    return {text, text + std::char_traits<char>::length(text)};
}

std::string SanitizeText(std::string_view text, bool allowMinus, int decimals) {
    std::string result;
    bool hasDecimal{};
    int fractionDigits{};
    for (const char character : text) {
        if (character >= '0' && character <= '9') {
            if (!hasDecimal || fractionDigits++ < decimals) result += character;
        } else if (character == '-' && allowMinus && result.empty()) {
            result += character;
        } else if (character == '.' && !hasDecimal) {
            hasDecimal = true;
            result += character;
        }
    }
    return result;
}

bool ParseText(std::string_view text, double& value) {
    return DuiDoubleSpinBox::TryParseDouble(text, value);
}

template <typename State>
void SyncInput(State& state) {
    if (!state.textInput) return;
    const std::string text = FormatValue(state);
    if (state.textInput->Text() == text) return;
    state.suppressInput = true;
    state.textInput->SetText(text);
    state.suppressInput = false;
}

template <typename State>
void CommitInput(State& state, bool notify) {
    if (!state.textInput || state.suppressInput) return;
    double value{};
    if (!ParseText(state.textInput->Text(), value)) { SyncInput(state); return; }
    const double normalized = Quantize(DuiDoubleSpinBox::ClampOrWrap(value, state.minimum, state.maximum, state.wrap), state);
    const bool changed = !NearlyEqual(normalized, state.value);
    state.value = normalized;
    SyncInput(state);
    if (changed && notify && state.changed) state.changed(normalized);
}
}

class DuiDoubleSpinBox::Impl {
public:
    double minimum{};
    double maximum{100.0};
    double value{};
    double step{1.0};
    int decimals{2};
    int hovered{};
    int pressed{};
    bool wrap{};
    bool suppressInput{};
    bool selecting{};
    ui::DuiTextInput* textInput{};
    std::shared_ptr<int> textInputLifetime;
    std::function<void(double)> changed;
};

DuiDoubleSpinBox::DuiDoubleSpinBox() : doubleSpinBox_(std::make_unique<Impl>()) {}
DuiDoubleSpinBox::~DuiDoubleSpinBox()
{
    if (!doubleSpinBox_) return;
    doubleSpinBox_->textInputLifetime.reset();
    doubleSpinBox_->textInput = nullptr;
}
DuiDoubleSpinBox::DuiDoubleSpinBox(DuiDoubleSpinBox&&) noexcept = default;
DuiDoubleSpinBox& DuiDoubleSpinBox::operator=(DuiDoubleSpinBox&& other) noexcept {
    if (this != &other) {
        doubleSpinBox_->textInputLifetime.reset();
        doubleSpinBox_->textInput = nullptr;
        core::Control::operator=(std::move(other));
        doubleSpinBox_ = std::move(other.doubleSpinBox_);
    }
    return *this;
}
void DuiDoubleSpinBox::SetRange(double minimum, double maximum) {
    if (minimum > maximum) std::swap(minimum, maximum);
    doubleSpinBox_->minimum = minimum;
    doubleSpinBox_->maximum = maximum;
    doubleSpinBox_->value = Quantize(ClampOrWrap(doubleSpinBox_->value, minimum, maximum, false), *doubleSpinBox_);
    SyncInput(*doubleSpinBox_);
}
void DuiDoubleSpinBox::SetStep(double step) { doubleSpinBox_->step = step > 0.0 ? step : 0.01; }
void DuiDoubleSpinBox::SetDecimals(int decimals) {
    doubleSpinBox_->decimals = (std::clamp)(decimals, 0, 9);
    doubleSpinBox_->value = Quantize(doubleSpinBox_->value, *doubleSpinBox_);
    SyncInput(*doubleSpinBox_);
}
void DuiDoubleSpinBox::SetWrap(bool wrap) { doubleSpinBox_->wrap = wrap; }
void DuiDoubleSpinBox::SetValue(double value, bool notify) {
    const double normalized = Quantize(ClampOrWrap(value, Minimum(), Maximum(), doubleSpinBox_->wrap), *doubleSpinBox_);
    const bool changed = !NearlyEqual(normalized, doubleSpinBox_->value);
    doubleSpinBox_->value = normalized;
    SyncInput(*doubleSpinBox_);
    if (changed && notify && doubleSpinBox_->changed) doubleSpinBox_->changed(normalized);
}
double DuiDoubleSpinBox::Value() const { return doubleSpinBox_->value; }
double DuiDoubleSpinBox::Minimum() const { return doubleSpinBox_->minimum; }
double DuiDoubleSpinBox::Maximum() const { return doubleSpinBox_->maximum; }
int DuiDoubleSpinBox::Decimals() const { return doubleSpinBox_->decimals; }
double DuiDoubleSpinBox::ClampOrWrap(double value, double minimum, double maximum, bool wrap) {
    if (minimum > maximum) std::swap(minimum, maximum);
    if (!wrap) return (std::clamp)(value, minimum, maximum);
    const double range = maximum - minimum;
    if (range <= 0.0) return minimum;
    double offset = std::fmod(value - minimum, range);
    if (offset < 0.0) offset += range;
    return NearlyEqual(offset, 0.0) && value >= maximum ? maximum : minimum + offset;
}
bool DuiDoubleSpinBox::TryParseDouble(std::string_view text, double& value) {
    if (text.empty() || text == "-" || text == "." || text == "-.") return false;
    bool decimal{};
    bool digit{};
    std::size_t index = text.front() == '-' ? 1 : 0;
    for (; index < text.size(); ++index) {
        const char character = text[index];
        if (character >= '0' && character <= '9') digit = true;
        else if (character == '.' && !decimal) decimal = true;
        else return false;
    }
    if (!digit) return false;
    const std::string owned(text);
    char* end{};
    value = std::strtod(owned.c_str(), &end);
    return end == owned.c_str() + owned.size() && std::isfinite(value);
}
void DuiDoubleSpinBox::SetValueChangedHandler(std::function<void(double)> handler) { doubleSpinBox_->changed = std::move(handler); }
core::Rect DuiDoubleSpinBox::UpRect() const
{
    const auto bounds = Bounds();
    const int strip = StripWidthFor(bounds);
    const int inset = ScaleByHeight(1, bounds.Height());
    return {bounds.right - strip, bounds.top + inset, bounds.right - inset,
            (bounds.top + bounds.bottom) / 2};
}

core::Rect DuiDoubleSpinBox::DownRect() const
{
    const auto bounds = Bounds();
    const int strip = StripWidthFor(bounds);
    const int inset = ScaleByHeight(1, bounds.Height());
    return {bounds.right - strip, (bounds.top + bounds.bottom) / 2,
            bounds.right - inset, bounds.bottom - inset};
}

core::Rect DuiDoubleSpinBox::TextRect() const
{
    const auto bounds = Bounds();
    const int inset = ScaleByHeight(1, bounds.Height());
    return {bounds.left + inset, bounds.top + inset, UpRect().left, bounds.bottom - inset};
}

core::Size DuiDoubleSpinBox::DesiredSize() const
{
    return {DEFAULT_WIDTH, DEFAULT_HEIGHT};
}
void DuiDoubleSpinBox::Layout(core::Rect bounds) {
    SetBounds(bounds);
    if (doubleSpinBox_->textInput) {
        doubleSpinBox_->textInput->SetBounds(InputTextRect(*this));
        doubleSpinBox_->textInput->SetVisible(EffectivelyVisible());
        doubleSpinBox_->textInput->SetEnabled(Enabled());
    }
}
void DuiDoubleSpinBox::SetTextInput(ui::DuiTextInput* input) {
    if (doubleSpinBox_->textInput == input) return;
    if (doubleSpinBox_->selecting) {
        if (doubleSpinBox_->textInput) doubleSpinBox_->textInput->EndSelection();
        doubleSpinBox_->selecting = false;
        SetCaptured(false);
    }
    doubleSpinBox_->textInputLifetime.reset();
    if (doubleSpinBox_->textInput) { doubleSpinBox_->textInput->SetChangedHandler({}); doubleSpinBox_->textInput->SetFocusLostHandler({}); }
    doubleSpinBox_->textInput = input;
    if (!input) return;
    input->SetBorderVisible(false);
    doubleSpinBox_->textInputLifetime = std::make_shared<int>();
    const std::weak_ptr<int> lifetime = doubleSpinBox_->textInputLifetime;
    input->SetChangedHandler([lifetime, state = doubleSpinBox_.get()] {
        if (!lifetime.lock()) return;
        if (state->suppressInput) return;
        const std::string text = SanitizeText(state->textInput->Text(), state->minimum < 0.0, state->decimals);
        if (text == state->textInput->Text()) return;
        state->suppressInput = true;
        state->textInput->SetText(text);
        state->suppressInput = false;
    });
    input->SetFocusLostHandler([lifetime, state = doubleSpinBox_.get()] {
        if (lifetime.lock()) CommitInput(*state, true);
    });
    input->SetBounds(InputTextRect(*this));
    input->SetVisible(EffectivelyVisible());
    input->SetEnabled(Enabled());
    SyncInput(*doubleSpinBox_);
}
ui::DuiTextInput* DuiDoubleSpinBox::TextInput() const { return doubleSpinBox_->textInput; }
void DuiDoubleSpinBox::CommitTextInput(bool notify) { CommitInput(*doubleSpinBox_, notify); }
bool DuiDoubleSpinBox::OnEvent(const core::Event& event) {
    if (!Enabled()) return false;
    if (event.type == core::EventType::PointerMove) {
        doubleSpinBox_->hovered = UpRect().Contains(event.position) ? 1 : DownRect().Contains(event.position) ? -1 : 0;
        SetHovered(Bounds().Contains(event.position));
        if (ysDui::controls::detail::HandleTextInputPointer(
                *this, doubleSpinBox_->textInput, TextRect(), event, doubleSpinBox_->selecting,
                core::DuiPointerCursor::Arrow)) {
            return true;
        }
        return Hovered();
    }
    if (event.type == core::EventType::PointerLeave) {
        doubleSpinBox_->hovered = 0;
        SetHovered(false);
        (void)ysDui::controls::detail::HandleTextInputPointer(
            *this, doubleSpinBox_->textInput, TextRect(), event, doubleSpinBox_->selecting,
            core::DuiPointerCursor::Arrow);
        return true;
    }
    if (event.type == core::EventType::PointerDown) {
        if (UpRect().Contains(event.position)) { doubleSpinBox_->pressed = 1; SetCaptured(true); return true; }
        if (DownRect().Contains(event.position)) { doubleSpinBox_->pressed = -1; SetCaptured(true); return true; }
        return ysDui::controls::detail::HandleTextInputPointer(
            *this, doubleSpinBox_->textInput, TextRect(), event, doubleSpinBox_->selecting,
            core::DuiPointerCursor::Arrow);
    }
    if ((event.type == core::EventType::PointerCancel || event.type == core::EventType::PointerUp)
        && doubleSpinBox_->selecting) {
        return ysDui::controls::detail::HandleTextInputPointer(
            *this, doubleSpinBox_->textInput, TextRect(), event, doubleSpinBox_->selecting,
            core::DuiPointerCursor::Arrow);
    }
    if (event.type == core::EventType::PointerCancel && doubleSpinBox_->pressed) {
        doubleSpinBox_->pressed = 0;
        SetCaptured(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && doubleSpinBox_->pressed) {
        const int direction = doubleSpinBox_->pressed;
        doubleSpinBox_->pressed = 0;
        SetCaptured(false);
        if ((direction > 0 ? UpRect() : DownRect()).Contains(event.position)) {
            CommitTextInput(false);
            SetValue(Value() + direction * doubleSpinBox_->step, true);
        }
        return true;
    }
    if (event.type == core::EventType::PointerWheel) {
        CommitTextInput(false);
        SetValue(Value() + (event.wheelDelta > 0 ? doubleSpinBox_->step : -doubleSpinBox_->step), true);
        return true;
    }
    return false;
}
void DuiDoubleSpinBox::Paint(render::Canvas& canvas, core::Rect dirty) const {
    const core::Rect bounds = Bounds();
    const core::Rect clipped = core::Rect::Intersect(bounds, dirty);
    if (!EffectivelyVisible() || clipped.Empty()) return;
    const auto up = UpRect();
    const auto down = DownRect();
    canvas.FillRect(clipped, Enabled() ? Theme().Get(core::ThemeSlot::SurfaceBackground)
                                       : Theme().Get(core::ThemeSlot::FieldPlainDisabledBackground));
    if (doubleSpinBox_->hovered == 1 || doubleSpinBox_->pressed == 1)
        canvas.FillRect(up, doubleSpinBox_->pressed == 1 ? Theme().Get(core::ThemeSlot::SpinButtonPressed)
                                                         : Theme().Get(core::ThemeSlot::SpinButtonHover));
    if (doubleSpinBox_->hovered == -1 || doubleSpinBox_->pressed == -1)
        canvas.FillRect(down, doubleSpinBox_->pressed == -1 ? Theme().Get(core::ThemeSlot::SpinButtonPressed)
                                                            : Theme().Get(core::ThemeSlot::SpinButtonHover));
    const core::Color border = !Enabled() ? Theme().Get(core::ThemeSlot::FieldDisabledBorder)
        : Hovered() || doubleSpinBox_->pressed != 0 ? Theme().Get(core::ThemeSlot::FieldHoverBorder)
        : Focused() ? Theme().Get(core::ThemeSlot::FieldFocusBorder) : Theme().Get(core::ThemeSlot::FieldBorder);
    const float borderWidth = static_cast<float>(ScaleByHeight(1, bounds.Height()));
    canvas.StrokeRoundedRect(bounds, 0, border, borderWidth);
    render::DuiTextStyle style;
    style.color = Enabled() ? Theme().Get(core::ThemeSlot::SpinText)
                            : Theme().Get(core::ThemeSlot::SpinDisabledText);
    style.pointSize = TextPointSizeFor(bounds);
    ysDui::controls::detail::PaintTextInput(
        canvas, doubleSpinBox_->textInput, FormatValue(*doubleSpinBox_), {}, InputTextRect(*this), style,
        style.color, render::DuiTextAlignment::Start, false,
        Focused() && Enabled() && doubleSpinBox_->textInput != nullptr,
        Theme().Get(core::ThemeSlot::TextSelectionBackground),
        Theme().Get(core::ThemeSlot::TextSelectionText));
    const int arrowHalfW = ScaleByHeight(ARROW_HALF_WIDTH, bounds.Height());
    const int arrowHalfH = ScaleByHeight(ARROW_HALF_HEIGHT, bounds.Height());
    const auto drawArrow = [&canvas, this, arrowHalfW, arrowHalfH](core::Rect rect, bool isUp) {
        const int x = (rect.left + rect.right) / 2;
        const int y = (rect.top + rect.bottom) / 2;
        render::DuiPath path;
        if (isUp) {
            path.MoveTo({x, y - arrowHalfH});
            path.LineTo({x - arrowHalfW, y + 1});
            path.LineTo({x + arrowHalfW, y + 1});
        } else {
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
