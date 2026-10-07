/**
 * 文件名：DuiWin32SpreadsheetUiaProvider.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-02
 * 用途：实现工作表虚拟单元格的 Win32 UI Automation Provider。
 */
#include "DuiWin32SpreadsheetUiaProvider.hpp"

#include "DuiWin32UiaProvider.hpp"
#include "DuiWin32Utf8.hpp"

#include <UIAutomation.h>
#include <UIAutomationCore.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

#include "ysDui/controls/list/DuiSpreadsheet.hpp"
#include "ysDui/core/DuiDpi.hpp"
#include "ysDui/core/DuiHost.hpp"

namespace ysDui::platform::win32 {
namespace {

bool Contains(const core::Control* root, const core::Control* target)
{
    if (!root || !target)
        return false;
    if (root == target)
        return true;
    for (const auto& child : root->Children()) {
        if (Contains(child.get(), target))
            return true;
    }
    return false;
}

bool IsEffectivelyEnabled(const core::Control* control)
{
    for (const core::Control* current = control; current; current = current->Parent()) {
        if (!current->Enabled())
            return false;
    }
    return control != nullptr;
}

void SetString(::VARIANT* result, std::string_view value)
{
    result->vt = VT_BSTR;
    const std::wstring nativeValue = detail::Utf8ToWide(value);
    result->bstrVal = ::SysAllocStringLen(nativeValue.data(), static_cast<::UINT>(nativeValue.size()));
}

std::string CellName(controls::list::DuiCellAddress cell)
{
    std::string column;
    for (int value = cell.column + 1; value > 0; value = (value - 1) / 26)
        column.insert(column.begin(), static_cast<char>('A' + (value - 1) % 26));
    return column + std::to_string(cell.row + 1);
}

::UiaRect ScreenBounds(::HWND window, core::Rect logicalBounds)
{
    const core::DuiDpiScale scale(static_cast<int>(::GetDpiForWindow(window)));
    const core::Rect bounds = scale.Scale(logicalBounds);
    ::POINT topLeft{bounds.left, bounds.top};
    ::POINT bottomRight{bounds.right, bounds.bottom};
    ::ClientToScreen(window, &topLeft);
    ::ClientToScreen(window, &bottomRight);
    return {static_cast<double>(topLeft.x), static_cast<double>(topLeft.y),
            static_cast<double>(bottomRight.x - topLeft.x),
            static_cast<double>(bottomRight.y - topLeft.y)};
}

::HRESULT SetBoundingRectangle(::VARIANT* result, const ::UiaRect& bounds)
{
    ::SAFEARRAY* values = ::SafeArrayCreateVector(VT_R8, 0, 4);
    if (!values)
        return E_OUTOFMEMORY;
    const double dimensions[] = {bounds.left, bounds.top, bounds.width, bounds.height};
    for (::LONG index = 0; index < 4; ++index) {
        const ::HRESULT status = ::SafeArrayPutElement(
            values, &index, const_cast<double*>(&dimensions[index]));
        if (FAILED(status)) {
            ::SafeArrayDestroy(values);
            return status;
        }
    }
    result->vt = VT_ARRAY | VT_R8;
    result->parray = values;
    return S_OK;
}

class SpreadsheetCellProvider final : public ::IRawElementProviderSimple,
                                      public ::IRawElementProviderFragment,
                                      public ::IGridItemProvider,
                                      public ::IValueProvider
{
public:
    SpreadsheetCellProvider(::HWND window, std::shared_ptr<core::Host> host,
                            controls::list::DuiSpreadsheet* spreadsheet,
                            controls::list::IDuiWorkbookModel* model,
                            controls::list::DuiWorksheetId sheet,
                            controls::list::DuiCellAddress cell)
        : window_(window), host_(std::move(host)), spreadsheet_(spreadsheet), model_(model),
          sheet_(sheet), cell_(cell)
    {
    }

    ::HRESULT STDMETHODCALLTYPE QueryInterface(REFIID identifier, void** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (identifier == IID_IUnknown || identifier == __uuidof(::IRawElementProviderSimple))
            *result = static_cast<::IRawElementProviderSimple*>(this);
        else if (identifier == __uuidof(::IRawElementProviderFragment))
            *result = static_cast<::IRawElementProviderFragment*>(this);
        else if (identifier == __uuidof(::IGridItemProvider))
            *result = static_cast<::IGridItemProvider*>(this);
        else if (identifier == __uuidof(::IValueProvider))
            *result = static_cast<::IValueProvider*>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }

    ::ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ::ULONG STDMETHODCALLTYPE Release() override
    {
        const ::ULONG references = --references_;
        if (references == 0)
            delete this;
        return references;
    }

    ::HRESULT STDMETHODCALLTYPE get_ProviderOptions(::ProviderOptions* result) override
    {
        if (!result)
            return E_POINTER;
        *result = ProviderOptions_ServerSideProvider;
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE GetPatternProvider(::PATTERNID pattern, ::IUnknown** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (pattern == UIA_GridItemPatternId)
            return QueryInterface(__uuidof(::IGridItemProvider), reinterpret_cast<void**>(result));
        if (pattern == UIA_ValuePatternId)
            return QueryInterface(__uuidof(::IValueProvider), reinterpret_cast<void**>(result));
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE GetPropertyValue(::PROPERTYID property, ::VARIANT* result) override
    {
        if (!result)
            return E_POINTER;
        ::VariantInit(result);
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        switch (property) {
        case UIA_ControlTypePropertyId:
            result->vt = VT_I4;
            result->lVal = UIA_DataItemControlTypeId;
            return S_OK;
        case UIA_NamePropertyId:
        {
            std::string value;
            if (!ReadCellText(value))
                return UIA_E_ELEMENTNOTAVAILABLE;
            const std::string name = value.empty() ? CellName(cell_) : CellName(cell_) + " " + value;
            SetString(result, name);
            return S_OK;
        }
        case UIA_ValueValuePropertyId:
        {
            std::string value;
            if (!ReadCellText(value))
                return UIA_E_ELEMENTNOTAVAILABLE;
            SetString(result, value);
            return S_OK;
        }
        case UIA_ClassNamePropertyId:
            result->vt = VT_BSTR;
            result->bstrVal = ::SysAllocString(L"ysDui.SpreadsheetCell");
            return S_OK;
        case UIA_FrameworkIdPropertyId:
            result->vt = VT_BSTR;
            result->bstrVal = ::SysAllocString(L"ysDui");
            return S_OK;
        case UIA_IsEnabledPropertyId:
            result->vt = VT_BOOL;
            result->boolVal = IsEffectivelyEnabled(spreadsheet_) ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case UIA_IsOffscreenPropertyId:
            result->vt = VT_BOOL;
            result->boolVal = spreadsheet_->VisibleCellBounds(cell_).has_value()
                ? VARIANT_FALSE : VARIANT_TRUE;
            return S_OK;
        case UIA_IsKeyboardFocusablePropertyId:
            result->vt = VT_BOOL;
            result->boolVal = VARIANT_TRUE;
            return S_OK;
        case UIA_HasKeyboardFocusPropertyId:
            result->vt = VT_BOOL;
            result->boolVal = host_->FocusedControl() == spreadsheet_
                && spreadsheet_->ActiveCell() == cell_ ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case UIA_ValueIsReadOnlyPropertyId:
            result->vt = VT_BOOL;
            result->boolVal = VARIANT_FALSE;
            return S_OK;
        case UIA_GridItemRowPropertyId:
            result->vt = VT_I4;
            result->lVal = cell_.row;
            return S_OK;
        case UIA_GridItemColumnPropertyId:
            result->vt = VT_I4;
            result->lVal = cell_.column;
            return S_OK;
        case UIA_GridItemRowSpanPropertyId:
        case UIA_GridItemColumnSpanPropertyId:
            result->vt = VT_I4;
            result->lVal = 1;
            return S_OK;
        case UIA_BoundingRectanglePropertyId:
            return SetBoundingRectangle(result, CurrentScreenBounds());
        default:
            return S_OK;
        }
    }

    ::HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
        ::IRawElementProviderSimple** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE Navigate(::NavigateDirection direction,
                                         ::IRawElementProviderFragment** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (direction != NavigateDirection_Parent)
            return S_OK;
        ::IRawElementProviderSimple* provider = CreateUiaElementProvider(window_, host_, spreadsheet_);
        if (!provider)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const ::HRESULT status = provider->QueryInterface(
            __uuidof(::IRawElementProviderFragment), reinterpret_cast<void**>(result));
        provider->Release();
        return status;
    }

    ::HRESULT STDMETHODCALLTYPE GetRuntimeId(::SAFEARRAY** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        ::SAFEARRAY* runtimeId = ::SafeArrayCreateVector(VT_I4, 0, 5);
        if (!runtimeId)
            return E_OUTOFMEMORY;
        const ::LONG values[] = {
            UiaAppendRuntimeId,
            static_cast<::LONG>(reinterpret_cast<std::uintptr_t>(window_)),
            static_cast<::LONG>(reinterpret_cast<std::uintptr_t>(spreadsheet_)),
            cell_.row,
            cell_.column,
        };
        for (::LONG index = 0; index < 5; ++index) {
            const ::HRESULT status = ::SafeArrayPutElement(
                runtimeId, &index, const_cast<::LONG*>(&values[index]));
            if (FAILED(status)) {
                ::SafeArrayDestroy(runtimeId);
                return status;
            }
        }
        *result = runtimeId;
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE get_BoundingRectangle(::UiaRect* result) override
    {
        if (!result)
            return E_POINTER;
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        *result = CurrentScreenBounds();
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(::SAFEARRAY** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE SetFocus() override
    {
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (!IsEffectivelyEnabled(spreadsheet_))
            return UIA_E_ELEMENTNOTENABLED;
        if (spreadsheet_->ActiveCell() != cell_)
            spreadsheet_->SetActiveCell(cell_);
        if (!IsAlive() || spreadsheet_->ActiveCell() != cell_)
            return UIA_E_ELEMENTNOTAVAILABLE;
        ::SetFocus(window_);
        host_->SetFocusedControl(spreadsheet_);
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE get_FragmentRoot(
        ::IRawElementProviderFragmentRoot** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        ::IRawElementProviderSimple* provider = CreateUiaElementProvider(window_, host_, spreadsheet_);
        if (!provider)
            return UIA_E_ELEMENTNOTAVAILABLE;
        ::IRawElementProviderFragment* fragment{};
        ::HRESULT status = provider->QueryInterface(
            __uuidof(::IRawElementProviderFragment), reinterpret_cast<void**>(&fragment));
        provider->Release();
        if (FAILED(status))
            return status;
        status = fragment->get_FragmentRoot(result);
        fragment->Release();
        return status;
    }

    ::HRESULT STDMETHODCALLTYPE get_Row(int* result) override
    {
        return ReturnCoordinate(result, cell_.row);
    }

    ::HRESULT STDMETHODCALLTYPE get_Column(int* result) override
    {
        return ReturnCoordinate(result, cell_.column);
    }

    ::HRESULT STDMETHODCALLTYPE get_RowSpan(int* result) override
    {
        return ReturnCoordinate(result, 1);
    }

    ::HRESULT STDMETHODCALLTYPE get_ColumnSpan(int* result) override
    {
        return ReturnCoordinate(result, 1);
    }

    ::HRESULT STDMETHODCALLTYPE get_ContainingGrid(
        ::IRawElementProviderSimple** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        *result = CreateUiaElementProvider(window_, host_, spreadsheet_);
        return *result ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }

    ::HRESULT STDMETHODCALLTYPE SetValue(::LPCWSTR value) override
    {
        if (!value)
            return E_INVALIDARG;
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (!IsEffectivelyEnabled(spreadsheet_))
            return UIA_E_ELEMENTNOTENABLED;
        if (model_->IsCellReadOnly(sheet_, cell_))
            return IsAlive() ? UIA_E_INVALIDOPERATION : UIA_E_ELEMENTNOTAVAILABLE;
        if (!IsAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        const std::wstring_view nativeValue(value);
        const std::string utf8Value = detail::WideToUtf8(nativeValue);
        if (!nativeValue.empty() && utf8Value.empty())
            return E_INVALIDARG;
        if (spreadsheet_->ActiveCell() != cell_)
            spreadsheet_->SetActiveCell(cell_);
        if (!IsAlive() || spreadsheet_->ActiveCell() != cell_)
            return UIA_E_ELEMENTNOTAVAILABLE;
        return static_cast<core::Control*>(spreadsheet_)->PerformAccessibilityAction(
            core::DuiAccessibilityAction::SetValue, utf8Value) ? S_OK : UIA_E_INVALIDOPERATION;
    }

    ::HRESULT STDMETHODCALLTYPE get_Value(::BSTR* result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        std::string value;
        if (!ReadCellText(value))
            return UIA_E_ELEMENTNOTAVAILABLE;
        const std::wstring nativeValue = detail::Utf8ToWide(value);
        *result = ::SysAllocStringLen(nativeValue.data(), static_cast<::UINT>(nativeValue.size()));
        return *result || nativeValue.empty() ? S_OK : E_OUTOFMEMORY;
    }

    ::HRESULT STDMETHODCALLTYPE get_IsReadOnly(::BOOL* result) override
    {
        if (!result)
            return E_POINTER;
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        const bool readOnly = model_->IsCellReadOnly(sheet_, cell_);
        if (!IsAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        *result = readOnly ? TRUE : FALSE;
        return S_OK;
    }

private:
    [[nodiscard]] bool IsAlive() const
    {
        return host_ && Contains(host_->Root(), spreadsheet_)
            && spreadsheet_->WorkbookModel() == model_
            && spreadsheet_->ActiveWorksheet() == sheet_;
    }

    [[nodiscard]] bool CellExists() const
    {
        if (!IsAlive())
            return false;
        const int rows = model_->RowCount(sheet_);
        if (!IsAlive())
            return false;
        const int columns = model_->ColumnCount(sheet_);
        return IsAlive() && cell_.row >= 0 && cell_.row < rows
            && cell_.column >= 0 && cell_.column < columns;
    }

    [[nodiscard]] bool ReadCellText(std::string& result) const
    {
        if (!CellExists())
            return false;
        result = model_->CellPresentation(sheet_, cell_).displayText;
        return IsAlive();
    }

    [[nodiscard]] ::UiaRect CurrentScreenBounds() const
    {
        const auto bounds = spreadsheet_->VisibleCellBounds(cell_);
        return bounds ? ScreenBounds(window_, *bounds) : ::UiaRect{};
    }

    ::HRESULT ReturnCoordinate(int* result, int value) const
    {
        if (!result)
            return E_POINTER;
        if (!CellExists())
            return UIA_E_ELEMENTNOTAVAILABLE;
        *result = value;
        return S_OK;
    }

    std::atomic<::ULONG> references_{1};
    ::HWND window_{};
    std::shared_ptr<core::Host> host_;
    controls::list::DuiSpreadsheet* spreadsheet_{};
    controls::list::IDuiWorkbookModel* model_{};
    controls::list::DuiWorksheetId sheet_{};
    controls::list::DuiCellAddress cell_{};
};

} // namespace

bool SupportsSpreadsheetGridUia(const core::Control* control)
{
    const auto* spreadsheet = dynamic_cast<const controls::list::DuiSpreadsheet*>(control);
    return spreadsheet && spreadsheet->WorkbookModel() != nullptr
        && spreadsheet->ActiveWorksheet() != 0;
}

bool SupportsSpreadsheetTableUia(const core::Control* control)
{
    return SupportsSpreadsheetGridUia(control);
}

::HRESULT GetSpreadsheetGridItem(::HWND window, const std::shared_ptr<core::Host>& host,
                                 core::Control* control, int row, int column,
                                 ::IRawElementProviderSimple** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!host || !Contains(host->Root(), control))
        return UIA_E_ELEMENTNOTAVAILABLE;
    auto* spreadsheet = dynamic_cast<controls::list::DuiSpreadsheet*>(control);
    if (!spreadsheet || !SupportsSpreadsheetGridUia(control))
        return UIA_E_NOTSUPPORTED;
    controls::list::IDuiWorkbookModel* const model = spreadsheet->WorkbookModel();
    const controls::list::DuiWorksheetId sheet = spreadsheet->ActiveWorksheet();
    const auto contextValid = [&]
    {
        return host && Contains(host->Root(), control) && spreadsheet->WorkbookModel() == model
            && spreadsheet->ActiveWorksheet() == sheet;
    };
    const int rows = model->RowCount(sheet);
    if (!contextValid())
        return UIA_E_ELEMENTNOTAVAILABLE;
    const int columns = model->ColumnCount(sheet);
    if (!contextValid())
        return UIA_E_ELEMENTNOTAVAILABLE;
    if (row < 0 || row >= rows || column < 0 || column >= columns)
        return E_INVALIDARG;
    *result = CreateSpreadsheetCellUiaProvider(window, host, spreadsheet, model, sheet,
                                               {row, column});
    return *result ? S_OK : E_OUTOFMEMORY;
}

::HRESULT GetSpreadsheetGridRowCount(const std::shared_ptr<core::Host>& host,
                                     core::Control* control, int* result)
{
    if (!result)
        return E_POINTER;
    if (!host || !Contains(host->Root(), control))
        return UIA_E_ELEMENTNOTAVAILABLE;
    auto* spreadsheet = dynamic_cast<controls::list::DuiSpreadsheet*>(control);
    if (!spreadsheet || !SupportsSpreadsheetGridUia(control))
        return UIA_E_NOTSUPPORTED;
    controls::list::IDuiWorkbookModel* const model = spreadsheet->WorkbookModel();
    const controls::list::DuiWorksheetId sheet = spreadsheet->ActiveWorksheet();
    const int rows = model->RowCount(sheet);
    if (!host || !Contains(host->Root(), control) || spreadsheet->WorkbookModel() != model
        || spreadsheet->ActiveWorksheet() != sheet)
    {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *result = rows;
    return S_OK;
}

::HRESULT GetSpreadsheetGridColumnCount(const std::shared_ptr<core::Host>& host,
                                        core::Control* control, int* result)
{
    if (!result)
        return E_POINTER;
    if (!host || !Contains(host->Root(), control))
        return UIA_E_ELEMENTNOTAVAILABLE;
    auto* spreadsheet = dynamic_cast<controls::list::DuiSpreadsheet*>(control);
    if (!spreadsheet || !SupportsSpreadsheetGridUia(control))
        return UIA_E_NOTSUPPORTED;
    controls::list::IDuiWorkbookModel* const model = spreadsheet->WorkbookModel();
    const controls::list::DuiWorksheetId sheet = spreadsheet->ActiveWorksheet();
    const int columns = model->ColumnCount(sheet);
    if (!host || !Contains(host->Root(), control) || spreadsheet->WorkbookModel() != model
        || spreadsheet->ActiveWorksheet() != sheet)
    {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *result = columns;
    return S_OK;
}

::HRESULT GetSpreadsheetTableHeaders(const std::shared_ptr<core::Host>& host,
                                     core::Control* control, ::SAFEARRAY** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!host || !Contains(host->Root(), control))
        return UIA_E_ELEMENTNOTAVAILABLE;
    return SupportsSpreadsheetTableUia(control) ? S_OK : UIA_E_NOTSUPPORTED;
}

::HRESULT GetSpreadsheetTableMajor(const std::shared_ptr<core::Host>& host,
                                   core::Control* control, ::RowOrColumnMajor* result)
{
    if (!result)
        return E_POINTER;
    if (!host || !Contains(host->Root(), control))
        return UIA_E_ELEMENTNOTAVAILABLE;
    if (!SupportsSpreadsheetTableUia(control))
        return UIA_E_NOTSUPPORTED;
    *result = RowOrColumnMajor_RowMajor;
    return S_OK;
}

::IRawElementProviderSimple* CreateSpreadsheetCellUiaProvider(
    ::HWND window, std::shared_ptr<core::Host> host,
    controls::list::DuiSpreadsheet* spreadsheet,
    controls::list::IDuiWorkbookModel* model,
    controls::list::DuiWorksheetId sheet,
    controls::list::DuiCellAddress cell)
{
    if (!window || !host || !spreadsheet || !model || !cell.Valid())
        return nullptr;
    return new SpreadsheetCellProvider(window, std::move(host), spreadsheet, model, sheet, cell);
}

} // namespace ysDui::platform::win32
