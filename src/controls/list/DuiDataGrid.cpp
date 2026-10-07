#include "ysDui/controls/list/DuiDataGrid.hpp"

#include <algorithm>
#include <utility>

#include "../input/DuiTextInputPaint.hpp"
#include "ysDui/controls/input/DuiScrollBar.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::list {
namespace {
constexpr int DefaultRowHeight = 28;
constexpr int DefaultHeaderHeight = 26;
constexpr int MinimumRowHeight = 16;
constexpr int ScrollBarWidth = 17;
constexpr int CheckboxColumnWidth = 22;
constexpr int TextPadding = 4;
}

class DuiDataGrid::Impl {
public:
    struct Row final { std::vector<std::string> cells; bool checked{}; };
    input::DuiScrollBar* scrollBar{};
    ui::DuiTextInput* textInput{};
    std::shared_ptr<int> textInputLifetime;
    std::vector<DuiDataGridColumn> columns;
    std::vector<Row> rows;
    std::vector<int> selection;
    render::DuiTextStyle textStyle;
    std::function<void(int)> selectionChanged;
    std::function<void(int, int)> columnClicked;
    std::function<void(int, bool)> checkChanged;
    std::function<void(int, int, std::string_view)> cellEdited;
    int selectedRow{-1};
    int selectionAnchor{-1};
    int editRow{-1};
    int editColumn{-1};
    int rowHeight{DefaultRowHeight};
    int headerHeight{DefaultHeaderHeight};
    int sortColumn{-1};
    int sortDirection{};
    bool checkboxesVisible{};
    bool multiSelect{};
    bool editable{};
    bool textStyleOverride{};
};

DuiDataGrid::DuiDataGrid() : dataGrid_(std::make_unique<Impl>())
{
    auto scrollBar = std::make_unique<input::DuiScrollBar>();
    dataGrid_->scrollBar = scrollBar.get();
    scrollBar->SetLineSize(DefaultRowHeight);
    scrollBar->SetValueChangedHandler([](int) {});
    AddChild(std::move(scrollBar));
}

DuiDataGrid::~DuiDataGrid()
{
    dataGrid_->textInputLifetime.reset();
    dataGrid_->textInput = nullptr;
}

int DuiDataGrid::AddColumn(DuiDataGridColumn column)
{
    column.minimumWidth = (std::max)(1, column.minimumWidth);
    column.width = (std::max)(column.minimumWidth, column.width);
    dataGrid_->columns.push_back(std::move(column));
    for (auto& row : dataGrid_->rows)
        row.cells.resize(ColumnCount());
    return ColumnCount() - 1;
}

int DuiDataGrid::ColumnCount() const { return static_cast<int>(dataGrid_->columns.size()); }
DuiDataGridColumn DuiDataGrid::ColumnAt(int column) const { return column >= 0 && column < ColumnCount() ? dataGrid_->columns[column] : DuiDataGridColumn{}; }
void DuiDataGrid::SetColumnWidth(int column, int width) { if (column >= 0 && column < ColumnCount()) dataGrid_->columns[column].width = (std::max)(dataGrid_->columns[column].minimumWidth, width); }
void DuiDataGrid::SetColumnEditable(int column, bool editable) { if (column >= 0 && column < ColumnCount()) dataGrid_->columns[column].editable = editable; }

int DuiDataGrid::AddRow() { return InsertRow(RowCount()); }
int DuiDataGrid::InsertRow(int index)
{
    index = std::clamp(index, 0, RowCount());
    dataGrid_->rows.insert(dataGrid_->rows.begin() + index, {std::vector<std::string>(ColumnCount()), false});
    for (int& selected : dataGrid_->selection) if (selected >= index) ++selected;
    if (dataGrid_->selectedRow >= index) ++dataGrid_->selectedRow;
    if (dataGrid_->selectionAnchor >= index) ++dataGrid_->selectionAnchor;
    UpdateScrollRange();
    return index;
}
void DuiDataGrid::RemoveRow(int index)
{
    if (index < 0 || index >= RowCount()) return;
    if (dataGrid_->editRow == index) CancelEdit();
    dataGrid_->rows.erase(dataGrid_->rows.begin() + index);
    auto& selection = dataGrid_->selection;
    selection.erase(std::remove(selection.begin(), selection.end(), index), selection.end());
    for (int& selected : selection) if (selected > index) --selected;
    if (dataGrid_->selectedRow == index) dataGrid_->selectedRow = selection.empty() ? -1 : selection.front(); else if (dataGrid_->selectedRow > index) --dataGrid_->selectedRow;
    if (dataGrid_->selectionAnchor == index) dataGrid_->selectionAnchor = -1; else if (dataGrid_->selectionAnchor > index) --dataGrid_->selectionAnchor;
    UpdateScrollRange();
}
void DuiDataGrid::ClearRows() { CancelEdit(); dataGrid_->rows.clear(); dataGrid_->selection.clear(); dataGrid_->selectedRow = -1; dataGrid_->selectionAnchor = -1; UpdateScrollRange(); }
int DuiDataGrid::RowCount() const { return static_cast<int>(dataGrid_->rows.size()); }
void DuiDataGrid::SetCellText(int row, int column, std::string text) { if (row >= 0 && row < RowCount() && column >= 0 && column < ColumnCount()) dataGrid_->rows[row].cells[column] = std::move(text); }
std::string DuiDataGrid::CellText(int row, int column) const { return row >= 0 && row < RowCount() && column >= 0 && column < ColumnCount() ? dataGrid_->rows[row].cells[column] : std::string{}; }

