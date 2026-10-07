/**
 * 文件名：DuiWin32SpreadsheetUiaProvider.hpp
 * 开发者：青蓝
 * 开发时间：2026-08-02
 * 用途：声明按需创建工作表虚拟单元格 UI Automation Provider 的内部工厂。
 */
#pragma once

#include <memory>

#define NOMINMAX
#include <windows.h>
#include <UIAutomation.h>

#include "ysDui/controls/list/DuiWorkbookModel.hpp"

struct IRawElementProviderSimple;

namespace ysDui::core {
class Control;
class Host;
}

namespace ysDui::controls::list {
class DuiSpreadsheet;
}

namespace ysDui::platform::win32 {

[[nodiscard]] IRawElementProviderSimple* CreateSpreadsheetCellUiaProvider(
    HWND window, std::shared_ptr<core::Host> host,
    controls::list::DuiSpreadsheet* spreadsheet,
    controls::list::IDuiWorkbookModel* model,
    controls::list::DuiWorksheetId sheet,
    controls::list::DuiCellAddress cell);
[[nodiscard]] bool SupportsSpreadsheetGridUia(const core::Control* control);
[[nodiscard]] bool SupportsSpreadsheetTableUia(const core::Control* control);
HRESULT GetSpreadsheetGridItem(HWND window, const std::shared_ptr<core::Host>& host,
                               core::Control* control, int row, int column,
                               IRawElementProviderSimple** result);
HRESULT GetSpreadsheetGridRowCount(const std::shared_ptr<core::Host>& host,
                                   core::Control* control, int* result);
HRESULT GetSpreadsheetGridColumnCount(const std::shared_ptr<core::Host>& host,
                                      core::Control* control, int* result);
HRESULT GetSpreadsheetTableHeaders(const std::shared_ptr<core::Host>& host,
                                   core::Control* control, SAFEARRAY** result);
HRESULT GetSpreadsheetTableMajor(const std::shared_ptr<core::Host>& host,
                                 core::Control* control, RowOrColumnMajor* result);

} // namespace ysDui::platform::win32
