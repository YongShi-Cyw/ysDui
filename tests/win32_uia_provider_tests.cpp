/**
 * 文件名：win32_uia_provider_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-27
 * 用途：验证平台无关无障碍覆盖到 Win32 UI Automation 属性的映射。
 */
#include "DuiWin32UiaProvider.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <string>

#include <UIAutomation.h>
#include <oleauto.h>

#include "ysDui/controls/basic/DuiExpander.hpp"
#include "ysDui/controls/basic/DuiButton.hpp"
#include "ysDui/controls/input/DuiEditHost.hpp"
#include "ysDui/controls/list/DuiSpreadsheet.hpp"
#include "ysDui/core/DuiHost.hpp"

namespace {

class TableControl final : public ysDui::core::Control
{
protected:
    [[nodiscard]] ysDui::core::DuiAccessibilityData CreateAccessibilityData() const override
    {
        return {ysDui::core::DuiAccessibilityRole::Table, "Spreadsheet", {}, {}, true, {}};
    }
};

class SpreadsheetModel final : public ysDui::controls::list::IDuiWorkbookModel
{
public:
    [[nodiscard]] int WorksheetCount() const override { return 1; }
    [[nodiscard]] ysDui::controls::list::DuiWorksheetId WorksheetIdAt(int index) const override { return index == 0 ? 1 : 0; }
    [[nodiscard]] std::string WorksheetName(ysDui::controls::list::DuiWorksheetId) const override { return "Sheet1"; }
    bool RenameWorksheet(ysDui::controls::list::DuiWorksheetId, std::string) override { return true; }
    [[nodiscard]] ysDui::controls::list::DuiWorksheetId AddWorksheet(std::string) override { return 0; }
    bool RemoveWorksheet(ysDui::controls::list::DuiWorksheetId) override { return false; }
    [[nodiscard]] int RowCount(ysDui::controls::list::DuiWorksheetId) const override
    {
        if (rowCountReadHandler) {
            auto handler = std::move(rowCountReadHandler);
            handler();
        }
        return 100;
    }
    [[nodiscard]] int ColumnCount(ysDui::controls::list::DuiWorksheetId) const override { return 100; }
    [[nodiscard]] std::string CellText(ysDui::controls::list::DuiWorksheetId,
                                       ysDui::controls::list::DuiCellAddress cell) const override
    {
        return cell == lastCell ? cellText : "Value";
    }
    [[nodiscard]] ysDui::controls::list::DuiCellPresentation CellPresentation(
        ysDui::controls::list::DuiWorksheetId, ysDui::controls::list::DuiCellAddress cell) const override
    {
        if (cell == lastCell)
            return {cellDisplayText, ysDui::controls::list::DuiCellValueKind::Number, "currency"};
        return {"Value", ysDui::controls::list::DuiCellValueKind::Text, {}};
    }
    [[nodiscard]] bool IsCellReadOnly(ysDui::controls::list::DuiWorksheetId,
                                      ysDui::controls::list::DuiCellAddress cell) const override
    {
        return readOnly && cell == lastCell;
    }
    bool SetCellText(ysDui::controls::list::DuiWorksheetId, ysDui::controls::list::DuiCellAddress cell,
                     std::string text) override
    {
        lastCell = cell;
        cellText = std::move(text);
        return true;
    }
    [[nodiscard]] int RowHeight(ysDui::controls::list::DuiWorksheetId, int) const override { return 24; }
    [[nodiscard]] int ColumnWidth(ysDui::controls::list::DuiWorksheetId, int) const override { return 96; }
    bool SetRowHeight(ysDui::controls::list::DuiWorksheetId, int, int) override { return true; }
    bool SetColumnWidth(ysDui::controls::list::DuiWorksheetId, int, int) override { return true; }

