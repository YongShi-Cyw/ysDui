#include "ysDui/controls/list/DuiPropertyGrid.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <utility>

#include "../input/DuiTextInputPaint.hpp"
#include "ysDui/controls/input/DuiScrollBar.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::list {
namespace {
constexpr int ScrollBarWidth = 17;
constexpr int DefaultNameColumnWidth = 140;
constexpr int DefaultRowHeight = 26;
constexpr int MinimumNameColumnWidth = 40;
constexpr int MinimumRowHeight = 18;
constexpr int TextPadding = 6;

bool TryParseInt(std::string_view text, int& value)
{
    if (text.empty()) return false;
    int sign = 1;
    std::size_t index{};
    if (text.front() == '-') {
        sign = -1;
        index = 1;
    }
    if (index == text.size()) return false;
    std::int64_t result{};
    const std::int64_t limit = sign > 0 ? std::numeric_limits<int>::max() :
                                         -static_cast<std::int64_t>(std::numeric_limits<int>::min());
    for (; index < text.size(); ++index) {
        const char character = text[index];
        if (character < '0' || character > '9') return false;
        if (result > (limit - (character - '0')) / 10) return false;
        result = result * 10 + character - '0';
    }
    value = sign > 0 ? static_cast<int>(result) : result == limit ? std::numeric_limits<int>::min() : -static_cast<int>(result);
    return true;
}

std::string FormatInt(int value)
{
    char buffer[16]{};
    const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
    std::string text;
    for (const char* character = buffer; character != result.ptr; ++character)
        text.push_back(static_cast<char>(*character));
    return text;
}
} // namespace

class DuiPropertyGrid::Impl {
public:
    struct Row final {
        RowType type{RowType::String};
        std::string name;
        std::string stringValue;
        bool boolValue{};
        int intValue{};
        int enumValue{};
        std::vector<EnumOption> options;
    };

    input::DuiScrollBar* scrollBar{};
    ui::DuiTextInput* textInput{};
    std::shared_ptr<int> textInputLifetime;
    std::vector<Row> rows;
    std::function<void(int)> changed;
    std::function<void(int)> edited;
    render::DuiTextStyle textStyle;
    int nameColumnWidth{DefaultNameColumnWidth};
    int rowHeight{DefaultRowHeight};
    int selectedRow{-1};
    int editRow{-1};
    bool textStyleOverride{};
};

DuiPropertyGrid::DuiPropertyGrid() : propertyGrid_(std::make_unique<Impl>())
{
    auto scrollBar = std::make_unique<input::DuiScrollBar>();
    propertyGrid_->scrollBar = scrollBar.get();
    scrollBar->SetLineSize(DefaultRowHeight);
    AddChild(std::move(scrollBar));
}

DuiPropertyGrid::~DuiPropertyGrid()
{
    propertyGrid_->textInputLifetime.reset();
    propertyGrid_->textInput = nullptr;
}

int DuiPropertyGrid::AddGroup(std::string title)
{
    propertyGrid_->rows.push_back({RowType::Group, std::move(title), {}, false, 0, 0, {}});
    UpdateScrollRange();
    return RowCount() - 1;
}

int DuiPropertyGrid::AddString(std::string name, std::string value)
{
    propertyGrid_->rows.push_back({RowType::String, std::move(name), std::move(value), false, 0, 0, {}});
    UpdateScrollRange();
    return RowCount() - 1;
}

int DuiPropertyGrid::AddBool(std::string name, bool value)
{
    Impl::Row row{RowType::Bool, std::move(name), {}, false, 0, 0, {}};
    row.boolValue = value;
    propertyGrid_->rows.push_back(std::move(row));
    UpdateScrollRange();
    return RowCount() - 1;
}