void DuiDataGrid::SetCheckboxesVisible(bool visible) { dataGrid_->checkboxesVisible = visible; }
bool DuiDataGrid::CheckboxesVisible() const { return dataGrid_->checkboxesVisible; }
void DuiDataGrid::SetRowChecked(int row, bool checked, bool notify) { if (row >= 0 && row < RowCount() && dataGrid_->rows[row].checked != checked) { dataGrid_->rows[row].checked = checked; if (notify && dataGrid_->checkChanged) dataGrid_->checkChanged(row, checked); } }
bool DuiDataGrid::RowChecked(int row) const { return row >= 0 && row < RowCount() && dataGrid_->rows[row].checked; }

void DuiDataGrid::SetSelectedRow(int row, bool notify)
{
    if (row < -1 || row >= RowCount()) return;
    const bool changed = dataGrid_->selectedRow != row || dataGrid_->selection.size() != (row < 0 ? 0U : 1U);
    dataGrid_->selectedRow = row; dataGrid_->selectionAnchor = row; dataGrid_->selection.clear();
    if (row >= 0) dataGrid_->selection.push_back(row);
    if (changed && notify && dataGrid_->selectionChanged) dataGrid_->selectionChanged(row);
}
int DuiDataGrid::SelectedRow() const { return dataGrid_->selectedRow; }
void DuiDataGrid::SetMultiSelect(bool enabled) { dataGrid_->multiSelect = enabled; if (!enabled) SetSelectedRow(dataGrid_->selectedRow, false); }
bool DuiDataGrid::MultiSelect() const { return dataGrid_->multiSelect; }
bool DuiDataGrid::IsRowSelected(int row) const { return std::find(dataGrid_->selection.begin(), dataGrid_->selection.end(), row) != dataGrid_->selection.end(); }
std::vector<int> DuiDataGrid::SelectedRows() const { auto result = dataGrid_->selection; std::sort(result.begin(), result.end()); return result; }
void DuiDataGrid::ClearSelection(bool notify) { const bool changed = dataGrid_->selectedRow >= 0 || !dataGrid_->selection.empty(); dataGrid_->selectedRow = -1; dataGrid_->selectionAnchor = -1; dataGrid_->selection.clear(); if (changed && notify && dataGrid_->selectionChanged) dataGrid_->selectionChanged(-1); }