    ysDui::controls::list::DuiCellAddress lastCell{3, 4};
    std::string cellText{"1234.50"};
    std::string cellDisplayText{"$1,234.50"};
    bool readOnly{};
    mutable std::function<void()> rowCountReadHandler;
};

std::wstring ReadString(::IRawElementProviderSimple* provider, ::PROPERTYID property)
{
    ::VARIANT value{};
    ::VariantInit(&value);
    assert(provider->GetPropertyValue(property, &value) == S_OK);
    const std::wstring result = value.vt == VT_BSTR && value.bstrVal != nullptr ? value.bstrVal : L"";
    ::VariantClear(&value);
    return result;
}

} // namespace

int main()
{
    ::HWND window = ::CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                      100, 100, 320, 200, nullptr, nullptr,
                                      ::GetModuleHandleW(nullptr), nullptr);
    assert(window != nullptr);

    auto root = std::make_unique<ysDui::core::Control>();
    auto button = std::make_unique<ysDui::controls::basic::DuiButton>();
    button->SetText("&Save");
    button->SetAccessibilityName("Save changes");
    button->SetAccessibilityDescription("Writes the form to disk");
    button->SetAccessibilityIdentifier("9101");
    auto* rawButton = button.get();
    int invoked{};
    rawButton->SetClickHandler([&invoked] { ++invoked; });

    auto edit = std::make_unique<ysDui::controls::input::DuiEditHost>();
    auto* rawEdit = edit.get();
    edit->SetText("Initial");
    int valueChanges{};
    edit->SetTextChangedHandler([&valueChanges](std::string_view) { ++valueChanges; });

    auto expander = std::make_unique<ysDui::controls::basic::DuiExpander>();
    auto* rawExpander = expander.get();
    expander->SetTitle("Details");

    auto table = std::make_unique<TableControl>();
    auto* rawTable = table.get();

    SpreadsheetModel spreadsheetModel;
    auto spreadsheet = std::make_unique<ysDui::controls::list::DuiSpreadsheet>();
    spreadsheet->SetWorkbookModel(&spreadsheetModel);
    spreadsheet->Layout({0, 0, 320, 180});
    spreadsheet->SetActiveCell({1, 2});
    spreadsheet->SetSelection({{1, 2}, {3, 4}});
    auto* rawSpreadsheet = spreadsheet.get();

    root->AddChild(std::move(button));
    root->AddChild(std::move(edit));
    root->AddChild(std::move(expander));
    root->AddChild(std::move(table));
    root->AddChild(std::move(spreadsheet));

    auto host = std::make_shared<ysDui::core::Host>();
    host->SetRoot(std::move(root));
    ::IRawElementProviderSimple* provider =
        ysDui::platform::win32::CreateUiaElementProvider(window, host, rawButton);
    assert(provider != nullptr);
    ::ProviderOptions providerOptions{};
    assert(provider->get_ProviderOptions(&providerOptions) == S_OK);
    assert((providerOptions & ProviderOptions_ServerSideProvider) != 0);
    assert((providerOptions & ProviderOptions_UseComThreading) == 0);
    assert(ReadString(provider, UIA_NamePropertyId) == L"Save changes");
    assert(ReadString(provider, UIA_HelpTextPropertyId) == L"Writes the form to disk");
    assert(ReadString(provider, UIA_AutomationIdPropertyId) == L"9101");

    ::VARIANT controlType{};
    ::VariantInit(&controlType);
    assert(provider->GetPropertyValue(UIA_ControlTypePropertyId, &controlType) == S_OK);
    assert(controlType.vt == VT_I4 && controlType.lVal == UIA_ButtonControlTypeId);
    ::VariantClear(&controlType);

    ::VARIANT focusable{};
    ::VariantInit(&focusable);
    assert(provider->GetPropertyValue(UIA_IsKeyboardFocusablePropertyId, &focusable) == S_OK);
    assert(focusable.vt == VT_BOOL && focusable.boolVal == VARIANT_TRUE);
    ::VariantClear(&focusable);

    ::IUnknown* unknownPattern{};
    assert(provider->GetPatternProvider(UIA_InvokePatternId, &unknownPattern) == S_OK);
    assert(unknownPattern != nullptr);
    ::IInvokeProvider* invokeProvider{};
    assert(unknownPattern->QueryInterface(__uuidof(::IInvokeProvider),
                                          reinterpret_cast<void**>(&invokeProvider)) == S_OK);
    unknownPattern->Release();
    assert(invokeProvider->Invoke() == S_OK);
    assert(invoked == 1);
    rawButton->SetEnabled(false);
    assert(invokeProvider->Invoke() == UIA_E_ELEMENTNOTENABLED);
    rawButton->SetEnabled(true);
    ::IUnknown* unsupportedPattern{};
    assert(provider->GetPatternProvider(UIA_ValuePatternId, &unsupportedPattern) == S_OK);
    assert(unsupportedPattern == nullptr);
    invokeProvider->Release();

    provider->Release();

    provider = ysDui::platform::win32::CreateUiaElementProvider(window, host, rawEdit);
    assert(provider != nullptr);
    unknownPattern = nullptr;
    assert(provider->GetPatternProvider(UIA_ValuePatternId, &unknownPattern) == S_OK);
    assert(unknownPattern != nullptr);
    ::IValueProvider* valueProvider{};
    assert(unknownPattern->QueryInterface(__uuidof(::IValueProvider),
                                          reinterpret_cast<void**>(&valueProvider)) == S_OK);
    unknownPattern->Release();
    ::BSTR value{};
    assert(valueProvider->get_Value(&value) == S_OK);
    assert(std::wstring(value, ::SysStringLen(value)) == L"Initial");
    ::SysFreeString(value);
    assert(valueProvider->SetValue(L"中文 value") == S_OK);
    assert(rawEdit->Text() == "中文 value" && valueChanges == 1);
    ysDui::ui::DuiTextInputOptions readOnlyOptions;
    readOnlyOptions.readOnly = true;
    rawEdit->SetOptions(readOnlyOptions);
    ::BOOL readOnly{};
    assert(valueProvider->get_IsReadOnly(&readOnly) == S_OK && readOnly == TRUE);
    assert(valueProvider->SetValue(L"Rejected") == UIA_E_INVALIDOPERATION);
    assert(rawEdit->Text() == "中文 value" && valueChanges == 1);
    valueProvider->Release();
    provider->Release();

    provider = ysDui::platform::win32::CreateUiaElementProvider(window, host, rawExpander);
    assert(provider != nullptr);
    unknownPattern = nullptr;
    assert(provider->GetPatternProvider(UIA_ExpandCollapsePatternId, &unknownPattern) == S_OK);
    assert(unknownPattern != nullptr);
    ::IExpandCollapseProvider* expandProvider{};
    assert(unknownPattern->QueryInterface(__uuidof(::IExpandCollapseProvider),
                                          reinterpret_cast<void**>(&expandProvider)) == S_OK);
    unknownPattern->Release();
    ::ExpandCollapseState state{};
    assert(expandProvider->get_ExpandCollapseState(&state) == S_OK);
    assert(state == ExpandCollapseState_Expanded);
    assert(expandProvider->Collapse() == S_OK && !rawExpander->Expanded());
    assert(expandProvider->get_ExpandCollapseState(&state) == S_OK);
    assert(state == ExpandCollapseState_Collapsed);
    assert(expandProvider->Expand() == S_OK && rawExpander->Expanded());
    expandProvider->Release();
    provider->Release();

    provider = ysDui::platform::win32::CreateUiaElementProvider(window, host, rawTable);
    assert(provider != nullptr);
    ::VariantInit(&controlType);
    assert(provider->GetPropertyValue(UIA_ControlTypePropertyId, &controlType) == S_OK);
    assert(controlType.vt == VT_I4 && controlType.lVal == UIA_TableControlTypeId);
    ::VariantClear(&controlType);
    provider->Release();

    provider = ysDui::platform::win32::CreateUiaElementProvider(window, host, rawSpreadsheet);
    assert(provider != nullptr);
    unknownPattern = nullptr;
    assert(provider->GetPatternProvider(UIA_GridPatternId, &unknownPattern) == S_OK);
    assert(unknownPattern != nullptr);
    ::IGridProvider* gridProvider{};
    assert(unknownPattern->QueryInterface(__uuidof(::IGridProvider),
                                          reinterpret_cast<void**>(&gridProvider)) == S_OK);
    unknownPattern->Release();
    int rowCount{};
    int columnCount{};
    assert(gridProvider->get_RowCount(&rowCount) == S_OK && rowCount == 100);
    assert(gridProvider->get_ColumnCount(&columnCount) == S_OK && columnCount == 100);
    ::IRawElementProviderSimple* cellProvider{};
    assert(gridProvider->GetItem(3, 4, &cellProvider) == S_OK && cellProvider != nullptr);
    ::IRawElementProviderSimple* invalidCell{};
    assert(gridProvider->GetItem(-1, 2, &invalidCell) == E_INVALIDARG && invalidCell == nullptr);

    unknownPattern = nullptr;
    assert(cellProvider->GetPatternProvider(UIA_GridItemPatternId, &unknownPattern) == S_OK);
    assert(unknownPattern != nullptr);
    ::IGridItemProvider* gridItemProvider{};
    assert(unknownPattern->QueryInterface(__uuidof(::IGridItemProvider),
                                          reinterpret_cast<void**>(&gridItemProvider)) == S_OK);
    unknownPattern->Release();
    int row{};
    int column{};
    int rowSpan{};
    int columnSpan{};
    assert(gridItemProvider->get_Row(&row) == S_OK && row == 3);
    assert(gridItemProvider->get_Column(&column) == S_OK && column == 4);
    assert(gridItemProvider->get_RowSpan(&rowSpan) == S_OK && rowSpan == 1);
    assert(gridItemProvider->get_ColumnSpan(&columnSpan) == S_OK && columnSpan == 1);
    ::IRawElementProviderSimple* containingGrid{};
    assert(gridItemProvider->get_ContainingGrid(&containingGrid) == S_OK && containingGrid != nullptr);
    ::VARIANT containingGridType{};
    ::VariantInit(&containingGridType);
    assert(containingGrid->GetPropertyValue(UIA_ControlTypePropertyId, &containingGridType) == S_OK);
    assert(containingGridType.vt == VT_I4 && containingGridType.lVal == UIA_TableControlTypeId);
    ::VariantClear(&containingGridType);
    containingGrid->Release();

    unknownPattern = nullptr;
    assert(cellProvider->GetPatternProvider(UIA_ValuePatternId, &unknownPattern) == S_OK);
    assert(unknownPattern != nullptr);
    ::IValueProvider* cellValueProvider{};
    assert(unknownPattern->QueryInterface(__uuidof(::IValueProvider),
                                          reinterpret_cast<void**>(&cellValueProvider)) == S_OK);
    unknownPattern->Release();
    value = nullptr;
    assert(cellValueProvider->get_Value(&value) == S_OK);
    assert(std::wstring(value, ::SysStringLen(value)) == L"$1,234.50");
    ::SysFreeString(value);
    assert(cellValueProvider->SetValue(L"Grid value") == S_OK);
    assert(spreadsheetModel.CellText(1, {3, 4}) == "Grid value");
    spreadsheetModel.readOnly = true;
    assert(cellValueProvider->get_IsReadOnly(&readOnly) == S_OK && readOnly == TRUE);
    assert(cellValueProvider->SetValue(L"Blocked") == UIA_E_INVALIDOPERATION);
    assert(spreadsheetModel.CellText(1, {3, 4}) == "Grid value");
    spreadsheetModel.readOnly = false;

    unknownPattern = nullptr;
    assert(provider->GetPatternProvider(UIA_TablePatternId, &unknownPattern) == S_OK);
    assert(unknownPattern != nullptr);
    ::ITableProvider* tableProvider{};
    assert(unknownPattern->QueryInterface(__uuidof(::ITableProvider),
                                          reinterpret_cast<void**>(&tableProvider)) == S_OK);
    unknownPattern->Release();
    ::RowOrColumnMajor major{};
    assert(tableProvider->get_RowOrColumnMajor(&major) == S_OK
           && major == RowOrColumnMajor_RowMajor);
    ::SAFEARRAY* rowHeaders{};
    ::SAFEARRAY* columnHeaders{};
    assert(tableProvider->GetRowHeaders(&rowHeaders) == S_OK && rowHeaders == nullptr);
    assert(tableProvider->GetColumnHeaders(&columnHeaders) == S_OK && columnHeaders == nullptr);

    unknownPattern = nullptr;
    assert(provider->GetPatternProvider(UIA_SelectionPatternId, &unknownPattern) == S_OK);
    assert(unknownPattern != nullptr);
    ::ISelectionProvider* selectionProvider{};
    assert(unknownPattern->QueryInterface(__uuidof(::ISelectionProvider),
                                          reinterpret_cast<void**>(&selectionProvider)) == S_OK);
    unknownPattern->Release();
    ::BOOL canSelectMultiple{};
    ::BOOL selectionRequired{};
    assert(selectionProvider->get_CanSelectMultiple(&canSelectMultiple) == S_OK && canSelectMultiple == FALSE);
    assert(selectionProvider->get_IsSelectionRequired(&selectionRequired) == S_OK && selectionRequired == TRUE);
    ::SAFEARRAY* selected{};
    assert(selectionProvider->GetSelection(&selected) == S_OK && selected != nullptr);
    ::LONG first{};
    ::LONG last{};
    assert(::SafeArrayGetDim(selected) == 1
           && ::SafeArrayGetLBound(selected, 1, &first) == S_OK
           && ::SafeArrayGetUBound(selected, 1, &last) == S_OK
           && first == 0 && last == 0);
    ::IRawElementProviderSimple* selectedElement{};
    assert(::SafeArrayGetElement(selected, &first, &selectedElement) == S_OK && selectedElement != nullptr);
    assert(ReadString(selectedElement, UIA_NamePropertyId).find(L"selected C2:E4") != std::wstring::npos);
    unknownPattern = nullptr;
    assert(selectedElement->GetPatternProvider(UIA_SelectionItemPatternId, &unknownPattern) == S_OK);
    assert(unknownPattern != nullptr);
    ::ISelectionItemProvider* selectionItem{};
    assert(unknownPattern->QueryInterface(__uuidof(::ISelectionItemProvider),
                                          reinterpret_cast<void**>(&selectionItem)) == S_OK);
    unknownPattern->Release();
    ::BOOL isSelected{};
    assert(selectionItem->get_IsSelected(&isSelected) == S_OK && isSelected == TRUE);
    assert(selectionItem->Select() == S_OK);
    assert(selectionItem->RemoveFromSelection() == UIA_E_INVALIDOPERATION);
    ::IRawElementProviderSimple* selectionContainer{};
    assert(selectionItem->get_SelectionContainer(&selectionContainer) == S_OK && selectionContainer != nullptr);
    ::VariantInit(&controlType);
    assert(selectionContainer->GetPropertyValue(UIA_ControlTypePropertyId, &controlType) == S_OK);
    assert(controlType.vt == VT_I4 && controlType.lVal == UIA_TableControlTypeId);
    ::VariantClear(&controlType);
    selectionContainer->Release();
    selectionProvider->Release();

    spreadsheetModel.rowCountReadHandler = [&host] { host->SetRoot({}); };
    assert(gridProvider->get_RowCount(&rowCount) == UIA_E_ELEMENTNOTAVAILABLE);
    value = nullptr;
    assert(cellValueProvider->get_Value(&value) == UIA_E_ELEMENTNOTAVAILABLE && value == nullptr);
    cellValueProvider->Release();
    assert(gridItemProvider->get_Row(&row) == UIA_E_ELEMENTNOTAVAILABLE);
    gridItemProvider->Release();
    cellProvider->Release();
    assert(selectionItem->get_IsSelected(&isSelected) == UIA_E_ELEMENTNOTAVAILABLE);
    selectionItem->Release();
    selectedElement->Release();
    ::SafeArrayDestroy(selected);
    tableProvider->Release();
    gridProvider->Release();
    provider->Release();

    ::DestroyWindow(window);
}
