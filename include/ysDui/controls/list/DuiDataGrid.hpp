#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiTextInput.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::list {

struct DuiDataGridColumn final {
    std::string title;
    int width{120};
    int minimumWidth{40};
    render::DuiTextAlignment alignment{render::DuiTextAlignment::Start};
    bool sortable{true};
    bool editable{true};
};

class DuiDataGrid final : public core::Control, public render::DuiRenderable {
public:
    DuiDataGrid();
    ~DuiDataGrid() override;
    DuiDataGrid(const DuiDataGrid&) = delete;
    DuiDataGrid& operator=(const DuiDataGrid&) = delete;
    DuiDataGrid(DuiDataGrid&&) = delete;
    DuiDataGrid& operator=(DuiDataGrid&&) = delete;

    int AddColumn(DuiDataGridColumn column);
    [[nodiscard]] int ColumnCount() const;
    [[nodiscard]] DuiDataGridColumn ColumnAt(int column) const;
    void SetColumnWidth(int column, int width);
    void SetColumnEditable(int column, bool editable);

    int AddRow();
    int InsertRow(int index);
    void RemoveRow(int index);
    void ClearRows();
    [[nodiscard]] int RowCount() const;
    void SetCellText(int row, int column, std::string text);
    [[nodiscard]] std::string CellText(int row, int column) const;

    void SetCheckboxesVisible(bool visible);
    [[nodiscard]] bool CheckboxesVisible() const;
    void SetRowChecked(int row, bool checked, bool notify = true);
    [[nodiscard]] bool RowChecked(int row) const;

    void SetSelectedRow(int row, bool notify = true);
    [[nodiscard]] int SelectedRow() const;
    void SetMultiSelect(bool enabled);
    [[nodiscard]] bool MultiSelect() const;
    [[nodiscard]] bool IsRowSelected(int row) const;
    [[nodiscard]] std::vector<int> SelectedRows() const;
    void ClearSelection(bool notify = true);

    void SetEditable(bool editable);
    [[nodiscard]] bool Editable() const;
    void SetTextInput(ui::DuiTextInput* textInput);
    bool BeginEdit(int row, int column);
    void CommitEdit();
    void CancelEdit();
    [[nodiscard]] bool Editing() const;
    [[nodiscard]] core::Rect CellRect(int row, int column) const;

    void SetRowHeight(int pixels);
    [[nodiscard]] int RowHeight() const;
    void SetHeaderHeight(int pixels);
    [[nodiscard]] int HeaderHeight() const;
    void SetSortIndicator(int column, int direction);
    [[nodiscard]] int SortColumn() const;
    [[nodiscard]] int SortDirection() const;

    void SetTextStyle(render::DuiTextStyle style);
    void SetSelectionChangedHandler(std::function<void(int)> handler);
    void SetColumnClickedHandler(std::function<void(int, int)> handler);
    void SetCheckChangedHandler(std::function<void(int, bool)> handler);
    void SetCellEditedHandler(std::function<void(int, int, std::string_view)> handler);
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    void UpdateScrollRange();
    void SelectRow(int row, unsigned int modifiers, bool notify);
    [[nodiscard]] int BodyWidth() const;
    [[nodiscard]] int RowFromPoint(core::Point point) const;
    [[nodiscard]] int ColumnFromPoint(core::Point point) const;
    [[nodiscard]] int HeaderColumnFromPoint(core::Point point) const;
    [[nodiscard]] core::Rect CheckboxRect(int row) const;
    void PlaceTextInput();

    class Impl;
    std::unique_ptr<Impl> dataGrid_;
};

} // namespace ysDui::controls::list