void DuiDataGrid::SetEditable(bool editable) { dataGrid_->editable = editable; if (!editable) CancelEdit(); }
bool DuiDataGrid::Editable() const { return dataGrid_->editable; }
void DuiDataGrid::SetTextInput(ui::DuiTextInput* textInput)
{
    if (dataGrid_->textInput == textInput) return;
    CancelEdit();
    dataGrid_->textInputLifetime.reset();
    if (dataGrid_->textInput) { dataGrid_->textInput->SetChangedHandler({}); dataGrid_->textInput->SetFocusLostHandler({}); dataGrid_->textInput->SetVisible(false); }
    dataGrid_->textInput = textInput;
    if (textInput) {
        dataGrid_->textInputLifetime = std::make_shared<int>();
        const std::weak_ptr<int> lifetime = dataGrid_->textInputLifetime;
        textInput->SetFocusLostHandler([lifetime, this] {
            if (lifetime.lock()) CommitEdit();
        });
    }
}
bool DuiDataGrid::BeginEdit(int row, int column)
{
    if (!dataGrid_->textInput || !Editable() || row < 0 || row >= RowCount() || column < 0 || column >= ColumnCount() || !dataGrid_->columns[column].editable) return false;
    if (Editing()) CommitEdit();
    dataGrid_->editRow = row; dataGrid_->editColumn = column;
    dataGrid_->textInput->SetText(CellText(row, column));
    dataGrid_->textInput->SetBorderVisible(false); dataGrid_->textInput->SetEnabled(true); dataGrid_->textInput->SetVisible(true);
    PlaceTextInput(); dataGrid_->textInput->Focus();
    return true;
}
void DuiDataGrid::CommitEdit()
{
    if (!Editing()) return;
    const int row = dataGrid_->editRow; const int column = dataGrid_->editColumn;
    const std::string text = dataGrid_->textInput->Text();
    dataGrid_->editRow = -1; dataGrid_->editColumn = -1; dataGrid_->textInput->SetVisible(false);
    SetCellText(row, column, text);
    if (dataGrid_->cellEdited) dataGrid_->cellEdited(row, column, text);
}
void DuiDataGrid::CancelEdit() { if (!Editing()) return; dataGrid_->editRow = -1; dataGrid_->editColumn = -1; if (dataGrid_->textInput) dataGrid_->textInput->SetVisible(false); }
bool DuiDataGrid::Editing() const { return dataGrid_->textInput && dataGrid_->editRow >= 0 && dataGrid_->editColumn >= 0; }

void DuiDataGrid::SetRowHeight(int pixels) { dataGrid_->rowHeight = (std::max)(MinimumRowHeight, pixels); dataGrid_->scrollBar->SetLineSize(dataGrid_->rowHeight); UpdateScrollRange(); }
int DuiDataGrid::RowHeight() const { return dataGrid_->rowHeight; }
void DuiDataGrid::SetHeaderHeight(int pixels) { dataGrid_->headerHeight = (std::max)(MinimumRowHeight, pixels); UpdateScrollRange(); PlaceTextInput(); }
int DuiDataGrid::HeaderHeight() const { return dataGrid_->headerHeight; }
void DuiDataGrid::SetSortIndicator(int column, int direction) { dataGrid_->sortColumn = column >= 0 && column < ColumnCount() ? column : -1; dataGrid_->sortDirection = direction > 0 ? 1 : direction < 0 ? -1 : 0; }
int DuiDataGrid::SortColumn() const { return dataGrid_->sortColumn; }
int DuiDataGrid::SortDirection() const { return dataGrid_->sortDirection; }
void DuiDataGrid::SetTextStyle(render::DuiTextStyle style) { dataGrid_->textStyle = std::move(style); dataGrid_->textStyleOverride = true; }
void DuiDataGrid::SetSelectionChangedHandler(std::function<void(int)> handler) { dataGrid_->selectionChanged = std::move(handler); }
void DuiDataGrid::SetColumnClickedHandler(std::function<void(int, int)> handler) { dataGrid_->columnClicked = std::move(handler); }
void DuiDataGrid::SetCheckChangedHandler(std::function<void(int, bool)> handler) { dataGrid_->checkChanged = std::move(handler); }
void DuiDataGrid::SetCellEditedHandler(std::function<void(int, int, std::string_view)> handler) { dataGrid_->cellEdited = std::move(handler); }

