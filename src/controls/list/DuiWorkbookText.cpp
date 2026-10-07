/**
 * 文件名：DuiWorkbookText.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-02
 * 用途：实现独立 CSV/TSV 工作表互操作适配器。
 */
#include "ysDui/controls/list/DuiWorkbookText.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace ysDui::controls::list {
namespace {

using ParsedRows = std::vector<std::vector<std::string>>;

struct SourceLocation final
{
    std::size_t offset{};
    int line{1};
    int column{1};
};

[[nodiscard]] SourceLocation Locate(std::string_view text, std::size_t offset)
{
    SourceLocation result{offset, 1, 1};
    for (std::size_t index{}; index < offset; ++index)
    {
        if (text[index] == '\n')
        {
            ++result.line;
            result.column = 1;
        }
        else
            ++result.column;
    }
    return result;
}

[[nodiscard]] DuiWorkbookTextError MakeError(DuiWorkbookTextErrorCode code, std::string message,
    DuiWorksheetId worksheet, std::string_view text = {}, std::size_t offset = {}, DuiCellAddress cell = {},
    DuiWorkbookWriteErrorCode modelError = DuiWorkbookWriteErrorCode::None)
{
    const SourceLocation location = Locate(text, (std::min)(offset, text.size()));
    return {code, std::move(message), worksheet, cell, location.offset, location.line, location.column, modelError};
}

[[nodiscard]] bool IsUtf8Continuation(unsigned char value)
{
    return value >= 0x80U && value <= 0xBFU;
}

[[nodiscard]] std::optional<std::size_t> InvalidUtf8Offset(std::string_view text)
{
    for (std::size_t index{}; index < text.size();)
    {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        if (first <= 0x7FU)
        {
            ++index;
            continue;
        }
        const auto has = [&text, index](std::size_t count)
        {
            return index + count < text.size()
                && IsUtf8Continuation(static_cast<unsigned char>(text[index + count]));
        };
        if (first >= 0xC2U && first <= 0xDFU && has(1))
        {
            index += 2;
            continue;
        }
        if (first >= 0xE0U && first <= 0xEFU && has(1) && has(2))
        {
            const unsigned char second = static_cast<unsigned char>(text[index + 1]);
            if ((first != 0xE0U || second >= 0xA0U) && (first != 0xEDU || second <= 0x9FU))
            {
                index += 3;
                continue;
            }
        }
        if (first >= 0xF0U && first <= 0xF4U && has(1) && has(2) && has(3))
        {
            const unsigned char second = static_cast<unsigned char>(text[index + 1]);
            if ((first != 0xF0U || second >= 0x90U) && (first != 0xF4U || second <= 0x8FU))
            {
                index += 4;
                continue;
            }
        }
        return index;
    }
    return {};
}

[[nodiscard]] char Delimiter(DuiDelimitedTextFormat format)
{
    return format == DuiDelimitedTextFormat::Csv ? ',' : '\t';
}

[[nodiscard]] bool WorksheetExists(const IDuiWorkbookModel& model, DuiWorksheetId worksheet)
{
    const int count = model.WorksheetCount();
    for (int index{}; index < count; ++index)
        if (model.WorksheetIdAt(index) == worksheet)
            return true;
    return false;
}

[[nodiscard]] std::optional<DuiWorkbookTextError> Parse(std::string_view text,
    DuiWorksheetId worksheet, DuiDelimitedTextFormat format, ParsedRows& rows)
{
    if (const auto invalid = InvalidUtf8Offset(text))
        return MakeError(DuiWorkbookTextErrorCode::InvalidUtf8, "Delimited text is not valid UTF-8",
            worksheet, text, *invalid);
    if (text.empty())
        return {};

    std::vector<std::string> row;
    std::string field;
    const char delimiter = Delimiter(format);
    bool quoted{};
    bool afterQuote{};
    std::size_t openingQuote{};
    const auto finishRow = [&]
    {
        row.push_back(std::move(field));
        field.clear();
        rows.push_back(std::move(row));
        row.clear();
        afterQuote = false;
    };

    for (std::size_t index{}; index < text.size(); ++index)
    {
        const char value = text[index];
        if (quoted)
        {
            if (value == '"')
            {
                if (index + 1 < text.size() && text[index + 1] == '"')
                {
                    field.push_back('"');
                    ++index;
                }
                else
                {
                    quoted = false;
                    afterQuote = true;
                }
                continue;
            }
            if (value == '\r' && index + 1 < text.size() && text[index + 1] == '\n')
                ++index;
            field.push_back(value == '\r' ? '\n' : value);
            continue;
        }
        if (afterQuote && value != delimiter && value != '\r' && value != '\n')
            return MakeError(DuiWorkbookTextErrorCode::ParseError,
                "Unexpected character after closing quote", worksheet, text, index);
        if (!afterQuote && value == '"' && field.empty())
        {
            quoted = true;
            openingQuote = index;
            continue;
        }
        if (value == delimiter)
        {
            row.push_back(std::move(field));
            field.clear();
            afterQuote = false;
            continue;
        }
        if (value == '\r' || value == '\n')
        {
            if (value == '\r' && index + 1 < text.size() && text[index + 1] == '\n')
                ++index;
            finishRow();
            continue;
        }
        field.push_back(value);
    }
    if (quoted)
        return MakeError(DuiWorkbookTextErrorCode::ParseError, "Unterminated quoted field", worksheet, text, openingQuote);
    if (text.back() != '\r' && text.back() != '\n')
        finishRow();
    return {};
}

void AppendField(std::string& output, std::string_view text, char delimiter)
{
    const bool quote = text.find_first_of(std::string{delimiter} + "\"\r\n") != std::string_view::npos;
    if (!quote)
    {
        output.append(text);
        return;
    }
    output.push_back('"');
    for (const char value : text)
    {
        if (value == '"')
            output.push_back('"');
        output.push_back(value);
    }
    output.push_back('"');
}

} // namespace