int DuiPropertyGrid::AddInt(std::string name, int value)
{
    Impl::Row row{RowType::Int, std::move(name), {}, false, 0, 0, {}};
    row.intValue = value;
    propertyGrid_->rows.push_back(std::move(row));
    UpdateScrollRange();
    return RowCount() - 1;
}

int DuiPropertyGrid::AddEnum(std::string name, std::vector<EnumOption> options, int value)
{
    Impl::Row row{RowType::Enum, std::move(name), {}, false, 0, 0, {}};
    row.options = std::move(options);
    row.enumValue = value;
    propertyGrid_->rows.push_back(std::move(row));
    UpdateScrollRange();
    return RowCount() - 1;
}

void DuiPropertyGrid::Clear()
{
    CancelEdit();
    propertyGrid_->rows.clear();
    propertyGrid_->selectedRow = -1;
    UpdateScrollRange();
}

int DuiPropertyGrid::RowCount() const { return static_cast<int>(propertyGrid_->rows.size()); }
DuiPropertyGrid::RowType DuiPropertyGrid::TypeAt(int row) const { return row >= 0 && row < RowCount() ? propertyGrid_->rows[row].type : RowType::String; }
std::string DuiPropertyGrid::NameAt(int row) const { return row >= 0 && row < RowCount() ? propertyGrid_->rows[row].name : std::string{}; }
std::string DuiPropertyGrid::StringValueAt(int row) const { return row >= 0 && row < RowCount() && TypeAt(row) == RowType::String ? propertyGrid_->rows[row].stringValue : std::string{}; }

void DuiPropertyGrid::SetStringValue(int row, std::string value, bool notify)
{
    if (row < 0 || row >= RowCount() || TypeAt(row) != RowType::String || propertyGrid_->rows[row].stringValue == value) return;
    propertyGrid_->rows[row].stringValue = std::move(value);
    if (notify && propertyGrid_->changed) propertyGrid_->changed(row);
}

bool DuiPropertyGrid::BoolValueAt(int row) const { return row >= 0 && row < RowCount() && TypeAt(row) == RowType::Bool && propertyGrid_->rows[row].boolValue; }
void DuiPropertyGrid::SetBoolValue(int row, bool value, bool notify)
{
    if (row < 0 || row >= RowCount() || TypeAt(row) != RowType::Bool || propertyGrid_->rows[row].boolValue == value) return;
    propertyGrid_->rows[row].boolValue = value;
    if (notify && propertyGrid_->changed) propertyGrid_->changed(row);
}

int DuiPropertyGrid::IntValueAt(int row) const { return row >= 0 && row < RowCount() && TypeAt(row) == RowType::Int ? propertyGrid_->rows[row].intValue : 0; }
void DuiPropertyGrid::SetIntValue(int row, int value, bool notify)
{
    if (row < 0 || row >= RowCount() || TypeAt(row) != RowType::Int || propertyGrid_->rows[row].intValue == value) return;
    propertyGrid_->rows[row].intValue = value;
    if (notify && propertyGrid_->changed) propertyGrid_->changed(row);
}

int DuiPropertyGrid::EnumValueAt(int row) const { return row >= 0 && row < RowCount() && TypeAt(row) == RowType::Enum ? propertyGrid_->rows[row].enumValue : 0; }
void DuiPropertyGrid::SetEnumValue(int row, int value, bool notify)
{
    if (row < 0 || row >= RowCount() || TypeAt(row) != RowType::Enum || propertyGrid_->rows[row].enumValue == value) return;
    propertyGrid_->rows[row].enumValue = value;
    if (notify && propertyGrid_->changed) propertyGrid_->changed(row);
}

