#include "ysDui/controls/input/DuiDateTimePicker.hpp"

#include "ysDui/controls/input/DuiMonthCalendar.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiFocusVisual.hpp"
#include "ysDui/render/DuiPopupSurface.hpp"

namespace ysDui::controls::input {
namespace {
constexpr int CalendarWidth = 240;
constexpr int CalendarHeight = 230;
constexpr int HeaderHeight = 36;
constexpr int WeekHeight = 22;
constexpr int TimePanelHeight = 56;
constexpr int TimeApplyHeight = 28;
constexpr int PickerHeight = 25;
constexpr int DropWidth = 22;

int Weekday(int year, int month, int day)
{
    static constexpr std::array<int, 12> offsets{0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) --year;
    return (year + year / 4 - year / 100 + year / 400 + offsets[static_cast<std::size_t>(month - 1)] + day) % 7;
}

core::DateTime Normalized(core::DateTime value, core::DateTime fallback)
{
    if (!core::IsValidDate(value.year, value.month, value.day)) {
        value.year = fallback.year;
        value.month = fallback.month;
        value.day = fallback.day;
    }
    if (!core::IsValidTime(value.hour, value.minute, value.second)) value.hour = value.minute = value.second = 0;
    return value;
}

core::DateTime Clamp(core::DateTime value, core::DateTime fallback,
                     const std::optional<core::DateTime>& minimum, const std::optional<core::DateTime>& maximum)
{
    value = Normalized(value, fallback);
    if (minimum && core::CompareDate(value, *minimum) < 0) {
        value.year = minimum->year;
        value.month = minimum->month;
        value.day = minimum->day;
    }
    if (maximum && core::CompareDate(value, *maximum) > 0) {
        value.year = maximum->year;
        value.month = maximum->month;
        value.day = maximum->day;
    }
    return value;
}

void AppendNumber(std::string& target, int value, int width)
{
    std::array<char, 4> digits{};
    for (int index = width - 1; index >= 0; --index) {
        digits[static_cast<std::size_t>(index)] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    target.append(digits.data(), static_cast<std::size_t>(width));
}

bool ParseNumber(std::string_view text, std::size_t& position, int minimumDigits, int maximumDigits, int& value)
{
    value = 0;
    int count{};
    while (position < text.size() && count < maximumDigits && text[position] >= '0' && text[position] <= '9') {
        value = value * 10 + text[position++] - '0';
        ++count;
    }
    return count >= minimumDigits;
}

bool Consume(std::string_view text, std::size_t& position, char separator)
{
    if (position >= text.size() || text[position] != separator) return false;
    ++position;
    return true;
}

std::string_view DefaultFormat(DuiDateTimePicker::Mode mode)
{
    switch (mode) {
    case DuiDateTimePicker::Mode::Date:
        return "%Y-%m-%d";
    case DuiDateTimePicker::Mode::Time:
        return "%H:%M:%S";
    case DuiDateTimePicker::Mode::DateTime:
        return "%Y-%m-%d %H:%M:%S";
    }
    return {};
}
}

class DuiMonthCalendar::Impl {
public:
    int viewYear{2000};
    int viewMonth{1};
    core::DateTime selected{2000, 1, 1};
    std::optional<core::DateTime> minimum;
    std::optional<core::DateTime> maximum;
    std::function<void(core::DateTime)> selectedHandler;
    render::DuiTextStyle textStyle;
};

DuiMonthCalendar::DuiMonthCalendar() : calendar_(std::make_unique<Impl>()) {}
DuiMonthCalendar::~DuiMonthCalendar() = default;

void DuiMonthCalendar::SetViewMonth(int year, int month)
{
    if (year < 1) year = 1;
    while (month < 1) { month += 12; --year; }
    while (month > 12) { month -= 12; ++year; }
    calendar_->viewYear = year;
    calendar_->viewMonth = month;
}

int DuiMonthCalendar::ViewYear() const { return calendar_->viewYear; }
int DuiMonthCalendar::ViewMonth() const { return calendar_->viewMonth; }

void DuiMonthCalendar::SetSelected(core::DateTime value)
{
    calendar_->selected = Clamp(value, calendar_->selected, calendar_->minimum, calendar_->maximum);
    SetViewMonth(calendar_->selected.year, calendar_->selected.month);
}

core::DateTime DuiMonthCalendar::Selected() const { return calendar_->selected; }
void DuiMonthCalendar::SetMinDate(std::optional<core::DateTime> value) { calendar_->minimum = std::move(value); SetSelected(calendar_->selected); }
void DuiMonthCalendar::SetMaxDate(std::optional<core::DateTime> value) { calendar_->maximum = std::move(value); SetSelected(calendar_->selected); }
void DuiMonthCalendar::SetDateSelectedHandler(std::function<void(core::DateTime)> handler) { calendar_->selectedHandler = std::move(handler); }
core::Size DuiMonthCalendar::DesiredSize() const { return {CalendarWidth, CalendarHeight}; }
void DuiMonthCalendar::Layout(core::Rect bounds) { SetBounds(bounds); }

int DuiMonthCalendar::DayAtCell(int cell) const
{
    if (cell < 0 || cell >= 42) return 0;
    const int day = cell - Weekday(calendar_->viewYear, calendar_->viewMonth, 1) + 1;
    return day >= 1 && day <= core::DaysInMonth(calendar_->viewYear, calendar_->viewMonth) ? day : 0;
}

core::Point DuiMonthCalendar::CellFromPoint(core::Point point) const
{
    const auto bounds = Bounds();
    const int gridTop = bounds.top + HeaderHeight + WeekHeight;
    const int gridWidth = bounds.right - bounds.left - 8;
    const int gridHeight = bounds.bottom - gridTop - 4;
    if (point.y < gridTop || gridWidth <= 0 || gridHeight <= 0) return {-1, -1};
    const int width = gridWidth / 7;
    const int height = gridHeight / 6;
    const int column = (point.x - bounds.left - 4) / width;
    const int row = (point.y - gridTop) / height;
    return column >= 0 && column < 7 && row >= 0 && row < 6 ? core::Point{column, row} : core::Point{-1, -1};
}

bool DuiMonthCalendar::OnEvent(const core::Event& event)
{
    if (event.type != core::EventType::PointerDown || !Bounds().Contains(event.position)) return false;
    const auto bounds = Bounds();
    if (event.position.y < bounds.top + HeaderHeight) {
        if (event.position.x < bounds.left + 34) SetViewMonth(ViewYear(), ViewMonth() - 1);
        else if (event.position.x >= bounds.right - 34) SetViewMonth(ViewYear(), ViewMonth() + 1);
        return true;
    }
    const auto cell = CellFromPoint(event.position);
    if (cell.x < 0) return false;
    const int day = DayAtCell(cell.y * 7 + cell.x);
    if (day == 0) return true;
    core::DateTime value{ViewYear(), ViewMonth(), day};
    value = Clamp(value, calendar_->selected, calendar_->minimum, calendar_->maximum);
    calendar_->selected = value;
    if (calendar_->selectedHandler) calendar_->selectedHandler(value);
    return true;
}

void DuiMonthCalendar::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const auto bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty()) return;
    render::DuiTextStyle textStyle = calendar_->textStyle;
    textStyle.color = Theme().Get(core::ThemeSlot::ControlText);
    canvas.FillRect(bounds, Theme().Get(core::ThemeSlot::SurfaceBackground));
    canvas.DrawText(DuiDateTimePicker::FormatDate({ViewYear(), ViewMonth(), 1}, "%Y-%m"),
                    {Bounds().left + 32, Bounds().top, Bounds().right - 32, Bounds().top + HeaderHeight},
                    textStyle, render::DuiTextAlignment::Center, false);
    canvas.DrawText("<", {Bounds().left + 6, Bounds().top, Bounds().left + 28, Bounds().top + HeaderHeight}, textStyle, render::DuiTextAlignment::Center, false);
    canvas.DrawText(">", {Bounds().right - 28, Bounds().top, Bounds().right - 6, Bounds().top + HeaderHeight}, textStyle, render::DuiTextAlignment::Center, false);
    static constexpr std::array<std::string_view, 7> Weekdays{"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    const int cellWidth = (Bounds().right - Bounds().left - 8) / 7;
    for (int column = 0; column < 7; ++column)
        canvas.DrawText(Weekdays[static_cast<std::size_t>(column)], {Bounds().left + 4 + column * cellWidth, Bounds().top + HeaderHeight,
                        Bounds().left + 4 + (column + 1) * cellWidth, Bounds().top + HeaderHeight + WeekHeight}, textStyle, render::DuiTextAlignment::Center, false);
    const int gridTop = Bounds().top + HeaderHeight + WeekHeight;
    const int cellHeight = (Bounds().bottom - gridTop - 4) / 6;
    for (int cell = 0; cell < 42; ++cell) {
        const int day = DayAtCell(cell);
        if (day == 0) continue;
        const int column = cell % 7;
        const int row = cell / 7;
        const core::Rect cellBounds{Bounds().left + 4 + column * cellWidth, gridTop + row * cellHeight, Bounds().left + 4 + (column + 1) * cellWidth, gridTop + (row + 1) * cellHeight};
        const bool selected = calendar_->selected.year == ViewYear() && calendar_->selected.month == ViewMonth() && calendar_->selected.day == day;
        if (selected) canvas.FillRoundedRect(cellBounds, 3, Theme().Get(core::ThemeSlot::BrandPrimary));
        const std::string text = std::to_string(day);
        auto style = textStyle;
        if (selected) style.color = Theme().Get(core::ThemeSlot::TextOnPrimary);
        canvas.DrawText(text, cellBounds, style, render::DuiTextAlignment::Center, false);
    }
    // 独立使用（不在弹出层内）时依靠外框区分日历与页面底色
    canvas.StrokeRoundedRect(Bounds(), 0, Theme().Get(core::ThemeSlot::GridBorder), 1.0F);
}

class DuiDateTimePicker::Impl {
public:
    Mode mode{Mode::Date};
    core::DateTime value{2000, 1, 1};
    std::optional<core::DateTime> minimum;
    std::optional<core::DateTime> maximum;
    const ui::DuiDateTimeSource* dateTimeSource{};
    ui::IPopupHost* popup{};
    std::shared_ptr<int> popupLifetime;
    std::function<void(core::DateTime)> changed;
    std::string format;
    render::DuiTextStyle textStyle;
    bool popupOpen{};
};

class DateTimePopup final : public core::Control, public render::DuiRenderable {
public:
    DateTimePopup(DuiDateTimePicker::Mode mode, core::DateTime value, std::optional<core::DateTime> minimum,
                  std::optional<core::DateTime> maximum, std::function<void(core::DateTime)> accepted)
        : mode_(mode), value_(value), accepted_(std::move(accepted))
    {
        if (mode_ != DuiDateTimePicker::Mode::Time) {
            calendar_ = std::make_unique<DuiMonthCalendar>();
            calendar_->SetSelected(value_);
            calendar_->SetMinDate(std::move(minimum));
            calendar_->SetMaxDate(std::move(maximum));
            calendar_->SetDateSelectedHandler([this](core::DateTime date) {
                value_.year = date.year; value_.month = date.month; value_.day = date.day; accepted_(value_);
            });
        }
    }

