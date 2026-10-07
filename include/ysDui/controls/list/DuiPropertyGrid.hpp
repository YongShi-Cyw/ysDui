#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiTextInput.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::list {

class DuiPropertyGrid final : public core::Control, public render::DuiRenderable {
public:
    enum class RowType { Group, String, Bool, Int, Enum };

    struct EnumOption final {
        std::string text;
        int value{};
    };

    DuiPropertyGrid();
    ~DuiPropertyGrid() override;
    DuiPropertyGrid(const DuiPropertyGrid&) = delete;
    DuiPropertyGrid& operator=(const DuiPropertyGrid&) = delete;
    DuiPropertyGrid(DuiPropertyGrid&&) = delete;
    DuiPropertyGrid& operator=(DuiPropertyGrid&&) = delete;

    int AddGroup(std::string title);
    int AddString(std::string name, std::string value);
    int AddBool(std::string name, bool value);
    int AddInt(std::string name, int value);
    int AddEnum(std::string name, std::vector<EnumOption> options, int value);
    void Clear();
    [[nodiscard]] int RowCount() const;
    [[nodiscard]] RowType TypeAt(int row) const;
    [[nodiscard]] std::string NameAt(int row) const;
    [[nodiscard]] std::string StringValueAt(int row) const;
    void SetStringValue(int row, std::string value, bool notify = true);
    [[nodiscard]] bool BoolValueAt(int row) const;
    void SetBoolValue(int row, bool value, bool notify = true);
    [[nodiscard]] int IntValueAt(int row) const;
    void SetIntValue(int row, int value, bool notify = true);
    [[nodiscard]] int EnumValueAt(int row) const;
    void SetEnumValue(int row, int value, bool notify = true);

    void SetNameColumnWidth(int pixels);
    [[nodiscard]] int NameColumnWidth() const;
    void SetRowHeight(int pixels);
    [[nodiscard]] int RowHeight() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    [[nodiscard]] core::Rect RowRect(int row) const;
    [[nodiscard]] core::Rect ValueRect(int row) const;
    void SetTextInput(ui::DuiTextInput* input);
    bool BeginEdit(int row);
    void CommitEdit();
    void CancelEdit();
    [[nodiscard]] bool Editing() const;
    void SetSelectedRow(int row);
    [[nodiscard]] int SelectedRow() const;
    void SetValueChangedHandler(std::function<void(int)> handler);
    void SetEditedHandler(std::function<void(int)> handler);
    void SetTextStyle(render::DuiTextStyle style);
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    void UpdateScrollRange();
    [[nodiscard]] int RowFromPoint(core::Point point) const;
    [[nodiscard]] std::string DisplayValue(int row) const;
    void PlaceTextInput();

    class Impl;
    std::unique_ptr<Impl> propertyGrid_;
};

} // namespace ysDui::controls::list