void DuiPropertyGrid::SetNameColumnWidth(int pixels) { propertyGrid_->nameColumnWidth = (std::max)(MinimumNameColumnWidth, pixels); PlaceTextInput(); }
int DuiPropertyGrid::NameColumnWidth() const { return propertyGrid_->nameColumnWidth; }
void DuiPropertyGrid::SetRowHeight(int pixels) { propertyGrid_->rowHeight = (std::max)(MinimumRowHeight, pixels); propertyGrid_->scrollBar->SetLineSize(propertyGrid_->rowHeight); UpdateScrollRange(); PlaceTextInput(); }
int DuiPropertyGrid::RowHeight() const { return propertyGrid_->rowHeight; }
core::Size DuiPropertyGrid::DesiredSize() const { return {NameColumnWidth() + 180 + ScrollBarWidth, RowHeight() * 10}; }

void DuiPropertyGrid::SetTextInput(ui::DuiTextInput* input)
{
    if (propertyGrid_->textInput == input) return;
    CancelEdit();
    propertyGrid_->textInputLifetime.reset();
    if (propertyGrid_->textInput) {
        propertyGrid_->textInput->SetFocusLostHandler({});
        propertyGrid_->textInput->SetVisible(false);
    }
    propertyGrid_->textInput = input;
    if (input) {
        propertyGrid_->textInputLifetime = std::make_shared<int>();
        const std::weak_ptr<int> lifetime = propertyGrid_->textInputLifetime;
        input->SetFocusLostHandler([lifetime, this] {
            if (lifetime.lock()) CommitEdit();
        });
    }
}

bool DuiPropertyGrid::BeginEdit(int row)
{
    if (!propertyGrid_->textInput || row < 0 || row >= RowCount() || (TypeAt(row) != RowType::String && TypeAt(row) != RowType::Int)) return false;
    CommitEdit();
    propertyGrid_->editRow = row;
    propertyGrid_->textInput->SetText(DisplayValue(row));
    propertyGrid_->textInput->SetBorderVisible(false);
    propertyGrid_->textInput->SetVisible(true);
    propertyGrid_->textInput->SetEnabled(true);
    PlaceTextInput();
    propertyGrid_->textInput->Focus();
    return true;
}

void DuiPropertyGrid::CommitEdit()
{
    if (!Editing()) return;
    const int row = propertyGrid_->editRow;
    const std::string text = propertyGrid_->textInput->Text();
    propertyGrid_->editRow = -1;
    propertyGrid_->textInput->SetVisible(false);
    if (TypeAt(row) == RowType::String) SetStringValue(row, text, true);
    else {
        int value{};
        if (TryParseInt(text, value)) SetIntValue(row, value, true);
    }
    if (propertyGrid_->edited) propertyGrid_->edited(row);
}

void DuiPropertyGrid::CancelEdit()
{
    if (!Editing()) return;
    propertyGrid_->editRow = -1;
    propertyGrid_->textInput->SetVisible(false);
}

bool DuiPropertyGrid::Editing() const { return propertyGrid_->textInput && propertyGrid_->editRow >= 0; }
void DuiPropertyGrid::SetSelectedRow(int row) { propertyGrid_->selectedRow = row >= -1 && row < RowCount() ? row : -1; }
int DuiPropertyGrid::SelectedRow() const { return propertyGrid_->selectedRow; }
void DuiPropertyGrid::SetValueChangedHandler(std::function<void(int)> handler) { propertyGrid_->changed = std::move(handler); }
void DuiPropertyGrid::SetEditedHandler(std::function<void(int)> handler) { propertyGrid_->edited = std::move(handler); }
void DuiPropertyGrid::SetTextStyle(render::DuiTextStyle style) { propertyGrid_->textStyle = std::move(style); propertyGrid_->textStyleOverride = true; }

void DuiPropertyGrid::UpdateScrollRange()
{
    const int height = (std::max)(0, Bounds().Height());
    const int contentHeight = RowCount() * RowHeight();
    propertyGrid_->scrollBar->SetRange(0, (std::max)(0, contentHeight - height));
    propertyGrid_->scrollBar->SetPageSize((std::max)(1, height));
    propertyGrid_->scrollBar->SetVisible(contentHeight > height && height > 0);
}