int DuiDataGrid::BodyWidth() const { return Bounds().Width() - (dataGrid_->scrollBar->Visible() ? ScrollBarWidth : 0); }
void DuiDataGrid::UpdateScrollRange()
{
    const int bodyHeight = (std::max)(0, Bounds().Height() - HeaderHeight());
    dataGrid_->scrollBar->SetRange(0, (std::max)(0, RowCount() * RowHeight() - bodyHeight));
    dataGrid_->scrollBar->SetPageSize((std::max)(1, bodyHeight));
    dataGrid_->scrollBar->SetVisible(RowCount() * RowHeight() > bodyHeight && bodyHeight > 0);
}
void DuiDataGrid::Layout(core::Rect bounds) { SetBounds(bounds); UpdateScrollRange(); const int left = bounds.right - ScrollBarWidth; dataGrid_->scrollBar->SetBounds({left, bounds.top + HeaderHeight(), bounds.right, bounds.bottom}); PlaceTextInput(); }
int DuiDataGrid::RowFromPoint(core::Point point) const { if (!Bounds().Contains(point) || point.y < Bounds().top + HeaderHeight() || point.x >= Bounds().left + BodyWidth()) return -1; const int row = (point.y - Bounds().top - HeaderHeight() + dataGrid_->scrollBar->Position()) / RowHeight(); return row >= 0 && row < RowCount() ? row : -1; }
int DuiDataGrid::ColumnFromPoint(core::Point point) const { if (point.x < Bounds().left || point.x >= Bounds().left + BodyWidth()) return -1; int left = Bounds().left + (CheckboxesVisible() ? CheckboxColumnWidth : 0); for (int column = 0; column < ColumnCount(); ++column) { if (point.x >= left && point.x < left + dataGrid_->columns[column].width) return column; left += dataGrid_->columns[column].width; } return -1; }
int DuiDataGrid::HeaderColumnFromPoint(core::Point point) const { return point.y >= Bounds().top && point.y < Bounds().top + HeaderHeight() ? ColumnFromPoint(point) : -1; }
core::Rect DuiDataGrid::CellRect(int row, int column) const { if (row < 0 || row >= RowCount() || column < 0 || column >= ColumnCount()) return {}; int left = Bounds().left + (CheckboxesVisible() ? CheckboxColumnWidth : 0); for (int index = 0; index < column; ++index) left += dataGrid_->columns[index].width; const int top = Bounds().top + HeaderHeight() + row * RowHeight() - dataGrid_->scrollBar->Position(); return {left, top, left + dataGrid_->columns[column].width, top + RowHeight()}; }
core::Rect DuiDataGrid::CheckboxRect(int row) const { const int top = Bounds().top + HeaderHeight() + row * RowHeight() - dataGrid_->scrollBar->Position(); return {Bounds().left + 4, top + (RowHeight() - 14) / 2, Bounds().left + 18, top + (RowHeight() - 14) / 2 + 14}; }
void DuiDataGrid::PlaceTextInput() { if (Editing()) { core::Rect bounds = CellRect(dataGrid_->editRow, dataGrid_->editColumn); bounds.left += 1; bounds.top += 1; bounds.right -= 1; bounds.bottom -= 1; dataGrid_->textInput->SetBounds(bounds); } }

void DuiDataGrid::SelectRow(int row, unsigned int modifiers, bool notify)
{
    if (row < 0 || row >= RowCount()) return;
    if (!MultiSelect() || (modifiers & (core::modifier::Control | core::modifier::Shift)) == 0) { SetSelectedRow(row, notify); return; }
    if ((modifiers & core::modifier::Shift) != 0 && dataGrid_->selectionAnchor >= 0) { dataGrid_->selection.clear(); const int first = (std::min)(dataGrid_->selectionAnchor, row); const int last = (std::max)(dataGrid_->selectionAnchor, row); for (int index = first; index <= last; ++index) dataGrid_->selection.push_back(index); dataGrid_->selectedRow = row; }
    else if ((modifiers & core::modifier::Control) != 0) { auto found = std::find(dataGrid_->selection.begin(), dataGrid_->selection.end(), row); if (found == dataGrid_->selection.end()) { dataGrid_->selection.push_back(row); dataGrid_->selectedRow = row; dataGrid_->selectionAnchor = row; } else { dataGrid_->selection.erase(found); dataGrid_->selectedRow = dataGrid_->selection.empty() ? -1 : dataGrid_->selection.front(); } }
    if (notify && dataGrid_->selectionChanged) dataGrid_->selectionChanged(dataGrid_->selectedRow);
}

