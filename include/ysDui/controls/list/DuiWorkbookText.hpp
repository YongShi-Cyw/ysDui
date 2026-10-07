/**
 * 文件名：DuiWorkbookText.hpp
 * 开发者：青蓝
 * 开发时间：2026-08-02
 * 用途：声明独立于 Spreadsheet 控件的 CSV/TSV 工作表互操作适配器。
 */
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "ysDui/controls/list/DuiWorkbookModel.hpp"

namespace ysDui::controls::list {

/** \brief 分隔文本格式。 */
enum class DuiDelimitedTextFormat
{
    Csv,
    Tsv,
};

/** \brief CSV/TSV 转换失败类别。 */
enum class DuiWorkbookTextErrorCode
{
    None,
    InvalidWorksheet,
    InvalidRange,
    InvalidUtf8,
    ParseError,
    ModelWriteRejected,
};

/** \brief CSV/TSV 转换错误，位置采用 UTF-8 字节偏移及 1 基行列。 */
struct DuiWorkbookTextError final
{
    DuiWorkbookTextErrorCode code{DuiWorkbookTextErrorCode::None};
    std::string message;
    DuiWorksheetId worksheet{};
    DuiCellAddress cell;
    std::size_t byteOffset{};
    int line{1};
    int column{1};
    DuiWorkbookWriteErrorCode modelError{DuiWorkbookWriteErrorCode::None};
};

/** \brief 分隔文本导出结果。 */
struct DuiWorkbookTextExportResult final
{
    std::string text;
    std::optional<DuiWorkbookTextError> error;

    [[nodiscard]] explicit operator bool() const noexcept { return !error.has_value(); }
};

/** \brief 分隔文本导入结果。 */
struct DuiWorkbookTextImportResult final
{
    std::size_t cellsWritten{};
    /** \brief 成功写入单元格的最小包围区域；未写入时为空。 */
    std::optional<DuiCellRange> writtenRange;
    std::optional<DuiWorkbookTextError> error;

    [[nodiscard]] explicit operator bool() const noexcept { return !error.has_value(); }
};

/** \brief 仅通过 IDuiWorkbookModel 进行 CSV/TSV 单 Sheet 转换的无状态适配器。 */
class DuiWorkbookText final
{
public:
    /**
     * \brief 将指定有限区域导出为 UTF-8 CSV 或 TSV。
     * \param model 宿主持有的工作簿模型。
     * \param worksheet 要导出的工作表。
     * \param range 包含边界的单元格区域。
     * \param format 目标分隔文本格式。
     */
    [[nodiscard]] static DuiWorkbookTextExportResult Export(const IDuiWorkbookModel& model,
        DuiWorksheetId worksheet, DuiCellRange range, DuiDelimitedTextFormat format);

    /**
     * \brief 将 UTF-8 CSV 或 TSV 导入到指定起始单元格。
     * \param model 宿主持有的工作簿模型。
     * \param worksheet 目标工作表。
     * \param firstCell 导入区域左上角。
     * \param text UTF-8 分隔文本。
     * \param format 源分隔文本格式。
     */
    [[nodiscard]] static DuiWorkbookTextImportResult Import(IDuiWorkbookModel& model,
        DuiWorksheetId worksheet, DuiCellAddress firstCell, std::string_view text,
        DuiDelimitedTextFormat format);
};

} // namespace ysDui::controls::list