void DuiPropertyGrid::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    UpdateScrollRange();
    propertyGrid_->scrollBar->SetBounds({bounds.right - ScrollBarWidth, bounds.top, bounds.right, bounds.bottom});
    PlaceTextInput();
}

int DuiPropertyGrid::RowFromPoint(core::Point point) const
{
    const int bodyRight = Bounds().right - (propertyGrid_->scrollBar->Visible() ? ScrollBarWidth : 0);
    if (point.x < Bounds().left || point.x >= bodyRight || point.y < Bounds().top || point.y >= Bounds().bottom) return -1;
    const int row = (point.y - Bounds().top + propertyGrid_->scrollBar->Position()) / RowHeight();
    return row >= 0 && row < RowCount() ? row : -1;
}

core::Rect DuiPropertyGrid::RowRect(int row) const
{
    if (row < 0 || row >= RowCount()) return {};
    const int top = Bounds().top + row * RowHeight() - propertyGrid_->scrollBar->Position();
    const int right = Bounds().right - (propertyGrid_->scrollBar->Visible() ? ScrollBarWidth : 0);
    return {Bounds().left, top, right, top + RowHeight()};
}

core::Rect DuiPropertyGrid::ValueRect(int row) const
{
    const auto rowBounds = RowRect(row);
    return {Bounds().left + NameColumnWidth(), rowBounds.top, rowBounds.right, rowBounds.bottom};
}

std::string DuiPropertyGrid::DisplayValue(int row) const
{
    if (row < 0 || row >= RowCount()) return {};
    const auto& value = propertyGrid_->rows[row];
    if (value.type == RowType::String) return value.stringValue;
    if (value.type == RowType::Bool) return value.boolValue ? "True" : "False";
    if (value.type == RowType::Int) return FormatInt(value.intValue);
    if (value.type == RowType::Enum) {
        const auto found = std::find_if(value.options.begin(), value.options.end(), [&value](const EnumOption& option) { return option.value == value.enumValue; });
        return found == value.options.end() ? std::string{} : found->text;
    }
    return {};
}

void DuiPropertyGrid::PlaceTextInput()
{
    if (!Editing()) return;
    auto bounds = ValueRect(propertyGrid_->editRow);
    bounds.left += 1;
    bounds.top += 1;
    bounds.right -= 1;
    bounds.bottom -= 1;
    propertyGrid_->textInput->SetBounds(bounds);
}

bool DuiPropertyGrid::OnEvent(const core::Event& event)
{
    if (!Enabled()) return false;
    if (event.type == core::EventType::PointerWheel && Bounds().Contains(event.position)) {
        propertyGrid_->scrollBar->SetPosition(propertyGrid_->scrollBar->Position() + (event.wheelDelta > 0 ? -3 : 3) * RowHeight());
        return event.wheelDelta != 0;
    }
    if (event.type == core::EventType::PointerDown) {
        if (propertyGrid_->scrollBar->Visible() && propertyGrid_->scrollBar->Bounds().Contains(event.position)) return propertyGrid_->scrollBar->OnEvent(event);
        CommitEdit();
        const int row = RowFromPoint(event.position);
        if (row < 0) return false;
        SetSelectedRow(row);
        if (TypeAt(row) == RowType::Bool) SetBoolValue(row, !BoolValueAt(row));
        if (TypeAt(row) == RowType::Enum && !propertyGrid_->rows[row].options.empty()) {
            const auto& options = propertyGrid_->rows[row].options;
            const auto current = std::find_if(options.begin(), options.end(), [this, row](const EnumOption& option) { return option.value == propertyGrid_->rows[row].enumValue; });
            const int next = current == options.end() ? 0 : (static_cast<int>(current - options.begin()) + 1) % static_cast<int>(options.size());
            SetEnumValue(row, options[next].value);
        }
        return true;
    }
    if (event.type == core::EventType::PointerDoubleClick) {
        const int row = RowFromPoint(event.position);
        if (row < 0) return false;
        SetSelectedRow(row);
        return BeginEdit(row);
    }
    if (event.type == core::EventType::KeyDown) {
        if (event.key == core::key::Enter && Editing()) { CommitEdit(); return true; }
        if (event.key == core::key::Escape && Editing()) { CancelEdit(); return true; }
        if (event.key == core::key::Function1) return BeginEdit(SelectedRow());
        if (event.key == core::key::Space && TypeAt(SelectedRow()) == RowType::Bool) { SetBoolValue(SelectedRow(), !BoolValueAt(SelectedRow())); return true; }
        if ((event.key == core::key::Up || event.key == core::key::Down) && RowCount() > 0) { SetSelectedRow(std::clamp(SelectedRow() + (event.key == core::key::Down ? 1 : -1), 0, RowCount() - 1)); return true; }
    }
    return false;
}