DuiWorkbookTextExportResult DuiWorkbookText::Export(const IDuiWorkbookModel& model,
    DuiWorksheetId worksheet, DuiCellRange range, DuiDelimitedTextFormat format)
{
    DuiWorkbookTextExportResult result;
    if (!WorksheetExists(model, worksheet))
    {
        result.error = MakeError(DuiWorkbookTextErrorCode::InvalidWorksheet, "Worksheet does not exist", worksheet);
        return result;
    }
    if (!range.first.Valid() || !range.last.Valid())
    {
        result.error = MakeError(DuiWorkbookTextErrorCode::InvalidRange, "Cell range is invalid", worksheet);
        return result;
    }
    range = range.Normalized();
    if (range.last.row >= model.RowCount(worksheet) || range.last.column >= model.ColumnCount(worksheet))
    {
        result.error = MakeError(DuiWorkbookTextErrorCode::InvalidRange, "Cell range exceeds worksheet bounds", worksheet);
        return result;
    }
    const char delimiter = Delimiter(format);
    for (int row = range.first.row; row <= range.last.row; ++row)
    {
        for (int column = range.first.column; column <= range.last.column; ++column)
        {
            if (column != range.first.column)
                result.text.push_back(delimiter);
            const DuiCellAddress cell{row, column};
            const std::string text = model.CellText(worksheet, cell);
            if (const auto invalid = InvalidUtf8Offset(text))
            {
                result.error = MakeError(DuiWorkbookTextErrorCode::InvalidUtf8,
                    "Workbook cell text is not valid UTF-8", worksheet, text, *invalid, cell);
                result.text.clear();
                return result;
            }
            AppendField(result.text, text, delimiter);
        }
        result.text += "\r\n";
    }
    return result;
}

DuiWorkbookTextImportResult DuiWorkbookText::Import(IDuiWorkbookModel& model,
    DuiWorksheetId worksheet, DuiCellAddress firstCell, std::string_view text,
    DuiDelimitedTextFormat format)
{
    DuiWorkbookTextImportResult result;
    if (!WorksheetExists(model, worksheet))
    {
        result.error = MakeError(DuiWorkbookTextErrorCode::InvalidWorksheet, "Worksheet does not exist", worksheet);
        return result;
    }
    if (!firstCell.Valid())
    {
        result.error = MakeError(DuiWorkbookTextErrorCode::InvalidRange, "First cell is invalid", worksheet, {}, 0, firstCell);
        return result;
    }
    ParsedRows rows;
    if (const auto error = Parse(text, worksheet, format, rows))
    {
        result.error = *error;
        return result;
    }
    int widest{};
    for (const auto& row : rows)
        widest = (std::max)(widest, static_cast<int>(row.size()));
    if (firstCell.row + static_cast<int>(rows.size()) > model.RowCount(worksheet)
        || firstCell.column + widest > model.ColumnCount(worksheet))
    {
        result.error = MakeError(DuiWorkbookTextErrorCode::InvalidRange,
            "Delimited text exceeds worksheet bounds", worksheet, {}, 0, firstCell);
        return result;
    }
    for (int row{}; row < static_cast<int>(rows.size()); ++row)
    {
        for (int column{}; column < static_cast<int>(rows[row].size()); ++column)
        {
            const DuiCellAddress cell{firstCell.row + row, firstCell.column + column};
            DuiWorkbookWriteResult write = model.SetCellTextWithResult(worksheet, cell, rows[row][column]);
            if (!write)
            {
                result.error = MakeError(DuiWorkbookTextErrorCode::ModelWriteRejected,
                    std::move(write.message), worksheet, {}, 0, cell, write.code);
                return result;
            }
            ++result.cellsWritten;
            if (!result.writtenRange)
                result.writtenRange = {cell, cell};
            else
            {
                result.writtenRange->last.row = (std::max)(result.writtenRange->last.row, cell.row);
                result.writtenRange->last.column = (std::max)(result.writtenRange->last.column, cell.column);
            }
        }
    }
    return result;
}

} // namespace ysDui::controls::list
