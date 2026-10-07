/**
 * 文件名：workbook_text_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-02
 * 用途：验证独立 CSV/TSV 工作表适配器的 UTF-8、转义与错误诊断。
 */
#include <cassert>
#include <map>
#include <string>

#include "ysDui/controls/list/DuiWorkbookText.hpp"

namespace {

using namespace ysDui::controls::list;

class MemoryWorkbook final : public IDuiWorkbookModel
{
public:
    [[nodiscard]] int WorksheetCount() const override { return 1; }
    [[nodiscard]] DuiWorksheetId WorksheetIdAt(int index) const override { return index == 0 ? 1 : 0; }
    [[nodiscard]] std::string WorksheetName(DuiWorksheetId sheet) const override { return sheet == 1 ? "Sheet1" : std::string{}; }
    bool RenameWorksheet(DuiWorksheetId, std::string) override { return false; }
    [[nodiscard]] DuiWorksheetId AddWorksheet(std::string) override { return 0; }
    bool RemoveWorksheet(DuiWorksheetId) override { return false; }
    [[nodiscard]] int RowCount(DuiWorksheetId) const override { return 8; }
    [[nodiscard]] int ColumnCount(DuiWorksheetId) const override { return 8; }
    [[nodiscard]] std::string CellText(DuiWorksheetId, DuiCellAddress cell) const override
    {
        const auto found = cells.find({cell.row, cell.column});
        return found == cells.end() ? std::string{} : found->second;
    }
    bool SetCellText(DuiWorksheetId, DuiCellAddress cell, std::string text) override
    {
        if (!cell.Valid() || rejectWrites || successfulWritesBeforeReject == 0)
            return false;
        if (successfulWritesBeforeReject > 0)
            --successfulWritesBeforeReject;
        cells[{cell.row, cell.column}] = std::move(text);
        return true;
    }
    [[nodiscard]] DuiWorkbookWriteResult SetCellTextWithResult(
        DuiWorksheetId sheet, DuiCellAddress cell, std::string text) override
    {
        if (rejectWrites)
            return {DuiWorkbookWriteErrorCode::ReadOnly, "单元格受保护"};
        return IDuiWorkbookModel::SetCellTextWithResult(sheet, cell, std::move(text));
    }
    [[nodiscard]] int RowHeight(DuiWorksheetId, int) const override { return 24; }
    [[nodiscard]] int ColumnWidth(DuiWorksheetId, int) const override { return 96; }
    bool SetRowHeight(DuiWorksheetId, int, int) override { return false; }
    bool SetColumnWidth(DuiWorksheetId, int, int) override { return false; }

    std::map<std::pair<int, int>, std::string> cells;
    bool rejectWrites{};
    int successfulWritesBeforeReject{-1};
};

} // namespace

int main()
{
    MemoryWorkbook workbook;
    workbook.cells[{0, 0}] = "中文";
    workbook.cells[{0, 1}] = "comma,value";
    workbook.cells[{0, 2}] = "quote\"value";
    workbook.cells[{1, 0}] = "line\nbreak";
    workbook.cells[{1, 1}] = "emoji \xF0\x9F\x98\x80";

    const auto csv = DuiWorkbookText::Export(workbook, 1, {{0, 0}, {1, 2}}, DuiDelimitedTextFormat::Csv);
    assert(csv);
    assert(csv.text == "中文,\"comma,value\",\"quote\"\"value\"\r\n\"line\nbreak\",emoji \xF0\x9F\x98\x80,\r\n");

    const auto tsv = DuiWorkbookText::Export(workbook, 1, {{0, 0}, {1, 2}}, DuiDelimitedTextFormat::Tsv);
    assert(tsv);
    assert(tsv.text.find("\"line\nbreak\"") != std::string::npos);

    const auto imported = DuiWorkbookText::Import(workbook, 1, {3, 2},
        "alpha\tbeta\r\n\"line\nbreak\"\t\"emoji \xF0\x9F\x98\x80\"", DuiDelimitedTextFormat::Tsv);
    assert(imported);
    assert(imported.cellsWritten == 4);
    assert(imported.writtenRange.has_value());
    assert(imported.writtenRange->first == (DuiCellAddress{3, 2}));
    assert(imported.writtenRange->last == (DuiCellAddress{4, 3}));
    assert(workbook.CellText(1, {3, 2}) == "alpha");
    assert(workbook.CellText(1, {4, 2}) == "line\nbreak");
    assert(workbook.CellText(1, {4, 3}) == "emoji \xF0\x9F\x98\x80");

    const auto malformed = DuiWorkbookText::Import(workbook, 1, {0, 0}, "alpha,\"unterminated",
        DuiDelimitedTextFormat::Csv);
    assert(!malformed);
    assert(malformed.error.has_value());
    assert(malformed.error->code == DuiWorkbookTextErrorCode::ParseError);
    assert(malformed.error->worksheet == 1);
    assert(malformed.error->line == 1 && malformed.error->column == 7);

    const auto invalidWorksheet = DuiWorkbookText::Export(workbook, 99, {{0, 0}, {0, 0}},
        DuiDelimitedTextFormat::Csv);
    assert(!invalidWorksheet);
    assert(invalidWorksheet.error.has_value());
    assert(invalidWorksheet.error->code == DuiWorkbookTextErrorCode::InvalidWorksheet);
    assert(invalidWorksheet.error->worksheet == 99);

    const auto roundTrip = DuiWorkbookText::Import(workbook, 1, {6, 0}, csv.text, DuiDelimitedTextFormat::Csv);
    assert(roundTrip);
    assert(roundTrip.cellsWritten == 6);
    assert(roundTrip.writtenRange.has_value());
    assert(roundTrip.writtenRange->first == (DuiCellAddress{6, 0}));
    assert(roundTrip.writtenRange->last == (DuiCellAddress{7, 2}));

    workbook.rejectWrites = true;
    const auto rejected = DuiWorkbookText::Import(workbook, 1, {5, 1}, "blocked", DuiDelimitedTextFormat::Csv);
    assert(!rejected);
    assert(rejected.error.has_value());
    assert(rejected.error->code == DuiWorkbookTextErrorCode::ModelWriteRejected);
    assert(rejected.error->worksheet == 1);
    assert(rejected.error->cell == (DuiCellAddress{5, 1}));
    assert(rejected.error->modelError == DuiWorkbookWriteErrorCode::ReadOnly);
    assert(rejected.error->message == "单元格受保护");

    workbook.rejectWrites = false;
    workbook.successfulWritesBeforeReject = 1;
    const auto partial = DuiWorkbookText::Import(workbook, 1, {5, 1}, "left,right", DuiDelimitedTextFormat::Csv);
    assert(!partial);
    assert(partial.cellsWritten == 1);
    assert(partial.writtenRange.has_value());
    assert(partial.writtenRange->first == (DuiCellAddress{5, 1}));
    assert(partial.writtenRange->last == (DuiCellAddress{5, 1}));
    assert(partial.error.has_value());
    assert(partial.error->code == DuiWorkbookTextErrorCode::ModelWriteRejected);
    assert(partial.error->cell == (DuiCellAddress{5, 2}));
    assert(partial.error->modelError == DuiWorkbookWriteErrorCode::Rejected);

    const auto invalidUtf8 = DuiWorkbookText::Import(workbook, 1, {0, 0}, "\xC3", DuiDelimitedTextFormat::Csv);
    assert(!invalidUtf8);
    assert(invalidUtf8.error.has_value());
    assert(invalidUtf8.error->code == DuiWorkbookTextErrorCode::InvalidUtf8);

    return 0;
}
