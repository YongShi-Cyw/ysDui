/**
 * 文件名：DuiWin32UiaProvider.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现仅在 Win32 后端使用的 UI Automation Fragment 提供器。
 */
#include "DuiWin32UiaProvider.hpp"
#include "DuiWin32SpreadsheetUiaProvider.hpp"
#include "DuiWin32Utf8.hpp"

#include <UIAutomation.h>
#include <UIAutomationCore.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

#include "ysDui/core/DuiAccessibility.hpp"
#include "ysDui/core/DuiDpi.hpp"
#include "ysDui/controls/list/DuiSpreadsheet.hpp"

namespace ysDui::platform::win32 {
namespace {

CONTROLTYPEID ToControlType(core::DuiAccessibilityRole role)
{
    switch (role) {
    case core::DuiAccessibilityRole::Group: return UIA_GroupControlTypeId;
    case core::DuiAccessibilityRole::Button: return UIA_ButtonControlTypeId;
    case core::DuiAccessibilityRole::CheckBox: return UIA_CheckBoxControlTypeId;
    case core::DuiAccessibilityRole::RadioButton: return UIA_RadioButtonControlTypeId;
    case core::DuiAccessibilityRole::Hyperlink: return UIA_HyperlinkControlTypeId;
    case core::DuiAccessibilityRole::Edit: return UIA_EditControlTypeId;
    case core::DuiAccessibilityRole::Text: return UIA_TextControlTypeId;
    case core::DuiAccessibilityRole::Switch: return UIA_CheckBoxControlTypeId;
    case core::DuiAccessibilityRole::Slider: return UIA_SliderControlTypeId;
    case core::DuiAccessibilityRole::ComboBox: return UIA_ComboBoxControlTypeId;
    case core::DuiAccessibilityRole::List: return UIA_ListControlTypeId;
    case core::DuiAccessibilityRole::Table: return UIA_TableControlTypeId;
    case core::DuiAccessibilityRole::Tree: return UIA_TreeControlTypeId;
    case core::DuiAccessibilityRole::ProgressBar: return UIA_ProgressBarControlTypeId;
    case core::DuiAccessibilityRole::ScrollBar: return UIA_ScrollBarControlTypeId;
    case core::DuiAccessibilityRole::Image: return UIA_ImageControlTypeId;
    case core::DuiAccessibilityRole::ToolBar: return UIA_ToolBarControlTypeId;
    case core::DuiAccessibilityRole::StatusBar: return UIA_StatusBarControlTypeId;
    case core::DuiAccessibilityRole::Tab: return UIA_TabControlTypeId;
    case core::DuiAccessibilityRole::Menu: return UIA_MenuControlTypeId;
    case core::DuiAccessibilityRole::Pane: return UIA_PaneControlTypeId;
    default: return UIA_PaneControlTypeId;
    }
}

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

bool IsAlive(const std::shared_ptr<core::Host>& host, const core::Control* control)
{
    return host && Contains(host->Root(), control);
}

/**
 * @return 控件当前是否可交互。
 * 说明：`Control::Enabled()` 已沿父链求值，此处只补空指针判断，保留命名以表达"有效启用"的调用意图。
 */
bool IsEffectivelyEnabled(const core::Control* control)
{
    return control != nullptr && control->Enabled();
}

bool SupportsSpreadsheetSelection(const core::Control* control)
{
    const auto* spreadsheet = dynamic_cast<const controls::list::DuiSpreadsheet*>(control);
    return spreadsheet && spreadsheet->WorkbookModel() != nullptr
        && spreadsheet->ActiveWorksheet() != 0;
}

UiaRect ScreenBounds(::HWND window, const core::Control* control)
{
    core::Rect bounds{};
    if (control) {
        const core::DuiDpiScale scale(static_cast<int>(::GetDpiForWindow(window)));
        bounds = scale.Scale(control->Bounds());
    } else {
        ::RECT client{};
        ::GetClientRect(window, &client);
        bounds = {client.left, client.top, client.right, client.bottom};
    }
    ::POINT topLeft{bounds.left, bounds.top};
    ::POINT bottomRight{bounds.right, bounds.bottom};
    ::ClientToScreen(window, &topLeft);
    ::ClientToScreen(window, &bottomRight);
    return {static_cast<double>(topLeft.x), static_cast<double>(topLeft.y),
            static_cast<double>(bottomRight.x - topLeft.x), static_cast<double>(bottomRight.y - topLeft.y)};
}

void SetString(::VARIANT* result, std::string_view value)
{
    if (value.empty())
        return;
    result->vt = VT_BSTR;
    const std::wstring nativeValue = detail::Utf8ToWide(value);
    result->bstrVal = ::SysAllocStringLen(nativeValue.data(), static_cast<::UINT>(nativeValue.size()));
}

class RootProvider;
class SpreadsheetSelectionProvider;

class ElementProvider : public ::IRawElementProviderSimple, public ::IRawElementProviderFragment,
                        public ::IInvokeProvider, public ::IValueProvider,
                        public ::IExpandCollapseProvider, public ::ISelectionProvider,
                        public ::IGridProvider, public ::ITableProvider
{
public:
    ElementProvider(::HWND window, std::shared_ptr<core::Host> host, core::Control* control)
        : window_(window), host_(std::move(host)), control_(control) {}

    ::HRESULT STDMETHODCALLTYPE QueryInterface(REFIID identifier, void** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (identifier == IID_IUnknown || identifier == __uuidof(::IRawElementProviderSimple))
            *result = static_cast<::IRawElementProviderSimple*>(this);
        else if (identifier == __uuidof(::IRawElementProviderFragment))
            *result = static_cast<::IRawElementProviderFragment*>(this);
        else if (identifier == __uuidof(::IInvokeProvider))
            *result = static_cast<::IInvokeProvider*>(this);
        else if (identifier == __uuidof(::IValueProvider))
            *result = static_cast<::IValueProvider*>(this);
        else if (identifier == __uuidof(::IExpandCollapseProvider))
            *result = static_cast<::IExpandCollapseProvider*>(this);
        else if (identifier == __uuidof(::ISelectionProvider))
            *result = static_cast<::ISelectionProvider*>(this);
        else if (identifier == __uuidof(::IGridProvider))
            *result = static_cast<::IGridProvider*>(this);
        else if (identifier == __uuidof(::ITableProvider))
            *result = static_cast<::ITableProvider*>(this);
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
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        const core::DuiAccessibilityPattern patterns = control_->Accessibility().patterns;
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (pattern == UIA_InvokePatternId
            && core::HasAccessibilityPattern(patterns, core::DuiAccessibilityPattern::Invoke))
        {
            return QueryInterface(__uuidof(::IInvokeProvider), reinterpret_cast<void**>(result));
        }
        if (pattern == UIA_ValuePatternId
            && core::HasAccessibilityPattern(patterns, core::DuiAccessibilityPattern::Value))
        {
            return QueryInterface(__uuidof(::IValueProvider), reinterpret_cast<void**>(result));
        }
        if (pattern == UIA_ExpandCollapsePatternId
            && core::HasAccessibilityPattern(patterns,
                                             core::DuiAccessibilityPattern::ExpandCollapse))
        {
            return QueryInterface(__uuidof(::IExpandCollapseProvider),
                                  reinterpret_cast<void**>(result));
        }
        if (pattern == UIA_SelectionPatternId && SupportsSpreadsheetSelection(control_)
            && core::HasAccessibilityPattern(patterns, core::DuiAccessibilityPattern::Selection))
        {
            return QueryInterface(__uuidof(::ISelectionProvider), reinterpret_cast<void**>(result));
        }
        if (pattern == UIA_GridPatternId && SupportsSpreadsheetGridUia(control_)
            && core::HasAccessibilityPattern(patterns, core::DuiAccessibilityPattern::Grid))
            return QueryInterface(__uuidof(::IGridProvider), reinterpret_cast<void**>(result));
        if (pattern == UIA_TablePatternId && SupportsSpreadsheetTableUia(control_)
            && core::HasAccessibilityPattern(patterns, core::DuiAccessibilityPattern::Table))
            return QueryInterface(__uuidof(::ITableProvider), reinterpret_cast<void**>(result));
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE GetPropertyValue(::PROPERTYID property, ::VARIANT* result) override
    {
        if (!result)
            return E_POINTER;
        ::VariantInit(result);
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        const core::DuiAccessibilityData data = control_->Accessibility();
        switch (property) {
        case UIA_ControlTypePropertyId:
            result->vt = VT_I4;
            result->lVal = ToControlType(core::EffectiveAccessibilityRole(control_));
            return S_OK;
        case UIA_NamePropertyId: SetString(result, data.name); return S_OK;
        case UIA_HelpTextPropertyId: SetString(result, data.description); return S_OK;
        case UIA_ValueValuePropertyId: SetString(result, data.value); return S_OK;
        case UIA_AutomationIdPropertyId: SetString(result, data.identifier); return S_OK;
        case UIA_ClassNamePropertyId:
            result->vt = VT_BSTR;
            result->bstrVal = ::SysAllocString(L"ysDui.Control");
            return S_OK;
        case UIA_FrameworkIdPropertyId:
            result->vt = VT_BSTR;
            result->bstrVal = ::SysAllocString(L"ysDui");
            return S_OK;
        case UIA_IsEnabledPropertyId:
            result->vt = VT_BOOL;
            // Enabled() 已沿父链求值，禁用容器下的后代同样报告为禁用
            result->boolVal = control_ != nullptr && control_->Enabled() ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case UIA_IsOffscreenPropertyId:
            result->vt = VT_BOOL;
            result->boolVal = control_->EffectivelyVisible() ? VARIANT_FALSE : VARIANT_TRUE;
            return S_OK;
        case UIA_IsKeyboardFocusablePropertyId:
            result->vt = VT_BOOL;
            result->boolVal = data.keyboardFocusable ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case UIA_HasKeyboardFocusPropertyId:
            result->vt = VT_BOOL;
            result->boolVal = host_->FocusedControl() == control_ ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case UIA_ValueIsReadOnlyPropertyId:
            if (!core::HasAccessibilityPattern(data.patterns,
                                               core::DuiAccessibilityPattern::Value))
            {
                return S_OK;
            }
            result->vt = VT_BOOL;
            result->boolVal = data.readOnly ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case UIA_ExpandCollapseExpandCollapseStatePropertyId:
            if (!core::HasAccessibilityPattern(
                    data.patterns, core::DuiAccessibilityPattern::ExpandCollapse))
            {
                return S_OK;
            }
            result->vt = VT_I4;
            result->lVal = data.expanded ? ExpandCollapseState_Expanded
                                         : ExpandCollapseState_Collapsed;
            return S_OK;
        case UIA_NativeWindowHandlePropertyId:
            result->vt = VT_I4;
            result->lVal = static_cast<::LONG>(reinterpret_cast<std::uintptr_t>(window_));
            return S_OK;
        case UIA_BoundingRectanglePropertyId:
        {
            const UiaRect bounds = ScreenBounds(window_, control_);
            ::SAFEARRAY* values = ::SafeArrayCreateVector(VT_R8, 0, 4);
            if (!values)
                return E_OUTOFMEMORY;
            const double dimensions[] = {bounds.left, bounds.top, bounds.width, bounds.height};
            for (::LONG index = 0; index < 4; ++index)
            {
                const ::HRESULT status = ::SafeArrayPutElement(
                    values, &index, const_cast<double*>(&dimensions[index]));
                if (FAILED(status))
                {
                    ::SafeArrayDestroy(values);
                    return status;
                }
            }
            result->vt = VT_ARRAY | VT_R8;
            result->parray = values;
            return S_OK;
        }
        default: return S_OK;
        }
    }

    ::HRESULT STDMETHODCALLTYPE Invoke() override
    {
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (!core::HasAccessibilityPattern(control_->Accessibility().patterns,
                                           core::DuiAccessibilityPattern::Invoke))
        {
            return UIA_E_NOTSUPPORTED;
        }
        if (!IsEffectivelyEnabled(control_))
            return UIA_E_ELEMENTNOTENABLED;
        return control_->PerformAccessibilityAction(core::DuiAccessibilityAction::Invoke)
            ? S_OK : UIA_E_INVALIDOPERATION;
    }

    ::HRESULT STDMETHODCALLTYPE SetValue(::LPCWSTR value) override
    {
        if (!value)
            return E_INVALIDARG;
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        const core::DuiAccessibilityData data = control_->Accessibility();
        if (!core::HasAccessibilityPattern(data.patterns, core::DuiAccessibilityPattern::Value))
            return UIA_E_NOTSUPPORTED;
        if (!IsEffectivelyEnabled(control_))
            return UIA_E_ELEMENTNOTENABLED;
        if (data.readOnly)
            return UIA_E_INVALIDOPERATION;
        const std::wstring_view nativeValue(value);
        const std::string utf8Value = detail::WideToUtf8(nativeValue);
        if (!nativeValue.empty() && utf8Value.empty())
            return E_INVALIDARG;
        return control_->PerformAccessibilityAction(core::DuiAccessibilityAction::SetValue,
                                                    utf8Value)
            ? S_OK : UIA_E_INVALIDOPERATION;
    }

    ::HRESULT STDMETHODCALLTYPE get_Value(::BSTR* result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        const core::DuiAccessibilityData data = control_->Accessibility();
        if (!core::HasAccessibilityPattern(data.patterns, core::DuiAccessibilityPattern::Value))
            return UIA_E_NOTSUPPORTED;
        const std::wstring value = detail::Utf8ToWide(data.value);
        *result = ::SysAllocStringLen(value.data(), static_cast<::UINT>(value.size()));
        return *result || value.empty() ? S_OK : E_OUTOFMEMORY;
    }

    ::HRESULT STDMETHODCALLTYPE get_IsReadOnly(::BOOL* result) override
    {
        if (!result)
            return E_POINTER;
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        const core::DuiAccessibilityData data = control_->Accessibility();
        if (!core::HasAccessibilityPattern(data.patterns, core::DuiAccessibilityPattern::Value))
            return UIA_E_NOTSUPPORTED;
        *result = data.readOnly ? TRUE : FALSE;
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE Expand() override
    {
        return SetExpandedState(true);
    }

    ::HRESULT STDMETHODCALLTYPE Collapse() override
    {
        return SetExpandedState(false);
    }

    ::HRESULT STDMETHODCALLTYPE get_ExpandCollapseState(::ExpandCollapseState* result) override
    {
        if (!result)
            return E_POINTER;
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        const core::DuiAccessibilityData data = control_->Accessibility();
        if (!core::HasAccessibilityPattern(data.patterns,
                                           core::DuiAccessibilityPattern::ExpandCollapse))
        {
            return UIA_E_NOTSUPPORTED;
        }
        *result = data.expanded ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed;
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(::IRawElementProviderSimple** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE Navigate(::NavigateDirection direction, ::IRawElementProviderFragment** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        core::Control* next{};
        switch (direction) {
        case NavigateDirection_Parent: next = core::AccessibilityParent(control_); break;
        case NavigateDirection_FirstChild: next = core::AccessibilityFirstChild(control_); break;
        case NavigateDirection_LastChild: next = core::AccessibilityLastChild(control_); break;
        case NavigateDirection_NextSibling: next = core::AccessibilityNextSibling(control_); break;
        case NavigateDirection_PreviousSibling: next = core::AccessibilityPreviousSibling(control_); break;
        default: return S_OK;
        }
        if (!next)
            return S_OK;
        auto* provider = new ElementProvider(window_, host_, next);
        const ::HRESULT status = provider->QueryInterface(__uuidof(::IRawElementProviderFragment),
                                                           reinterpret_cast<void**>(result));
        provider->Release();
        return status;
    }

    ::HRESULT STDMETHODCALLTYPE GetRuntimeId(::SAFEARRAY** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        ::SAFEARRAY* runtimeId = ::SafeArrayCreateVector(VT_I4, 0, 3);
        if (!runtimeId)
            return E_OUTOFMEMORY;
        const ::LONG values[] = {UiaAppendRuntimeId,
                                  static_cast<::LONG>(reinterpret_cast<std::uintptr_t>(window_)),
                                  static_cast<::LONG>(reinterpret_cast<std::uintptr_t>(control_))};
        for (::LONG index = 0; index < 3; ++index)
        {
            const ::HRESULT status = ::SafeArrayPutElement(
                runtimeId, &index, const_cast<::LONG*>(&values[index]));
            if (FAILED(status))
            {
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
        *result = ScreenBounds(window_, control_);
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
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        ::SetFocus(window_);
        host_->SetFocusedControl(control_);
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE get_FragmentRoot(::IRawElementProviderFragmentRoot** result) override;

    ::HRESULT STDMETHODCALLTYPE GetSelection(::SAFEARRAY** result) override;
    ::HRESULT STDMETHODCALLTYPE get_CanSelectMultiple(::BOOL* result) override;
    ::HRESULT STDMETHODCALLTYPE get_IsSelectionRequired(::BOOL* result) override;
    ::HRESULT STDMETHODCALLTYPE GetItem(int row, int column,
                                        ::IRawElementProviderSimple** result) override;
    ::HRESULT STDMETHODCALLTYPE get_RowCount(int* result) override;
    ::HRESULT STDMETHODCALLTYPE get_ColumnCount(int* result) override;
    ::HRESULT STDMETHODCALLTYPE GetRowHeaders(::SAFEARRAY** result) override;
    ::HRESULT STDMETHODCALLTYPE GetColumnHeaders(::SAFEARRAY** result) override;
    ::HRESULT STDMETHODCALLTYPE get_RowOrColumnMajor(::RowOrColumnMajor* result) override;

protected:
    ::HWND window_{};
    std::shared_ptr<core::Host> host_;
    core::Control* control_{};

private:
    ::HRESULT SetExpandedState(bool expanded)
    {
        if (!IsAlive(host_, control_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (!core::HasAccessibilityPattern(control_->Accessibility().patterns,
                                           core::DuiAccessibilityPattern::ExpandCollapse))
        {
            return UIA_E_NOTSUPPORTED;
        }
        if (!IsEffectivelyEnabled(control_))
            return UIA_E_ELEMENTNOTENABLED;
        const core::DuiAccessibilityAction action = expanded
            ? core::DuiAccessibilityAction::Expand : core::DuiAccessibilityAction::Collapse;
        return control_->PerformAccessibilityAction(action) ? S_OK : UIA_E_INVALIDOPERATION;
    }

    std::atomic<::ULONG> references_{1};
};

// The range is a single virtual selection item so a large rectangular selection
// never expands into one native provider per logical spreadsheet cell.
class SpreadsheetSelectionProvider final : public ::IRawElementProviderSimple,
                                           public ::IRawElementProviderFragment,
                                           public ::ISelectionItemProvider
{
public:
    SpreadsheetSelectionProvider(::HWND window, std::shared_ptr<core::Host> host,
                                 core::Control* control)
        : window_(window), host_(std::move(host)), control_(control) {}

    ::HRESULT STDMETHODCALLTYPE QueryInterface(REFIID identifier, void** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (identifier == IID_IUnknown || identifier == __uuidof(::IRawElementProviderSimple))
            *result = static_cast<::IRawElementProviderSimple*>(this);
        else if (identifier == __uuidof(::IRawElementProviderFragment))
            *result = static_cast<::IRawElementProviderFragment*>(this);
        else if (identifier == __uuidof(::ISelectionItemProvider))
            *result = static_cast<::ISelectionItemProvider*>(this);
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
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (pattern != UIA_SelectionItemPatternId)
            return S_OK;
        return QueryInterface(__uuidof(::ISelectionItemProvider), reinterpret_cast<void**>(result));
    }

    ::HRESULT STDMETHODCALLTYPE GetPropertyValue(::PROPERTYID property, ::VARIANT* result) override
    {
        if (!result)
            return E_POINTER;
        ::VariantInit(result);
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        const core::DuiAccessibilityData data = control_->Accessibility();
        switch (property)
        {
        case UIA_ControlTypePropertyId:
            result->vt = VT_I4;
            result->lVal = UIA_DataItemControlTypeId;
            return S_OK;
        case UIA_NamePropertyId:
            SetString(result, data.description);
            return S_OK;
        case UIA_HelpTextPropertyId:
            SetString(result, data.description);
            return S_OK;
        case UIA_ClassNamePropertyId:
            result->vt = VT_BSTR;
            result->bstrVal = ::SysAllocString(L"ysDui.SpreadsheetSelection");
            return S_OK;
        case UIA_FrameworkIdPropertyId:
            result->vt = VT_BSTR;
            result->bstrVal = ::SysAllocString(L"ysDui");
            return S_OK;
        case UIA_IsEnabledPropertyId:
            result->vt = VT_BOOL;
            // Enabled() 已沿父链求值，禁用容器下的后代同样报告为禁用
            result->boolVal = control_ != nullptr && control_->Enabled() ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case UIA_IsOffscreenPropertyId:
            result->vt = VT_BOOL;
            result->boolVal = control_->EffectivelyVisible() ? VARIANT_FALSE : VARIANT_TRUE;
            return S_OK;
        case UIA_SelectionItemIsSelectedPropertyId:
            result->vt = VT_BOOL;
            result->boolVal = VARIANT_TRUE;
            return S_OK;
        case UIA_BoundingRectanglePropertyId:
        {
            const UiaRect bounds = ScreenBounds(window_, control_);
            ::SAFEARRAY* values = ::SafeArrayCreateVector(VT_R8, 0, 4);
            if (!values)
                return E_OUTOFMEMORY;
            const double dimensions[] = {bounds.left, bounds.top, bounds.width, bounds.height};
            for (::LONG index = 0; index < 4; ++index)
            {
                const ::HRESULT status = ::SafeArrayPutElement(
                    values, &index, const_cast<double*>(&dimensions[index]));
                if (FAILED(status))
                {
                    ::SafeArrayDestroy(values);
                    return status;
                }
            }
            result->vt = VT_ARRAY | VT_R8;
            result->parray = values;
            return S_OK;
        }
        default:
            return S_OK;
        }
    }

    ::HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(::IRawElementProviderSimple** result) override
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
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (direction != NavigateDirection_Parent)
            return S_OK;
        auto* provider = new ElementProvider(window_, host_, control_);
        const ::HRESULT status = provider->QueryInterface(__uuidof(::IRawElementProviderFragment),
                                                           reinterpret_cast<void**>(result));
        provider->Release();
        return status;
    }

    ::HRESULT STDMETHODCALLTYPE GetRuntimeId(::SAFEARRAY** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        ::SAFEARRAY* runtimeId = ::SafeArrayCreateVector(VT_I4, 0, 4);
        if (!runtimeId)
            return E_OUTOFMEMORY;
        const ::LONG values[] = {UiaAppendRuntimeId,
                                  static_cast<::LONG>(reinterpret_cast<std::uintptr_t>(window_)),
                                  static_cast<::LONG>(reinterpret_cast<std::uintptr_t>(control_)),
                                  0x5352};
        for (::LONG index = 0; index < 4; ++index)
        {
            const ::HRESULT status = ::SafeArrayPutElement(
                runtimeId, &index, const_cast<::LONG*>(&values[index]));
            if (FAILED(status))
            {
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
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        *result = ScreenBounds(window_, control_);
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
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        ::SetFocus(window_);
        host_->SetFocusedControl(control_);
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE get_FragmentRoot(::IRawElementProviderFragmentRoot** result) override;

    ::HRESULT STDMETHODCALLTYPE Select() override
    {
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        return IsEffectivelyEnabled(control_) ? S_OK : UIA_E_ELEMENTNOTENABLED;
    }

    ::HRESULT STDMETHODCALLTYPE AddToSelection() override
    {
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (!IsEffectivelyEnabled(control_))
            return UIA_E_ELEMENTNOTENABLED;
        return UIA_E_INVALIDOPERATION;
    }

    ::HRESULT STDMETHODCALLTYPE RemoveFromSelection() override
    {
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (!IsEffectivelyEnabled(control_))
            return UIA_E_ELEMENTNOTENABLED;
        return UIA_E_INVALIDOPERATION;
    }

    ::HRESULT STDMETHODCALLTYPE get_IsSelected(::BOOL* result) override
    {
        if (!result)
            return E_POINTER;
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        *result = TRUE;
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE get_SelectionContainer(::IRawElementProviderSimple** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (!IsSelectionAlive())
            return UIA_E_ELEMENTNOTAVAILABLE;
        auto* provider = new ElementProvider(window_, host_, control_);
        const ::HRESULT status = provider->QueryInterface(__uuidof(::IRawElementProviderSimple),
                                                           reinterpret_cast<void**>(result));
        provider->Release();
        return status;
    }

private:
    [[nodiscard]] bool IsSelectionAlive() const
    {
        return IsAlive(host_, control_)
            && SupportsSpreadsheetSelection(control_);
    }

    std::atomic<::ULONG> references_{1};
    ::HWND window_{};
    std::shared_ptr<core::Host> host_;
    core::Control* control_{};
};

class RootProvider final : public ::IRawElementProviderSimple, public ::IRawElementProviderFragment,
                           public ::IRawElementProviderFragmentRoot
{
public:
    RootProvider(::HWND window, std::shared_ptr<core::Host> host)
        : window_(window), host_(std::move(host))
    {
        if (host_ && host_->Root())
            root_ = new ElementProvider(window_, host_, host_->Root());
    }

    ~RootProvider()
    {
        if (root_)
            root_->Release();
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
        else if (identifier == __uuidof(::IRawElementProviderFragmentRoot))
            *result = static_cast<::IRawElementProviderFragmentRoot*>(this);
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

    ::HRESULT STDMETHODCALLTYPE get_ProviderOptions(::ProviderOptions* result) override { return root_ ? root_->get_ProviderOptions(result) : E_FAIL; }
    ::HRESULT STDMETHODCALLTYPE GetPatternProvider(::PATTERNID pattern, ::IUnknown** result) override { return root_ ? root_->GetPatternProvider(pattern, result) : E_FAIL; }
    ::HRESULT STDMETHODCALLTYPE GetPropertyValue(::PROPERTYID property, ::VARIANT* result) override { return root_ ? root_->GetPropertyValue(property, result) : E_FAIL; }
    ::HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(::IRawElementProviderSimple** result) override { return root_ ? root_->get_HostRawElementProvider(result) : E_FAIL; }
    ::HRESULT STDMETHODCALLTYPE Navigate(::NavigateDirection direction, ::IRawElementProviderFragment** result) override { return root_ ? root_->Navigate(direction, result) : E_FAIL; }
    ::HRESULT STDMETHODCALLTYPE GetRuntimeId(::SAFEARRAY** result) override { return root_ ? root_->GetRuntimeId(result) : E_FAIL; }
    ::HRESULT STDMETHODCALLTYPE get_BoundingRectangle(::UiaRect* result) override { return root_ ? root_->get_BoundingRectangle(result) : E_FAIL; }
    ::HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(::SAFEARRAY** result) override { return root_ ? root_->GetEmbeddedFragmentRoots(result) : E_FAIL; }
    ::HRESULT STDMETHODCALLTYPE SetFocus() override { return root_ ? root_->SetFocus() : E_FAIL; }

    ::HRESULT STDMETHODCALLTYPE get_FragmentRoot(::IRawElementProviderFragmentRoot** result) override
    {
        return QueryInterface(__uuidof(::IRawElementProviderFragmentRoot), reinterpret_cast<void**>(result));
    }

    ::HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x, double y, ::IRawElementProviderFragment** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        ::POINT point{static_cast<::LONG>(x), static_cast<::LONG>(y)};
        ::ScreenToClient(window_, &point);
        const core::Point logicalPoint = core::DuiDpiScale(
            static_cast<int>(::GetDpiForWindow(window_))).Unscale(
                core::Point{point.x, point.y});
        core::Control* target = core::NearestAccessibleAncestor(
            host_ ? host_->HitTest(logicalPoint) : nullptr);
        if (!target)
            return S_OK;
        auto* provider = new ElementProvider(window_, host_, target);
        const ::HRESULT status = provider->QueryInterface(__uuidof(::IRawElementProviderFragment), reinterpret_cast<void**>(result));
        provider->Release();
        return status;
    }

    ::HRESULT STDMETHODCALLTYPE GetFocus(::IRawElementProviderFragment** result) override
    {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        core::Control* target = core::NearestAccessibleAncestor(host_ ? host_->FocusedControl() : nullptr);
        if (!target)
            return S_OK;
        auto* provider = new ElementProvider(window_, host_, target);
        const ::HRESULT status = provider->QueryInterface(__uuidof(::IRawElementProviderFragment), reinterpret_cast<void**>(result));
        provider->Release();
        return status;
    }

private:
    std::atomic<::ULONG> references_{1};
    ::HWND window_{};
    std::shared_ptr<core::Host> host_;
    ElementProvider* root_{};
};

::HRESULT ElementProvider::get_FragmentRoot(::IRawElementProviderFragmentRoot** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    auto* provider = new RootProvider(window_, host_);
    const ::HRESULT status = provider->QueryInterface(__uuidof(::IRawElementProviderFragmentRoot),
                                                       reinterpret_cast<void**>(result));
    provider->Release();
    return status;
}

::HRESULT ElementProvider::GetItem(int row, int column,
                                   ::IRawElementProviderSimple** result)
{
    return GetSpreadsheetGridItem(window_, host_, control_, row, column, result);
}

::HRESULT ElementProvider::get_RowCount(int* result)
{
    return GetSpreadsheetGridRowCount(host_, control_, result);
}

::HRESULT ElementProvider::get_ColumnCount(int* result)
{
    return GetSpreadsheetGridColumnCount(host_, control_, result);
}

::HRESULT ElementProvider::GetRowHeaders(::SAFEARRAY** result)
{
    return GetSpreadsheetTableHeaders(host_, control_, result);
}

::HRESULT ElementProvider::GetColumnHeaders(::SAFEARRAY** result)
{
    return GetRowHeaders(result);
}

::HRESULT ElementProvider::get_RowOrColumnMajor(::RowOrColumnMajor* result)
{
    return GetSpreadsheetTableMajor(host_, control_, result);
}

::HRESULT ElementProvider::GetSelection(::SAFEARRAY** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!IsAlive(host_, control_))
        return UIA_E_ELEMENTNOTAVAILABLE;
    if (!SupportsSpreadsheetSelection(control_))
    {
        return UIA_E_NOTSUPPORTED;
    }
    ::SAFEARRAY* selected = ::SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
    if (!selected)
        return E_OUTOFMEMORY;
    auto* provider = new SpreadsheetSelectionProvider(window_, host_, control_);
    ::LONG index{};
    const ::HRESULT status = ::SafeArrayPutElement(
        selected, &index, static_cast<::IRawElementProviderSimple*>(provider));
    provider->Release();
    if (FAILED(status))
    {
        ::SafeArrayDestroy(selected);
        return status;
    }
    *result = selected;
    return S_OK;
}

::HRESULT ElementProvider::get_CanSelectMultiple(::BOOL* result)
{
    if (!result)
        return E_POINTER;
    if (!IsAlive(host_, control_))
        return UIA_E_ELEMENTNOTAVAILABLE;
    if (!SupportsSpreadsheetSelection(control_))
    {
        return UIA_E_NOTSUPPORTED;
    }
    *result = FALSE;
    return S_OK;
}

::HRESULT ElementProvider::get_IsSelectionRequired(::BOOL* result)
{
    if (!result)
        return E_POINTER;
    if (!IsAlive(host_, control_))
        return UIA_E_ELEMENTNOTAVAILABLE;
    if (!SupportsSpreadsheetSelection(control_))
    {
        return UIA_E_NOTSUPPORTED;
    }
    *result = TRUE;
    return S_OK;
}

::HRESULT SpreadsheetSelectionProvider::get_FragmentRoot(::IRawElementProviderFragmentRoot** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!IsSelectionAlive())
        return UIA_E_ELEMENTNOTAVAILABLE;
    auto* provider = new RootProvider(window_, host_);
    const ::HRESULT status = provider->QueryInterface(__uuidof(::IRawElementProviderFragmentRoot),
                                                       reinterpret_cast<void**>(result));
    provider->Release();
    return status;
}

} // namespace

::IRawElementProviderSimple* CreateUiaElementProvider(
    ::HWND window, const std::shared_ptr<core::Host>& host, core::Control* control)
{
    if (!window || !host || !Contains(host->Root(), control))
        return nullptr;
    return new ElementProvider(window, host, control);
}

::LRESULT ReturnUiaProvider(::HWND window, const std::shared_ptr<core::Host>& host, ::WPARAM word, ::LPARAM data)
{
    if (data != static_cast<::LPARAM>(UiaRootObjectId) || !host)
        return 0;
    auto* provider = new RootProvider(window, host);
    const ::LRESULT result = ::UiaReturnRawElementProvider(window, word, data, provider);
    provider->Release();
    return result;
}

} // namespace ysDui::platform::win32
