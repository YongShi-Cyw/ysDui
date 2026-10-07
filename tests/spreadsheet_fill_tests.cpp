/**
 * 文件名：spreadsheet_fill_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-10
 * 用途：验证工作表填充柄拖拽、序列推断、撤销和失败回滚。
 */
#include "spreadsheet_test_support.hpp"

using namespace ysDui;
using namespace ysDui::controls::list;
using namespace ysDui::test::spreadsheet;

int main()
{
    WorkbookModel verticalModel;
    DuiSpreadsheet vertical;
    vertical.SetWorkbookModel(&verticalModel);
    vertical.Layout({0, 0, 500, 300});
    const DuiWorksheetId verticalSheet = vertical.ActiveWorksheet();
    assert(verticalModel.SetCellText(verticalSheet, {1, 1}, "9"));
    assert(verticalModel.SetCellText(verticalSheet, {2, 1}, "10"));
    vertical.SetSelection({{1, 1}, {2, 1}});

    assert(vertical.OnEvent(test::MakeEvent(core::EventType::PointerDown, {233, 95})));
    assert(vertical.Captured());
    assert(vertical.OnEvent(test::MakeEvent(core::EventType::PointerMove, {233, 155})));
    assert(vertical.Selection().Normalized() == (DuiCellRange{{1, 1}, {2, 1}}));
    assert(verticalModel.CellText(verticalSheet, {3, 1}).empty());
    RecordingCanvas preview;
    vertical.Paint(preview, vertical.Bounds());
    const core::Color activeColor = vertical.Theme().Get(core::ThemeSlot::SpreadsheetActiveCell);
    const core::Color previewColor{128, 128, 128, 255};
    assert(std::count_if(preview.fills.begin(), preview.fills.end(), [previewColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == previewColor
            && ((fill.bounds.Width() == 2 && fill.bounds.Height() == 3)
                || (fill.bounds.Width() == 3 && fill.bounds.Height() == 2));
    }) > 20);
    assert(std::any_of(preview.fills.begin(), preview.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.left == 136 && fill.bounds.top == 94
            && fill.bounds.Width() == 99 && fill.bounds.Height() == 3;
    }));
    assert(!std::any_of(preview.fills.begin(), preview.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.left == 136 && fill.bounds.top == 166
            && fill.bounds.Width() == 99 && fill.bounds.Height() == 3;
    }));

    assert(vertical.OnEvent(test::MakeEvent(core::EventType::PointerUp, {233, 155})));
    assert(!vertical.Captured());
    assert(vertical.Selection().Normalized() == (DuiCellRange{{1, 1}, {5, 1}}));
    assert(verticalModel.CellText(verticalSheet, {3, 1}) == "11");
    assert(verticalModel.CellText(verticalSheet, {4, 1}) == "12");
    assert(verticalModel.CellText(verticalSheet, {5, 1}) == "13");
    vertical.Undo();
    assert(verticalModel.CellText(verticalSheet, {3, 1}).empty());
    assert(verticalModel.CellText(verticalSheet, {5, 1}).empty());
    vertical.Redo();
    assert(verticalModel.CellText(verticalSheet, {3, 1}) == "11");
    assert(verticalModel.CellText(verticalSheet, {5, 1}) == "13");

    WorkbookModel upwardModel;
    DuiSpreadsheet upward;
    upward.SetWorkbookModel(&upwardModel);
    upward.Layout({0, 0, 500, 300});
    const DuiWorksheetId upwardSheet = upward.ActiveWorksheet();
    assert(upwardModel.SetCellText(upwardSheet, {3, 1}, "10"));
    assert(upwardModel.SetCellText(upwardSheet, {4, 1}, "12"));
    upward.SetSelection({{3, 1}, {4, 1}});
    assert(upward.OnEvent(test::MakeEvent(core::EventType::PointerDown, {233, 143})));
    assert(upward.OnEvent(test::MakeEvent(core::EventType::PointerMove, {233, 60})));
    assert(upward.OnEvent(test::MakeEvent(core::EventType::PointerUp, {233, 60})));
    assert(upwardModel.CellText(upwardSheet, {2, 1}) == "8");
    assert(upwardModel.CellText(upwardSheet, {1, 1}) == "6");

    WorkbookModel horizontalModel;
    DuiSpreadsheet horizontal;
    horizontal.SetWorkbookModel(&horizontalModel);
    horizontal.Layout({0, 0, 500, 300});
    const DuiWorksheetId horizontalSheet = horizontal.ActiveWorksheet();
    assert(horizontalModel.SetCellText(horizontalSheet, {1, 1}, "A"));
    assert(horizontalModel.SetCellText(horizontalSheet, {1, 2}, "B"));
    horizontal.SetSelection({{1, 1}, {1, 2}});
    assert(horizontal.OnEvent(test::MakeEvent(core::EventType::PointerDown, {329, 71})));
    assert(horizontal.OnEvent(test::MakeEvent(core::EventType::PointerMove, {460, 60})));
    assert(horizontal.OnEvent(test::MakeEvent(core::EventType::PointerUp, {460, 60})));
    assert(horizontalModel.CellText(horizontalSheet, {1, 3}) == "A");
    assert(horizontalModel.CellText(horizontalSheet, {1, 4}) == "B");

    WorkbookModel cancelModel;
    DuiSpreadsheet cancel;
    cancel.SetWorkbookModel(&cancelModel);
    cancel.Layout({0, 0, 500, 300});
    const DuiWorksheetId cancelSheet = cancel.ActiveWorksheet();
    assert(cancelModel.SetCellText(cancelSheet, {1, 1}, "A"));
    assert(cancelModel.SetCellText(cancelSheet, {1, 2}, "B"));
    cancel.SetSelection({{1, 1}, {1, 2}});
    assert(cancel.OnEvent(test::MakeEvent(core::EventType::PointerDown, {329, 71})));
    assert(cancel.OnEvent(test::MakeEvent(core::EventType::PointerMove, {460, 60})));
    assert(cancel.OnEvent(test::MakeEvent(core::EventType::PointerCancel, {460, 60})));
    assert(!cancel.Captured());
    assert(cancel.Selection().Normalized() == (DuiCellRange{{1, 1}, {1, 2}}));
    assert(cancelModel.CellText(cancelSheet, {1, 3}).empty());
    assert(cancelModel.CellText(cancelSheet, {1, 4}).empty());

    WorkbookModel rollbackModel;
    DuiSpreadsheet rollback;
    rollback.SetWorkbookModel(&rollbackModel);
    rollback.Layout({0, 0, 500, 300});
    const DuiWorksheetId rollbackSheet = rollback.ActiveWorksheet();
    assert(rollbackModel.SetCellText(rollbackSheet, {1, 1}, "1"));
    assert(rollbackModel.SetCellText(rollbackSheet, {2, 1}, "2"));
    assert(rollbackModel.SetCellText(rollbackSheet, {3, 1}, "old-3"));
    assert(rollbackModel.SetCellText(rollbackSheet, {4, 1}, "old-4"));
    rollback.SetSelection({{1, 1}, {2, 1}});
    int writeErrors{};
    rollback.SetWorkbookWriteErrorHandler([&writeErrors](const DuiSpreadsheetWriteError&) { ++writeErrors; });
    rollbackModel.SetCellWriteFailureAfter(1);
    assert(rollback.OnEvent(test::MakeEvent(core::EventType::PointerDown, {233, 95})));
    assert(rollback.OnEvent(test::MakeEvent(core::EventType::PointerMove, {233, 131})));
    assert(!rollback.OnEvent(test::MakeEvent(core::EventType::PointerUp, {233, 131})));
    assert(writeErrors == 1);
    assert(rollback.Selection().Normalized() == (DuiCellRange{{1, 1}, {2, 1}}));
    assert(rollbackModel.CellText(rollbackSheet, {3, 1}) == "old-3");
    assert(rollbackModel.CellText(rollbackSheet, {4, 1}) == "old-4");

    return 0;
}
