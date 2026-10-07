/**
 * 文件名：DuiSpreadsheet.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-31
 * 用途：声明支持虚拟化二维工作表编辑的控件。
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/controls/list/DuiWorkbookModel.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/ui/DuiClipboard.hpp"
#include "ysDui/ui/DuiHostRef.hpp"
#include "ysDui/ui/DuiTextInput.hpp"

namespace ysDui::render {
class DuiTextMeasurer;
}

namespace ysDui::ui {
class IUiHostFactory;
}

namespace ysDui::controls::list {

/** \brief Spreadsheet 写入被模型拒绝时提供给宿主的诊断信息。 */
enum class DuiSpreadsheetWriteOperation
{
    CellText,
    RowHeight,
    ColumnWidth,
};

struct DuiSpreadsheetWriteError final
{
    DuiWorksheetId worksheet{};
    DuiCellAddress cell;
    DuiWorkbookWriteResult result;
    DuiSpreadsheetWriteOperation operation{DuiSpreadsheetWriteOperation::CellText};
};

class DuiSpreadsheet final : public core::Control, public render::DuiRenderable
{
public:
    DuiSpreadsheet();
    ~DuiSpreadsheet() override;
    DuiSpreadsheet(const DuiSpreadsheet&) = delete;
    DuiSpreadsheet& operator=(const DuiSpreadsheet&) = delete;
    DuiSpreadsheet(DuiSpreadsheet&&) = delete;
    DuiSpreadsheet& operator=(DuiSpreadsheet&&) = delete;

    void SetWorkbookModel(IDuiWorkbookModel* model);
    [[nodiscard]] IDuiWorkbookModel* WorkbookModel() const;
    void ReloadFromModel();
    void SetTextInput(ui::DuiTextInput* textInput);
    void SetClipboard(ui::DuiClipboard* clipboard);
    /** \brief 设置右键菜单所需的平台无关宿主工厂与所有者引用。 */
    void SetPopupContext(ui::IUiHostFactory& factory, ui::HostRef owner);
    /** \brief 设置自动尺寸兼容计算使用的文本测量器；对象由宿主持有。 */
    void SetTextMeasurer(render::DuiTextMeasurer* measurer);

    [[nodiscard]] DuiWorksheetId ActiveWorksheet() const;
    bool SetActiveWorksheet(DuiWorksheetId sheet);
    [[nodiscard]] DuiWorksheetId AddWorksheet(std::string name = {});
    bool RenameActiveWorksheet(std::string name);
    bool RemoveActiveWorksheet();
    [[nodiscard]] DuiCellAddress ActiveCell() const;
    void SetActiveCell(DuiCellAddress cell, bool extendSelection = false);
    [[nodiscard]] DuiCellRange Selection() const;
    void SetSelection(DuiCellRange range);
    /** 返回单元格在当前视口内的可见区域；越界或完全不可见时返回空。 */
    [[nodiscard]] std::optional<core::Rect> VisibleCellBounds(DuiCellAddress cell) const;
    /** 返回当前固定在视口顶部的逻辑行数。 */
    [[nodiscard]] int FrozenRowCount() const;
    /** 返回当前固定在视口左侧的逻辑列数。 */
    [[nodiscard]] int FrozenColumnCount() const;
    /** 设置冻结窗格之前的行数和列数。 */
    void SetFrozenPanes(int rows, int columns);
    /** 清除当前工作表的冻结窗格。 */
    void ClearFrozenPanes();
    [[nodiscard]] bool Editing() const;
    bool BeginEdit();
    void CommitEdit();
    void CancelEdit();
    void Undo();
    void Redo();