bool DuiDataGrid::OnEvent(const core::Event& event)
{
    if (!Enabled()) return false;
    if (event.type == core::EventType::PointerWheel && Bounds().Contains(event.position)) { dataGrid_->scrollBar->SetPosition(dataGrid_->scrollBar->Position() + (event.wheelDelta > 0 ? -3 : 3) * RowHeight()); return event.wheelDelta != 0; }
    if (event.type == core::EventType::PointerDown) {
        if (Editing()) CommitEdit();
        const int header = HeaderColumnFromPoint(event.position);
        if (header >= 0) { if (dataGrid_->columns[header].sortable) { const int direction = SortColumn() == header && SortDirection() > 0 ? -1 : 1; SetSortIndicator(header, direction); if (dataGrid_->columnClicked) dataGrid_->columnClicked(header, direction); } return true; }
        const int row = RowFromPoint(event.position); if (row < 0) return false;
        if (CheckboxesVisible() && CheckboxRect(row).Contains(event.position)) { SetRowChecked(row, !RowChecked(row)); return true; }
        SelectRow(row, event.modifiers, true); return true;
    }
    if (event.type == core::EventType::PointerDoubleClick) { const int row = RowFromPoint(event.position); const int column = ColumnFromPoint(event.position); if (row < 0) return false; SetSelectedRow(row, false); return column >= 0 ? BeginEdit(row, column) : true; }
    if (event.type == core::EventType::KeyDown) { if (event.key == core::key::Enter && Editing()) { CommitEdit(); return true; } if (event.key == core::key::Escape && Editing()) { CancelEdit(); return true; } if (event.key == core::key::Function1 && SelectedRow() >= 0) return BeginEdit(SelectedRow(), 0); if (event.key == core::key::Up || event.key == core::key::Down) { if (RowCount() == 0) return false; const int row = std::clamp(SelectedRow() + (event.key == core::key::Down ? 1 : -1), 0, RowCount() - 1); SelectRow(row, 0, true); return true; } }
    return false;
}

void DuiDataGrid::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty); if (bounds.Empty() || !EffectivelyVisible()) return;
    const core::DuiTheme& theme = Theme();
    render::DuiTextStyle textStyle = dataGrid_->textStyle;
    if (!dataGrid_->textStyleOverride) textStyle.color = theme.Get(core::ThemeSlot::ListText);
    canvas.FillRect(bounds, theme.Get(core::ThemeSlot::GridBackground)); canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::GridBorder), 1.0F);
    const int bodyRight = Bounds().left + BodyWidth(); const core::Rect header{Bounds().left, Bounds().top, bodyRight, Bounds().top + HeaderHeight()};
    canvas.FillRect(header, theme.Get(core::ThemeSlot::GridHeaderBackground)); int left = Bounds().left + (CheckboxesVisible() ? CheckboxColumnWidth : 0);
    for (int column = 0; column < ColumnCount(); ++column) { const auto& definition = dataGrid_->columns[column]; std::string title = definition.title; if (column == SortColumn() && SortDirection() != 0) title += SortDirection() > 0 ? " +" : " -"; canvas.DrawText(title, {left + TextPadding, header.top, left + definition.width - TextPadding, header.bottom}, textStyle, definition.alignment, false); left += definition.width; canvas.FillRect({left - 1, header.top + 2, left, header.bottom - 2}, theme.Get(core::ThemeSlot::GridLine)); }
    canvas.PushClip({Bounds().left, Bounds().top + HeaderHeight(), bodyRight, Bounds().bottom}); const int first = dataGrid_->scrollBar->Position() / RowHeight(); const int count = (Bounds().Height() - HeaderHeight()) / RowHeight() + 2;
    for (int row = first; row < (std::min)(RowCount(), first + count); ++row) { const int top = Bounds().top + HeaderHeight() + row * RowHeight() - dataGrid_->scrollBar->Position(); const core::Rect rowRect{Bounds().left, top, bodyRight, top + RowHeight()}; if (IsRowSelected(row)) canvas.FillRect(rowRect, theme.Get(core::ThemeSlot::GridSelection)); canvas.FillRect({rowRect.left, rowRect.bottom - 1, rowRect.right, rowRect.bottom}, theme.Get(core::ThemeSlot::GridLine)); if (CheckboxesVisible()) { const auto check = CheckboxRect(row); canvas.StrokeRoundedRect(check, 1, theme.Get(core::ThemeSlot::ListCheckboxBorder), 1.0F); if (RowChecked(row)) canvas.FillRoundedRect(check, 1, theme.Get(core::ThemeSlot::BrandPrimary)); } for (int column = 0; column < ColumnCount(); ++column) { const auto cell = CellRect(row, column); const core::Rect textBounds{cell.left + TextPadding, cell.top, cell.right - TextPadding, cell.bottom}; if (Editing() && row == dataGrid_->editRow && column == dataGrid_->editColumn) ysDui::controls::detail::PaintTextInput(canvas, dataGrid_->textInput, {}, {}, textBounds, textStyle, textStyle.color, dataGrid_->columns[column].alignment, false, Focused(), theme.Get(core::ThemeSlot::SelectionBackground), theme.Get(core::ThemeSlot::TextOnSelectedRow)); else canvas.DrawText(CellText(row, column), textBounds, textStyle, dataGrid_->columns[column].alignment, false); } }
    canvas.PopClip();
}

} // namespace ysDui::controls::list
