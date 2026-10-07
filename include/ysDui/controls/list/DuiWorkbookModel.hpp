/**
 * 文件名：DuiWorkbookModel.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-31
 * 用途：声明平台无关的工作簿数据模型契约。
 */
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ysDui/controls/list/DuiWorkbookOperations.hpp"
#include "ysDui/core/DuiSubscription.hpp"

namespace ysDui::controls::list {

using DuiWorksheetId = std::uint64_t;

/** \brief 单元格展示值的语义类型。 */
enum class DuiCellValueKind
{
    Text,
    Number,
    Boolean,
    DateTime,
    Error,
    Custom,
};

/** \brief 单元格原始文本经过模型格式化后的展示信息。 */
struct DuiCellPresentation final
{
    std::string displayText;
    DuiCellValueKind kind{DuiCellValueKind::Text};
    std::string format;
};

/** \brief 工作簿模型拒绝写入的类别。 */
enum class DuiWorkbookWriteErrorCode
{
    None,               // 写入成功。
    Rejected,           // 模型未提供更具体的拒绝原因。
    ReadOnly,           // 目标单元格或工作表不可编辑。
    InvalidWorksheet,   // 工作表标识无效。
    InvalidCell,        // 单元格坐标无效。
    ValidationFailed,   // 文本未通过业务校验。
};

/** \brief 工作簿模型单元格写入结果，message 必须是 UTF-8 文本。 */
struct DuiWorkbookWriteResult final
{
    DuiWorkbookWriteErrorCode code{DuiWorkbookWriteErrorCode::None};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return code == DuiWorkbookWriteErrorCode::None;
    }
};

/** \brief 单个行高或列宽覆盖项，index 使用 0 起始逻辑索引。 */
struct DuiAxisSizeOverride final
{
    int index{};
    int pixels{};
};

/** \brief 行高或列宽的稀疏快照。 */
struct DuiSparseAxisSizes final
{
    int defaultPixels{};
    std::vector<DuiAxisSizeOverride> overrides;
};

/** 由宿主拥有的工作簿模型。所有方法均在 UI 线程调用。 */
class IDuiWorkbookModel
{
public:
    virtual ~IDuiWorkbookModel() = default;

    [[nodiscard]] virtual int WorksheetCount() const = 0;
    [[nodiscard]] virtual DuiWorksheetId WorksheetIdAt(int index) const = 0;
    [[nodiscard]] virtual std::string WorksheetName(DuiWorksheetId sheet) const = 0;
    virtual bool RenameWorksheet(DuiWorksheetId sheet, std::string name) = 0;
    [[nodiscard]] virtual DuiWorksheetId AddWorksheet(std::string name) = 0;
    virtual bool RemoveWorksheet(DuiWorksheetId sheet) = 0;

    [[nodiscard]] virtual int RowCount(DuiWorksheetId sheet) const = 0;
    [[nodiscard]] virtual int ColumnCount(DuiWorksheetId sheet) const = 0;
    /**
     * \brief 请求宿主模型准备指定工作表范围内的单元格数据。
     * \param sheet 目标工作表标识。
     * \param ranges 已裁剪并规范化的闭区间；控件不持有模型返回的数据。
     *
     * 默认空实现保持既有模型源兼容。异步实现应复制 ranges，并在数据准备完成后通过
     * 既有 UI 线程模型通知触发刷新。
     */
    virtual void RequestCellRanges(DuiWorksheetId sheet, const std::vector<DuiCellRange>& ranges)
    {
        (void)sheet;
        (void)ranges;
    }
    [[nodiscard]] virtual std::string CellText(DuiWorksheetId sheet, DuiCellAddress cell) const = 0;
    /** \brief 返回模型原生支持的数据操作能力。 */
    [[nodiscard]] virtual DuiWorkbookCapability Capabilities() const noexcept
    {
        return DuiWorkbookCapability::None;
    }
    /**
     * \brief 返回用于绘制和辅助功能的格式化展示信息。
     * \return 默认以 CellText() 作为文本展示，保持既有模型实现的源兼容。
     */
    [[nodiscard]] virtual DuiCellPresentation CellPresentation(DuiWorksheetId sheet, DuiCellAddress cell) const
    {
        return {CellText(sheet, cell), DuiCellValueKind::Text, {}};
    }
    /**
     * \brief 查询单元格是否只读。
     * \return 默认返回 false，保持现有模型的可编辑行为不变。
     */
    [[nodiscard]] virtual bool IsCellReadOnly(DuiWorksheetId sheet, DuiCellAddress cell) const
    {
        (void)sheet;
        (void)cell;
        return false;
    }
    virtual bool SetCellText(DuiWorksheetId sheet, DuiCellAddress cell, std::string text) = 0;
    /**
     * \brief 写入单元格文本并返回失败原因。
     * \return 默认实现适配既有 SetCellText()，已有模型无需修改即可保持行为。
     */
    [[nodiscard]] virtual DuiWorkbookWriteResult SetCellTextWithResult(
        DuiWorksheetId sheet, DuiCellAddress cell, std::string text)
    {
        if (SetCellText(sheet, cell, std::move(text)))
            return {};
        return {DuiWorkbookWriteErrorCode::Rejected, "Workbook model rejected the cell text update"};
    }
    [[nodiscard]] virtual int RowHeight(DuiWorksheetId sheet, int row) const = 0;
    [[nodiscard]] virtual int ColumnWidth(DuiWorksheetId sheet, int column) const = 0;
    /**
     * \brief 返回行高稀疏快照；越界项会被忽略，重复索引以后出现的值为准。
     * \return std::nullopt 表示控件应继续逐项调用 RowHeight()。
     */
    [[nodiscard]] virtual std::optional<DuiSparseAxisSizes> SparseRowSizes(DuiWorksheetId sheet) const
    {
        (void)sheet;
        return std::nullopt;
    }
    /**
     * \brief 返回列宽稀疏快照；越界项会被忽略，重复索引以后出现的值为准。
     * \return std::nullopt 表示控件应继续逐项调用 ColumnWidth()。
     */
    [[nodiscard]] virtual std::optional<DuiSparseAxisSizes> SparseColumnSizes(DuiWorksheetId sheet) const
    {
        (void)sheet;
        return std::nullopt;
    }
    virtual bool SetRowHeight(DuiWorksheetId sheet, int row, int pixels) = 0;
    virtual bool SetColumnWidth(DuiWorksheetId sheet, int column, int pixels) = 0;
    /**
     * \brief 写入行高并返回拒绝原因。
     * \return 默认实现适配既有 SetRowHeight()，已有模型无需修改即可保持行为。
     */
    [[nodiscard]] virtual DuiWorkbookWriteResult SetRowHeightWithResult(
        DuiWorksheetId sheet, int row, int pixels)
    {
        if (SetRowHeight(sheet, row, pixels))
            return {};
        return {DuiWorkbookWriteErrorCode::Rejected, "Workbook model rejected the row height update"};
    }
    /**
     * \brief 写入列宽并返回拒绝原因。
     * \return 默认实现适配既有 SetColumnWidth()，已有模型无需修改即可保持行为。
     */
    [[nodiscard]] virtual DuiWorkbookWriteResult SetColumnWidthWithResult(
        DuiWorksheetId sheet, int column, int pixels)
    {
        if (SetColumnWidth(sheet, column, pixels))
            return {};
        return {DuiWorkbookWriteErrorCode::Rejected, "Workbook model rejected the column width update"};
    }