    bool OnEvent(const core::Event& event) override
    {
        if (event.type == core::EventType::KeyDown && event.key == core::key::Enter) { accepted_(value_); return true; }
        if (event.type != core::EventType::PointerDown || !Bounds().Contains(event.position)) return false;
        if (calendar_ && event.position.y < Bounds().top + CalendarHeight) return calendar_->OnEvent(event);
        const int top = calendar_ ? Bounds().top + CalendarHeight : Bounds().top;
        const int localY = event.position.y - top;
        if (!calendar_ && localY >= TimePanelHeight) { accepted_(value_); return true; }
        const int column = std::clamp((event.position.x - Bounds().left) * 3 / std::max(1, Bounds().right - Bounds().left), 0, 2);
        const int delta = localY < TimePanelHeight / 2 ? 1 : -1;
        int* field = column == 0 ? &value_.hour : (column == 1 ? &value_.minute : &value_.second);
        const int limit = column == 0 ? 24 : 60;
        *field = (*field + delta + limit) % limit;
        return true;
    }

    void Layout(core::Rect bounds) { SetBounds(bounds); if (calendar_) calendar_->Layout({bounds.left, bounds.top, bounds.right, bounds.top + CalendarHeight}); }
    void SetPopupTheme(const core::DuiTheme* theme)
    {
        SetTheme(theme);
        if (calendar_)
            calendar_->SetTheme(theme);
    }
    void Paint(render::Canvas& canvas, core::Rect dirty) const override
    {
        canvas.FillRect(core::Rect::Intersect(Bounds(), dirty), Theme().Get(core::ThemeSlot::SurfaceBackground));
        if (calendar_) calendar_->Paint(canvas, dirty);
        const int top = calendar_ ? Bounds().top + CalendarHeight : Bounds().top;
        std::string time = DuiDateTimePicker::FormatDate(value_, "%H:%M:%S");
        render::DuiTextStyle style = style_;
        style.color = Theme().Get(core::ThemeSlot::ControlText);
        canvas.DrawText(time, {Bounds().left, top, Bounds().right, top + TimePanelHeight}, style, render::DuiTextAlignment::Center, false);
        if (!calendar_) canvas.DrawText("Apply", {Bounds().left, top + TimePanelHeight, Bounds().right, Bounds().bottom}, style, render::DuiTextAlignment::Center, false);
        render::PaintPopupBorder(canvas, Bounds(), Theme());
    }

private:
    DuiDateTimePicker::Mode mode_;
    core::DateTime value_;
    std::unique_ptr<DuiMonthCalendar> calendar_;
    std::function<void(core::DateTime)> accepted_;
    render::DuiTextStyle style_;
};

DuiDateTimePicker::DuiDateTimePicker() : picker_(std::make_unique<Impl>()) {}
DuiDateTimePicker::~DuiDateTimePicker()
{
    picker_->popupLifetime.reset();
    picker_->popup = nullptr;
}
void DuiDateTimePicker::SetMode(Mode mode) { picker_->mode = mode; }
DuiDateTimePicker::Mode DuiDateTimePicker::GetMode() const { return picker_->mode; }

void DuiDateTimePicker::SetDate(core::DateTime value, bool notify)
{
    const core::DateTime fallback = picker_->dateTimeSource ? picker_->dateTimeSource->LocalNow() : picker_->value;
    value = Clamp(value, fallback, picker_->minimum, picker_->maximum);
    if (picker_->mode == Mode::Date) value.hour = value.minute = value.second = 0;
    if (value == picker_->value) return;
    picker_->value = value;
    if (notify && picker_->changed) picker_->changed(value);
}

core::DateTime DuiDateTimePicker::Date() const { return picker_->value; }
void DuiDateTimePicker::SetFormat(std::string format) { picker_->format = std::move(format); }
const std::string& DuiDateTimePicker::Format() const { return picker_->format; }
void DuiDateTimePicker::SetMinDate(std::optional<core::DateTime> value) { picker_->minimum = std::move(value); SetDate(Date(), false); }
void DuiDateTimePicker::SetMaxDate(std::optional<core::DateTime> value) { picker_->maximum = std::move(value); SetDate(Date(), false); }
void DuiDateTimePicker::SetDateTimeSource(const ui::DuiDateTimeSource* source) { picker_->dateTimeSource = source; SetDate(Date(), false); }

void DuiDateTimePicker::SetPopupHost(ui::IPopupHost* popup)
{
    if (picker_->popup == popup) return;
    picker_->popupLifetime.reset();
    if (picker_->popup) { picker_->popup->SetDismissedHandler({}); if (picker_->popupOpen) picker_->popup->RequestHide(); }
    picker_->popup = popup;
    picker_->popupOpen = false;
    if (popup) {
        picker_->popupLifetime = std::make_shared<int>();
        const std::weak_ptr<int> lifetime = picker_->popupLifetime;
        popup->SetDismissedHandler([lifetime, state = picker_.get()] {
            if (lifetime.lock()) state->popupOpen = false;
        });
    }
}

void DuiDateTimePicker::SetValueChangedHandler(std::function<void(core::DateTime)> handler) { picker_->changed = std::move(handler); }
bool DuiDateTimePicker::PopupOpen() const { return picker_->popupOpen; }

void DuiDateTimePicker::OpenPopup()
{
    if (!picker_->popup || picker_->popupOpen) return;
    const int width = picker_->mode == Mode::Time ? 200 : std::max(CalendarWidth, Bounds().right - Bounds().left);
    const int height = picker_->mode == Mode::Time ? TimePanelHeight + TimeApplyHeight : CalendarHeight + (picker_->mode == Mode::DateTime ? TimePanelHeight : 0);
    Impl* state = picker_.get();
    const std::weak_ptr<int> lifetime = picker_->popupLifetime;
    auto content = std::make_unique<DateTimePopup>(state->mode, state->value, state->minimum, state->maximum, [lifetime, state](core::DateTime value) {
        if (!lifetime.lock()) return;
        const core::DateTime fallback = state->dateTimeSource ? state->dateTimeSource->LocalNow() : state->value;
        value = Clamp(value, fallback, state->minimum, state->maximum);
        if (state->mode == Mode::Date) value.hour = value.minute = value.second = 0;
        const bool changed = value != state->value;
        state->value = value;
        if (changed && state->changed) state->changed(value);
        if (state->popup) state->popup->RequestHide();
        state->popupOpen = false;
    });
    DateTimePopup* raw = content.get();
    raw->SetPopupTheme(&Theme());
    ui::DuiPopupOptions options;
    options.anchor = Bounds();
    options.size = {width, height};
    picker_->popupOpen = picker_->popup->Show(options, std::move(content), [raw](core::Rect bounds) { raw->Layout(bounds); },
        [raw](render::Canvas& canvas, core::Rect dirty) { raw->Paint(canvas, dirty); });
}

void DuiDateTimePicker::ClosePopup() { if (picker_->popup && picker_->popupOpen) picker_->popup->RequestHide(); picker_->popupOpen = false; }

bool DuiDateTimePicker::TryParseYmd(std::string_view text, core::DateTime& value)
{
    std::size_t position{}; int year{}; int month{}; int day{};
    if (!ParseNumber(text, position, 4, 4, year) || !Consume(text, position, '-') || !ParseNumber(text, position, 1, 2, month) || !Consume(text, position, '-') || !ParseNumber(text, position, 1, 2, day) || position != text.size() || !core::IsValidDate(year, month, day)) return false;
    value = {year, month, day}; return true;
}

bool DuiDateTimePicker::TryParseHms(std::string_view text, core::DateTime& value)
{
    std::size_t position{}; int hour{}; int minute{}; int second{};
    if (!ParseNumber(text, position, 1, 2, hour) || !Consume(text, position, ':') || !ParseNumber(text, position, 1, 2, minute)) return false;
    if (position < text.size()) { if (!Consume(text, position, ':') || !ParseNumber(text, position, 1, 2, second)) return false; }
    if (position != text.size() || !core::IsValidTime(hour, minute, second)) return false;
    value.hour = hour; value.minute = minute; value.second = second; return true;
}

bool DuiDateTimePicker::TryParseYmdHms(std::string_view text, core::DateTime& value)
{
    const std::size_t split = text.find_first_of(" T");
    if (split == std::string_view::npos) return false;
    core::DateTime date; core::DateTime time;
    if (!TryParseYmd(text.substr(0, split), date) || !TryParseHms(text.substr(split + 1), time)) return false;
    value = {date.year, date.month, date.day, time.hour, time.minute, time.second}; return true;
}

std::string DuiDateTimePicker::FormatDate(core::DateTime value, std::string_view format)
{
    std::string output;
    for (std::size_t index{}; index < format.size(); ++index) {
        if (format[index] != '%' || index + 1 == format.size()) { output.push_back(format[index]); continue; }
        const char code = format[++index];
        switch (code) {
        case 'Y': AppendNumber(output, value.year, 4); break;
        case 'm': AppendNumber(output, value.month, 2); break;
        case 'd': AppendNumber(output, value.day, 2); break;
        case 'H': AppendNumber(output, value.hour, 2); break;
        case 'M': AppendNumber(output, value.minute, 2); break;
        case 'S': AppendNumber(output, value.second, 2); break;
        default: output.push_back('%'); output.push_back(code); break;
        }
    }
    return output;
}

core::Rect DuiDateTimePicker::DropButtonRect() const { const auto bounds = Bounds(); return {bounds.right - DropWidth, bounds.top, bounds.right, bounds.bottom}; }
core::Size DuiDateTimePicker::DesiredSize() const
{
    switch (picker_->mode)
    {
    case Mode::Date:
        return {140, PickerHeight};
    case Mode::Time:
        return {120, PickerHeight};
    case Mode::DateTime:
        return {200, PickerHeight};
    }
    return {140, PickerHeight};
}
void DuiDateTimePicker::Layout(core::Rect bounds) { SetBounds(bounds); }

bool DuiDateTimePicker::OnEvent(const core::Event& event)
{
    if (!Enabled()) return false;
    const bool clicked = event.type == core::EventType::PointerDown && Bounds().Contains(event.position);
    const bool activated = event.type == core::EventType::KeyDown
        && (event.key == core::key::Enter || event.key == core::key::Space
            || event.key == core::key::Function1 + 3);
    if (!clicked && !activated) { if (event.type == core::EventType::KeyDown && event.key == core::key::Escape && PopupOpen()) { ClosePopup(); return true; } return false; }
    if (PopupOpen()) ClosePopup(); else OpenPopup();
    return true;
}

void DuiDateTimePicker::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const auto bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty()) return;
    canvas.FillRect(bounds, Theme().Get(core::ThemeSlot::SurfaceBackground));
    const core::Color border = Theme().Get(core::ThemeSlot::FieldBorder);
    canvas.StrokeRoundedRect(bounds, 0, border, 1.0F);
    const core::Rect drop = DropButtonRect();
    canvas.FillRect({drop.left, bounds.top + 1, drop.left + 1, bounds.bottom - 1},
                    border);
    const std::string_view format = picker_->format.empty() ? DefaultFormat(picker_->mode)
                                                              : std::string_view{picker_->format};
    render::DuiTextStyle textStyle = picker_->textStyle;
    textStyle.color = Theme().Get(core::ThemeSlot::ControlText);
    canvas.DrawText(FormatDate(Date(), format), {bounds.left + 8, bounds.top, drop.left - 4, bounds.bottom},
                    textStyle, render::DuiTextAlignment::Start, false);
    const int centerX = (drop.left + drop.right) / 2;
    const int centerY = (drop.top + drop.bottom) / 2;
    render::DuiPath arrow;
    arrow.MoveTo({centerX - 4, centerY - 2});
    arrow.LineTo({centerX + 4, centerY - 2});
    arrow.LineTo({centerX, centerY + 3});
    arrow.Close();
    canvas.FillPath(arrow, Theme().Get(core::ThemeSlot::DateTimeArrow));
    if (Focused() && Enabled())
        render::DrawThemedFocusRing(canvas, bounds, Theme());
}

} // namespace ysDui::controls::input