    /** \brief 查找下一个匹配单元格；成功命中时同步活动单元格和视口。 */
    [[nodiscard]] DuiWorkbookFindResult FindNext(
        std::string_view query, DuiWorkbookFindOptions options = {},
        std::optional<DuiCellRange> range = std::nullopt);
    /** \brief 委派模型替换文本，replaceAll 为 false 时只替换首个匹配项。 */
    [[nodiscard]] DuiWorkbookReplaceResult ReplaceText(
        std::string_view query, std::string_view replacement, bool replaceAll,
        DuiWorkbookFindOptions options = {}, std::optional<DuiCellRange> range = std::nullopt);
    /** \brief 委派模型在指定区域内排序；空关键字数组表示清除排序。 */
    [[nodiscard]] DuiWorkbookOperationResult ApplySort(
        std::vector<DuiWorkbookSortKey> keys, std::optional<DuiCellRange> range = std::nullopt);
    /** \brief 设置当前 Sheet 的列筛选条件。 */
    [[nodiscard]] DuiWorkbookOperationResult SetFilters(std::vector<DuiWorkbookFilter> filters);
    /** \brief 清除当前 Sheet 的全部筛选条件。 */
    [[nodiscard]] DuiWorkbookOperationResult ClearFilters();
    /** \brief 自动调整一个逻辑行的高度。 */
    [[nodiscard]] DuiWorkbookOperationResult AutoFitRow(int row);
    /** \brief 自动调整一个逻辑列的宽度。 */
    [[nodiscard]] DuiWorkbookOperationResult AutoFitColumn(int column);
    /** \brief 返回控件当前显示的排序标记。 */
    [[nodiscard]] std::vector<DuiWorkbookSortKey> SortKeys() const;
    /** \brief 返回控件当前显示的筛选标记。 */
    [[nodiscard]] std::vector<DuiWorkbookFilter> Filters() const;

    void SetActiveCellChangedHandler(std::function<void(DuiCellAddress)> handler);
    void SetSelectionChangedHandler(std::function<void(DuiCellRange)> handler);
    void SetWorksheetChangedHandler(std::function<void(DuiWorksheetId)> handler);
    /** \brief 设置 Ctrl+F 和右键菜单“查找”触发的宿主回调。 */
    void SetFindRequestedHandler(std::function<void()> handler);
    /** \brief 设置 Ctrl+H 和右键菜单“替换”触发的宿主回调。 */
    void SetReplaceRequestedHandler(std::function<void()> handler);
    /** \brief 设置右键菜单或公共数据操作失败时的结构化诊断回调。 */
    void SetWorkbookOperationErrorHandler(
        std::function<void(const DuiWorkbookOperationResult&)> handler);
    /**
     * \brief 设置模型拒绝单元格写入时的 UI 线程诊断回调。
     * \param handler 接收工作表、单元格和结构化失败结果；传入空函数可解除回调。
     */
    void SetWorkbookWriteErrorHandler(std::function<void(const DuiSpreadsheetWriteError&)> handler);
    void Layout(core::Rect bounds);
    [[nodiscard]] core::Control* HitTest(core::Point point) override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;
    bool PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                    std::string_view value) override;

private:
    class Impl;
    void EnsureVisible(DuiCellAddress cell);
    bool SetSelectionCursor(DuiCellAddress cell);
    void PlaceTextInput();
    void RequestVisibleCellRanges();
    [[nodiscard]] static int ColumnFromPoint(const Impl& data, core::Point point);
    [[nodiscard]] static int RowFromPoint(const Impl& data, core::Point point);
    [[nodiscard]] static int ColumnResizeIndex(const Impl& data, core::Point point);
    [[nodiscard]] static int RowResizeIndex(const Impl& data, core::Point point);
    [[nodiscard]] static std::optional<bool> WorksheetNameExists(
        Impl& data, IDuiWorkbookModel* expectedModel, DuiWorksheetId expectedSheet,
        std::string_view name, DuiWorksheetId excludedSheet = 0);
    [[nodiscard]] static std::optional<std::string> UniqueWorksheetName(
        Impl& data, IDuiWorkbookModel* expectedModel, DuiWorksheetId expectedSheet,
        std::string baseName, int firstSuffix = 0);
    [[nodiscard]] static bool SelectionBorderContains(const Impl& data, core::Point point);
    [[nodiscard]] static bool FillHandleContains(const Impl& data, core::Point point);
    bool BeginSelectionFill();
    bool UpdateSelectionFill(core::Point point);
    bool EndSelectionFill();
    bool CommitSelectionFill();
    bool CommitSelectionMove(DuiCellRange target);
    void ApplyHistory(bool undoOperation);
    bool OnKeyboardEvent(const core::Event& event);
    bool ShowContextMenu(core::Point point);
    bool ShowSheetContextMenu(DuiWorksheetId sheet, core::Rect anchor);
    void InvokeContextMenuCommand(std::uint32_t command);
    void InvokeSheetContextMenuCommand(std::uint32_t command);
    bool BeginWorksheetRename(DuiWorksheetId sheet, core::Rect bounds);
    void CommitWorksheetRename();
    void CancelWorksheetRename();
    std::unique_ptr<Impl> spreadsheet_;
};

} // namespace ysDui::controls::list