    /**
     * \brief 在给定区域内查找下一个匹配单元格。
     * \param startAfter 搜索起点；成功实现必须从该单元格之后开始，并按 options.wrap 决定是否回绕。
     */
    [[nodiscard]] virtual DuiWorkbookFindResult FindCell(
        DuiWorksheetId sheet, std::string_view query, std::optional<DuiCellAddress> startAfter,
        std::optional<DuiCellRange> range, DuiWorkbookFindOptions options) const
    {
        (void)sheet;
        (void)query;
        (void)startAfter;
        (void)range;
        (void)options;
        return {UnsupportedWorkbookOperation(), std::nullopt};
    }

    /** \brief 在给定区域内替换匹配文本，replaceAll 为 false 时只替换首个匹配项。 */
    [[nodiscard]] virtual DuiWorkbookReplaceResult ReplaceText(
        DuiWorksheetId sheet, std::string_view query, std::string_view replacement,
        std::optional<DuiCellRange> range, DuiWorkbookFindOptions options, bool replaceAll)
    {
        (void)sheet;
        (void)query;
        (void)replacement;
        (void)range;
        (void)options;
        (void)replaceAll;
        return {UnsupportedWorkbookOperation(), 0, std::nullopt};
    }

    /** \brief 由模型在指定行范围内执行多关键字排序。 */
    [[nodiscard]] virtual DuiWorkbookOperationResult ApplySort(
        DuiWorksheetId sheet, DuiCellRange range, const std::vector<DuiWorkbookSortKey>& keys)
    {
        (void)sheet;
        (void)range;
        (void)keys;
        return UnsupportedWorkbookOperation();
    }

    /** \brief 由模型设置当前 Sheet 的列筛选；空数组表示清除筛选。 */
    [[nodiscard]] virtual DuiWorkbookOperationResult SetFilters(
        DuiWorksheetId sheet, const std::vector<DuiWorkbookFilter>& filters)
    {
        (void)sheet;
        (void)filters;
        return UnsupportedWorkbookOperation();
    }

    /** \brief 返回模型预计算的完整行自动高度；空值表示由控件兼容计算。 */
    [[nodiscard]] virtual std::optional<int> PreferredRowHeight(DuiWorksheetId sheet, int row) const
    {
        (void)sheet;
        (void)row;
        return std::nullopt;
    }

    /** \brief 返回模型预计算的完整列自动宽度；空值表示由控件兼容计算。 */
    [[nodiscard]] virtual std::optional<int> PreferredColumnWidth(DuiWorksheetId sheet, int column) const
    {
        (void)sheet;
        (void)column;
        return std::nullopt;
    }

    /**
     * 订阅模型变化。回调必须在 UI 线程执行；默认空实现保持现有模型源兼容。
     * 控件收到通知后会重建行列度量并清空撤销历史。
     */
    [[nodiscard]] virtual core::DuiSubscription SubscribeChanged(std::function<void()> callback)
    {
        (void)callback;
        return {};
    }
};

} // namespace ysDui::controls::list