void DuiPropertyGrid::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const auto bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty() || !EffectivelyVisible()) return;
    const int bodyRight = Bounds().right - (propertyGrid_->scrollBar->Visible() ? ScrollBarWidth : 0);
    const core::DuiTheme& theme = Theme();
    render::DuiTextStyle textStyle = propertyGrid_->textStyle;
    if (!propertyGrid_->textStyleOverride) textStyle.color = theme.Get(core::ThemeSlot::ListText);
    canvas.FillRect(bounds, theme.Get(core::ThemeSlot::GridBackground));
    canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::GridBorder), 1.0F);
    canvas.PushClip({Bounds().left, Bounds().top, bodyRight, Bounds().bottom});
    const int first = propertyGrid_->scrollBar->Position() / RowHeight();
    const int last = (std::min)(RowCount(), first + Bounds().Height() / RowHeight() + 2);
    for (int row = first; row < last; ++row) {
        const auto rowBounds = RowRect(row);
        if (TypeAt(row) == RowType::Group) {
            canvas.FillRect(rowBounds, theme.Get(core::ThemeSlot::PropertyGroupBackground));
            canvas.DrawText(NameAt(row), {rowBounds.left + 8, rowBounds.top, rowBounds.right - TextPadding, rowBounds.bottom}, textStyle, render::DuiTextAlignment::Start, false);
        } else {
            if (row == SelectedRow()) canvas.FillRect(rowBounds, theme.Get(core::ThemeSlot::GridSelection));
            canvas.DrawText(NameAt(row), {rowBounds.left + TextPadding, rowBounds.top, Bounds().left + NameColumnWidth() - TextPadding, rowBounds.bottom}, textStyle, render::DuiTextAlignment::Start, false);
            const core::Rect valueBounds{ValueRect(row).left + TextPadding, rowBounds.top,
                                         rowBounds.right - TextPadding, rowBounds.bottom};
            if (Editing() && row == propertyGrid_->editRow)
                ysDui::controls::detail::PaintTextInput(
                    canvas, propertyGrid_->textInput, {}, {}, valueBounds, textStyle,
                    textStyle.color, render::DuiTextAlignment::Start, false, Focused(),
                    theme.Get(core::ThemeSlot::SelectionBackground),
                    theme.Get(core::ThemeSlot::TextOnSelectedRow));
            else
                canvas.DrawText(DisplayValue(row), valueBounds, textStyle,
                                render::DuiTextAlignment::Start, false);
        }
        canvas.FillRect({rowBounds.left, rowBounds.bottom - 1, rowBounds.right, rowBounds.bottom}, theme.Get(core::ThemeSlot::GridLine));
        canvas.FillRect({Bounds().left + NameColumnWidth() - 1, rowBounds.top, Bounds().left + NameColumnWidth(), rowBounds.bottom}, theme.Get(core::ThemeSlot::GridLine));
    }
    canvas.PopClip();
}

} // namespace ysDui::controls::list
