/**
 * 文件名：spreadsheet_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-31
 * 用途：验证工作表模型、虚拟范围和基本编辑交互。
 */
#include "spreadsheet_test_support.hpp"

#include "ysDui/controls/input/DuiScrollBar.hpp"

using namespace ysDui;
using namespace ysDui::controls::list;
using namespace ysDui::test::spreadsheet;

int main()
{
    core::DuiTheme theme;
    const core::Color white{255, 255, 255, 255};
    assert(theme.Get(core::ThemeSlot::SpreadsheetBackground) == white);
    assert((theme.Get(core::ThemeSlot::SpreadsheetGridLine) == core::Color{230, 230, 230, 255}));
    assert((theme.Get(core::ThemeSlot::SpreadsheetSelection) == core::Color{230, 230, 230, 255}));
    const DuiCellRange normalized = DuiCellRange{{4, 3}, {1, 0}}.Normalized();
    const DuiCellAddress expectedFirst{1, 0};
    const DuiCellAddress expectedLast{4, 3};
    assert(normalized.first == expectedFirst && normalized.last == expectedLast);

    WorkbookModel viewportModel;
    viewportModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    viewportModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    controls::list::DuiSpreadsheet viewportSpreadsheet;
    viewportSpreadsheet.SetWorkbookModel(&viewportModel);
    assert(viewportModel.RequestCellRangesCalls() == 0);
    viewportSpreadsheet.Layout({0, 0, 500, 300});
    assert(viewportModel.RequestCellRangesCalls() == 1);
    assert(viewportModel.LastRequestedSheet() == viewportSpreadsheet.ActiveWorksheet());
    assert((viewportModel.RequestedRanges() == std::vector<DuiCellRange>{{{0, 0}, {10, 6}}}));
    viewportSpreadsheet.Layout({0, 0, 500, 300});
    RecordingCanvas viewportCanvas;
    viewportSpreadsheet.Paint(viewportCanvas, viewportSpreadsheet.Bounds());
    assert(viewportModel.RequestCellRangesCalls() == 1);

    auto* horizontalScroll = dynamic_cast<controls::input::DuiScrollBar*>(viewportSpreadsheet.Children()[0].get());
    assert(horizontalScroll != nullptr);
    horizontalScroll->SetPosition(1);
    assert(viewportModel.RequestCellRangesCalls() == 1);
    horizontalScroll->SetPosition(289);
    assert(viewportModel.RequestCellRangesCalls() == 2);
    assert((viewportModel.RequestedRanges() == std::vector<DuiCellRange>{
        {{0, 0}, {10, 0}}, {{0, 4}, {10, 9}}}));
    horizontalScroll->SetPosition(horizontalScroll->Position() + 1);
    assert(viewportModel.RequestCellRangesCalls() == 2);

    std::size_t visibleCellReads{};
    viewportModel.SetCellPresentationReadHandler([&visibleCellReads] { ++visibleCellReads; });
    RecordingCanvas scrolledViewportCanvas;
    viewportSpreadsheet.Paint(scrolledViewportCanvas, viewportSpreadsheet.Bounds());
    assert(visibleCellReads == 55);
    assert(viewportModel.RequestCellRangesCalls() == 2);

    assert(viewportSpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::PointerWheel, {200, 100}, {}, {}, {}, -1)));
    assert(viewportModel.RequestCellRangesCalls() == 3);
    assert((viewportModel.RequestedRanges() == std::vector<DuiCellRange>{
        {{0, 0}, {0, 0}}, {{0, 4}, {0, 9}}, {{4, 0}, {13, 0}}, {{4, 4}, {13, 9}}}));

    WorkbookModel edgeViewportModel;
    edgeViewportModel.SetDimensions(3, 5);
    edgeViewportModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    edgeViewportModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    controls::list::DuiSpreadsheet edgeViewportSpreadsheet;
    edgeViewportSpreadsheet.SetWorkbookModel(&edgeViewportModel);
    edgeViewportSpreadsheet.Layout({0, 0, 500, 300});
    assert((edgeViewportModel.RequestedRanges() == std::vector<DuiCellRange>{{{0, 0}, {2, 4}}}));

    WorkbookModel emptyViewportModel;
    emptyViewportModel.SetDimensions(0, 0);
    emptyViewportModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    emptyViewportModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    controls::list::DuiSpreadsheet emptyViewportSpreadsheet;
    emptyViewportSpreadsheet.SetWorkbookModel(&emptyViewportModel);
    emptyViewportSpreadsheet.Layout({0, 0, 500, 300});
    assert(emptyViewportModel.RequestCellRangesCalls() == 0);

    assert(viewportSpreadsheet.SetActiveWorksheet(2));
    assert(viewportModel.RequestCellRangesCalls() == 4);
    assert(viewportModel.LastRequestedSheet() == 2);

    WorkbookModel sheetBarModel;
    for (int index = 3; index <= 6; ++index)
        assert(sheetBarModel.AddWorksheet("Sheet" + std::to_string(index)) != 0);
    controls::list::DuiSpreadsheet sheetBarSpreadsheet;
    sheetBarSpreadsheet.SetWorkbookModel(&sheetBarModel);
    sheetBarSpreadsheet.Layout({0, 0, 500, 300});
    auto* sheetHorizontal = dynamic_cast<controls::input::DuiScrollBar*>(sheetBarSpreadsheet.Children()[0].get());
    auto* sheetVertical = dynamic_cast<controls::input::DuiScrollBar*>(sheetBarSpreadsheet.Children()[1].get());
    assert(sheetHorizontal != nullptr && sheetVertical != nullptr);
    assert((sheetHorizontal->Bounds() == core::Rect{234, 278, 500, 295}));
    assert((sheetVertical->Bounds() == core::Rect{483, 24, 500, 274}));
    sheetBarSpreadsheet.SetFrozenPanes(3, 4);
    assert(sheetBarSpreadsheet.FrozenRowCount() == 3 && sheetBarSpreadsheet.FrozenColumnCount() == 4);
    sheetBarSpreadsheet.ClearFrozenPanes();
    assert(sheetBarSpreadsheet.FrozenRowCount() == 0 && sheetBarSpreadsheet.FrozenColumnCount() == 0);
    assert(sheetBarSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {47, 282})));
    assert(sheetBarSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {100, 282})));
    assert(sheetBarSpreadsheet.ActiveWorksheet() == sheetBarModel.WorksheetIdAt(1));
    UiHostFactoryMock sheetListFactory;
    sheetBarSpreadsheet.SetPopupContext(sheetListFactory, {});
    assert(sheetBarSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {83, 282})));
    assert(sheetListFactory.popup != nullptr);
    assert(sheetListFactory.popup->Dispatch(test::MakeEvent(
        core::EventType::PointerDown, {20, 172})));
    assert(sheetBarSpreadsheet.ActiveWorksheet() == sheetBarModel.WorksheetIdAt(5));
    RecordingCanvas sheetBarCanvas;
    sheetBarSpreadsheet.Paint(sheetBarCanvas, sheetBarSpreadsheet.Bounds());
    const core::Color sheetBarColor = theme.Get(core::ThemeSlot::ScrollTrack);
    assert(std::any_of(sheetBarCanvas.fills.begin(), sheetBarCanvas.fills.end(), [sheetBarColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == sheetBarColor && fill.bounds == core::Rect{0, 274, 500, 300};
    }));
    assert(std::any_of(sheetBarCanvas.fills.begin(), sheetBarCanvas.fills.end(), [](const RecordingCanvas::Fill& fill)
    {
        return fill.color == core::Color{240, 240, 240, 255}
            && fill.bounds == core::Rect{483, 0, 500, 24};
    }));
    assert(sheetBarSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {29, 282})));
    assert(!sheetBarSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {120, 285})));
    RecordingCanvas hoveredSheetCanvas;
    sheetBarSpreadsheet.Paint(hoveredSheetCanvas, sheetBarSpreadsheet.Bounds());
    assert(std::any_of(hoveredSheetCanvas.fills.begin(), hoveredSheetCanvas.fills.end(), [](const RecordingCanvas::Fill& fill)
    {
        return fill.color == core::Color{207, 207, 207, 255}
            && fill.bounds.Width() == 100 && fill.bounds.Height() == 22;
    }));
    assert(!sheetBarSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerLeave, {500, 300})));
    RecordingCanvas unhoveredSheetCanvas;
    sheetBarSpreadsheet.Paint(unhoveredSheetCanvas, sheetBarSpreadsheet.Bounds());
    assert(std::none_of(unhoveredSheetCanvas.fills.begin(), unhoveredSheetCanvas.fills.end(), [](const RecordingCanvas::Fill& fill)
    {
        return fill.color == core::Color{207, 207, 207, 255};
    }));

    WorkbookModel sheetOperationModel;
    const DuiWorksheetId sourceSheet = sheetOperationModel.WorksheetIdAt(0);
    assert(sheetOperationModel.SetCellText(sourceSheet, {0, 0}, "source"));
    controls::list::DuiSpreadsheet sheetOperationSpreadsheet;
    TextInputMock sheetNameInput;
    UiHostFactoryMock sheetOperationFactory;
    sheetOperationSpreadsheet.SetWorkbookModel(&sheetOperationModel);
    sheetOperationSpreadsheet.SetTextInput(&sheetNameInput);
    sheetOperationSpreadsheet.SetPopupContext(sheetOperationFactory, {});
    sheetOperationSpreadsheet.Layout({0, 0, 500, 300});

    assert(sheetOperationSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::PointerDown, {120, 285}, 0, 0, 0, 0, core::PointerButton::Secondary)));
    assert(sheetOperationFactory.popup != nullptr);
    assert(sheetOperationFactory.popup->Dispatch(test::MakeEvent(core::EventType::PointerDown, {20, 20})));
    const DuiWorksheetId insertedSheet = sheetOperationSpreadsheet.ActiveWorksheet();
    assert(insertedSheet != 0 && insertedSheet != sourceSheet);
    assert(sheetOperationModel.WorksheetIdAt(0) == sourceSheet);
    assert(sheetOperationModel.WorksheetIdAt(1) == insertedSheet);

    assert(sheetOperationSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::PointerDoubleClick, {120, 285})));
    assert(sheetNameInput.visible && sheetNameInput.text == "Sheet3");
    sheetNameInput.text = "Sheet1";
    sheetNameInput.Submit();
    assert(sheetNameInput.visible && sheetOperationModel.WorksheetName(insertedSheet) == "Sheet3");
    sheetNameInput.text = "Plan";
    sheetNameInput.Submit();
    assert(!sheetNameInput.visible && sheetOperationModel.WorksheetName(insertedSheet) == "Plan");
    assert(sheetOperationModel.SetCellText(insertedSheet, {0, 0}, "copied"));

    RecordingCanvas selectedSheetCanvas;
    sheetOperationSpreadsheet.Paint(selectedSheetCanvas, sheetOperationSpreadsheet.Bounds());
    const auto selectedText = std::find(selectedSheetCanvas.drawnTexts.begin(),
                                        selectedSheetCanvas.drawnTexts.end(), "Plan");
    assert(selectedText != selectedSheetCanvas.drawnTexts.end());
    const std::size_t selectedTextIndex = static_cast<std::size_t>(
        std::distance(selectedSheetCanvas.drawnTexts.begin(), selectedText));
    assert(selectedSheetCanvas.drawnTextStyles[selectedTextIndex].color
           == theme.Get(core::ThemeSlot::BrandPrimary));

    assert(sheetOperationSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::PointerDown, {120, 285}, 0, 0, 0, 0, core::PointerButton::Secondary)));
    assert(sheetOperationFactory.popup->Dispatch(test::MakeEvent(core::EventType::PointerDown, {20, 84})));
    const DuiWorksheetId duplicateSheet = sheetOperationSpreadsheet.ActiveWorksheet();
    assert(duplicateSheet != insertedSheet);
    assert(sheetOperationModel.WorksheetName(duplicateSheet) == "Plan (2)");
    assert(sheetOperationModel.CellText(duplicateSheet, {0, 0}) == "copied");

    assert(sheetOperationSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::PointerDown, {120, 285}, 0, 0, 0, 0, core::PointerButton::Secondary)));
    assert(sheetOperationFactory.popup->Dispatch(test::MakeEvent(core::EventType::PointerDown, {20, 52})));
    assert(sheetOperationModel.WorksheetCount() == 3);
    for (int index = 0; index < sheetOperationModel.WorksheetCount(); ++index)
        assert(sheetOperationModel.WorksheetIdAt(index) != duplicateSheet);
    assert(sheetOperationSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {205, 285})));
    assert(sheetOperationModel.WorksheetCount() == 4);
    const DuiWorksheetId uniqueSheet = sheetOperationSpreadsheet.AddWorksheet("Sheet1");
    assert(uniqueSheet != 0 && sheetOperationModel.WorksheetName(uniqueSheet) == "Sheet1 (2)");

    WorkbookModel sparseModel;
    sparseModel.SetSparseRowSizes(DuiSparseAxisSizes{
        30, {{3, 40}, {1, 50}, {3, 60}, {-1, 200}, {100000, 200}}});
    sparseModel.SetSparseColumnSizes(DuiSparseAxisSizes{
        80, {{2, 120}, {0, 70}, {2, 130}, {-1, 200}, {1000, 200}}});
    sparseModel.ResetDimensionReadCounts();
    controls::list::DuiSpreadsheet sparseSpreadsheet;
    sparseSpreadsheet.SetWorkbookModel(&sparseModel);
    assert(sparseModel.SparseRowSizeReads() == 1 && sparseModel.SparseColumnSizeReads() == 1);
    assert(sparseModel.RowHeightReads() == 0 && sparseModel.ColumnWidthReads() == 0);
    sparseSpreadsheet.Layout({0, 0, 500, 300});
    assert((sparseSpreadsheet.VisibleCellBounds({0, 0}) == core::Rect{42, 24, 112, 54}));
    assert((sparseSpreadsheet.VisibleCellBounds({1, 1}) == core::Rect{112, 54, 192, 104}));
    assert((sparseSpreadsheet.VisibleCellBounds({3, 2}) == core::Rect{192, 134, 322, 194}));
    sparseSpreadsheet.SetActiveCell({99999, 999});
    const auto lastSparseCell = sparseSpreadsheet.VisibleCellBounds({99999, 999});
    assert(lastSparseCell.has_value() && lastSparseCell->left >= 42 && lastSparseCell->right <= 486
           && lastSparseCell->top >= 24 && lastSparseCell->bottom <= 274);

    sparseSpreadsheet.SetFrozenPanes(99999, 999);
    assert(sparseSpreadsheet.FrozenRowCount() == 99999 && sparseSpreadsheet.FrozenColumnCount() == 999);
    std::size_t frozenCellReads{};
    sparseModel.SetCellPresentationReadHandler([&frozenCellReads] { ++frozenCellReads; });
    RecordingCanvas sparseCanvas;
    sparseSpreadsheet.Paint(sparseCanvas, sparseSpreadsheet.Bounds());
    assert(frozenCellReads < 500);

    WorkbookModel fallbackModel;
    fallbackModel.SetDimensions(4, 3);
    fallbackModel.ResetDimensionReadCounts();
    controls::list::DuiSpreadsheet fallbackSpreadsheet;
    fallbackSpreadsheet.SetWorkbookModel(&fallbackModel);
    assert(fallbackModel.SparseRowSizeReads() == 1 && fallbackModel.SparseColumnSizeReads() == 1);
    assert(fallbackModel.RowHeightReads() == 4 && fallbackModel.ColumnWidthReads() == 3);

    WorkbookModel sparseResizeModel;
    sparseResizeModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    sparseResizeModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    controls::list::DuiSpreadsheet sparseResizeSpreadsheet;
    sparseResizeSpreadsheet.SetWorkbookModel(&sparseResizeModel);
    sparseResizeSpreadsheet.Layout({0, 0, 500, 300});
    sparseResizeModel.ResetCellRangeRequests();
    sparseResizeModel.ResetDimensionReadCounts();
    assert(sparseResizeSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 48})));
    assert(sparseResizeSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {10, 68})));
    assert(sparseResizeSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {10, 68})));
    assert(sparseResizeModel.RowHeightReads() == 1 && sparseResizeModel.ColumnWidthReads() == 0);
    assert(sparseResizeSpreadsheet.VisibleCellBounds({1, 0})->top == 68);
    assert(sparseResizeModel.RequestCellRangesCalls() == 1);
    assert((sparseResizeModel.RequestedRanges() == std::vector<DuiCellRange>{{{0, 0}, {9, 6}}}));
    sparseResizeSpreadsheet.Undo();
    assert(sparseResizeModel.RowHeightReads() == 2);
    assert(sparseResizeSpreadsheet.VisibleCellBounds({1, 0})->top == 48);
    assert(sparseResizeModel.RequestCellRangesCalls() == 2);
    assert((sparseResizeModel.RequestedRanges() == std::vector<DuiCellRange>{{{0, 0}, {10, 6}}}));
    sparseResizeSpreadsheet.Redo();
    assert(sparseResizeModel.RowHeightReads() == 3);
    assert(sparseResizeSpreadsheet.VisibleCellBounds({1, 0})->top == 68);
    assert(sparseResizeModel.RequestCellRangesCalls() == 3);

    WorkbookModel model;
    controls::list::DuiSpreadsheet spreadsheet;
    TextInputMock input;
    ClipboardMock clipboard;
    spreadsheet.SetWorkbookModel(&model);
    spreadsheet.SetTextInput(&input);
    spreadsheet.SetClipboard(&clipboard);
    spreadsheet.Layout({0, 0, 500, 300});
    RecordingCanvas frameCanvas;
    spreadsheet.Paint(frameCanvas, spreadsheet.Bounds());
    const core::Color frameColor = theme.Get(core::ThemeSlot::SpreadsheetGridLine);
    assert(std::any_of(frameCanvas.fills.begin(), frameCanvas.fills.end(), [frameColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == frameColor && fill.bounds == core::Rect{0, 0, 500, 1};
    }));
    assert(std::any_of(frameCanvas.fills.begin(), frameCanvas.fills.end(), [frameColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == frameColor && fill.bounds == core::Rect{0, 0, 1, 274};
    }));
    assert(std::any_of(frameCanvas.fills.begin(), frameCanvas.fills.end(), [frameColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == frameColor && fill.bounds == core::Rect{499, 0, 500, 274};
    }));
    assert(std::any_of(frameCanvas.fills.begin(), frameCanvas.fills.end(), [frameColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == frameColor && fill.bounds == core::Rect{0, 273, 500, 274};
    }));
    assert(spreadsheet.ActiveWorksheet() == 1 && spreadsheet.ActiveCell() == DuiCellAddress{});
    const auto firstCellBounds = spreadsheet.VisibleCellBounds({0, 0});
    assert(firstCellBounds.has_value() && firstCellBounds->left == 42 && firstCellBounds->top == 24);
    assert(!spreadsheet.VisibleCellBounds({-1, 0}).has_value());
    assert(!spreadsheet.VisibleCellBounds({50, 50}).has_value());

    WorkbookModel emptyModel;
    emptyModel.SetDimensions(0, 0);
    controls::list::DuiSpreadsheet emptySpreadsheet;
    ClipboardMock emptyClipboard;
    emptySpreadsheet.SetWorkbookModel(&emptyModel);
    emptySpreadsheet.SetClipboard(&emptyClipboard);
    emptySpreadsheet.Layout({0, 0, 500, 300});
    assert(!emptySpreadsheet.VisibleCellBounds({0, 0}).has_value());
    assert(!emptySpreadsheet.BeginEdit());
    assert(!emptySpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::KeyDown, {}, 'C', core::modifier::Control)));
    assert(!emptySpreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Delete)));
    assert(!static_cast<core::Control&>(emptySpreadsheet).PerformAccessibilityAction(
        core::DuiAccessibilityAction::SetValue, "invalid"));
    const core::DuiAccessibilityData emptyAccessibility = emptySpreadsheet.Accessibility();
    assert(emptyAccessibility.role == core::DuiAccessibilityRole::Table
           && core::HasAccessibilityPattern(emptyAccessibility.patterns, core::DuiAccessibilityPattern::Grid)
           && core::HasAccessibilityPattern(emptyAccessibility.patterns, core::DuiAccessibilityPattern::Table)
           && !core::HasAccessibilityPattern(emptyAccessibility.patterns, core::DuiAccessibilityPattern::Value)
           && !core::HasAccessibilityPattern(emptyAccessibility.patterns, core::DuiAccessibilityPattern::Selection));

    WorkbookModel readOnlyModel;
    readOnlyModel.SetCellReadOnly(true);
    controls::list::DuiSpreadsheet readOnlySpreadsheet;
    TextInputMock readOnlyInput;
    readOnlySpreadsheet.SetWorkbookModel(&readOnlyModel);
    readOnlySpreadsheet.SetTextInput(&readOnlyInput);
    ClipboardMock readOnlyClipboard;
    readOnlyClipboard.text = "paste";
    readOnlySpreadsheet.SetClipboard(&readOnlyClipboard);
    readOnlySpreadsheet.Layout({0, 0, 500, 300});
    assert(readOnlyModel.SetCellText(readOnlySpreadsheet.ActiveWorksheet(), {0, 0}, "protected"));
    std::vector<DuiSpreadsheetWriteError> readOnlyErrors;
    readOnlySpreadsheet.SetWorkbookWriteErrorHandler(
        [&readOnlyErrors](const DuiSpreadsheetWriteError& error) { readOnlyErrors.push_back(error); });
    assert(!readOnlySpreadsheet.BeginEdit() && !readOnlyInput.visible);
    assert(readOnlySpreadsheet.Accessibility().readOnly);
    assert(!static_cast<core::Control&>(readOnlySpreadsheet).PerformAccessibilityAction(
        core::DuiAccessibilityAction::SetValue, "blocked"));
    assert(!readOnlySpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::KeyDown, {}, 'V', core::modifier::Control)));
    assert(readOnlyModel.CellText(readOnlySpreadsheet.ActiveWorksheet(), {0, 0}) == "protected");
    assert(!readOnlySpreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Delete)));
    assert(readOnlyModel.CellText(readOnlySpreadsheet.ActiveWorksheet(), {0, 0}) == "protected");
    assert(readOnlyErrors.size() == 2);
    for (const DuiSpreadsheetWriteError& error : readOnlyErrors)
        assert(error.result.code == DuiWorkbookWriteErrorCode::ReadOnly);

    const std::string complexText = "中文 العربية e\xCC\x81 \xF0\x9F\x98\x80";
    assert(model.SetCellText(spreadsheet.ActiveWorksheet(), {0, 0}, complexText));
    RecordingCanvas complexTextCanvas;
    spreadsheet.Paint(complexTextCanvas, spreadsheet.Bounds());
    assert(std::find(complexTextCanvas.drawnTexts.begin(), complexTextCanvas.drawnTexts.end(), complexText)
           != complexTextCanvas.drawnTexts.end());

    assert(model.SetCellText(spreadsheet.ActiveWorksheet(), {0, 1}, "1234.50"));
    assert(model.SetPresentation(spreadsheet.ActiveWorksheet(), {0, 1}, {"$1,234.50", DuiCellValueKind::Number, "currency"}));
    const DuiCellPresentation presentation = model.CellPresentation(spreadsheet.ActiveWorksheet(), {0, 1});
    assert(presentation.kind == DuiCellValueKind::Number && presentation.format == "currency");
    RecordingCanvas presentationCanvas;
    spreadsheet.Paint(presentationCanvas, spreadsheet.Bounds());
    assert(std::find(presentationCanvas.drawnTexts.begin(), presentationCanvas.drawnTexts.end(), "$1,234.50")
           != presentationCanvas.drawnTexts.end());
    spreadsheet.SetActiveCell({0, 1});
    assert(spreadsheet.Accessibility().value == "$1,234.50");
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Function2)));
    assert(input.text == "1234.50");
    spreadsheet.CancelEdit();
    spreadsheet.SetSelection({{0, 1}, {0, 1}});
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, 'C', core::modifier::Control)));
    assert(clipboard.text == "1234.50");

    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {150, 60})));
    assert(spreadsheet.ActiveCell().row == 1 && spreadsheet.ActiveCell().column == 1);
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {260, 84})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {260, 84})));
    assert(spreadsheet.Selection().Normalized().last.row >= 2 && spreadsheet.Selection().Normalized().last.column >= 2);

    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 55})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {10, 103})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {10, 103})));
    const DuiCellRange rowSelection = spreadsheet.Selection().Normalized();
    assert(rowSelection.first.row == 1 && rowSelection.last.row >= 3);
    assert(rowSelection.first.column == 0 && rowSelection.last.column == 999);

    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {150, 10})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {260, 10})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {260, 10})));
    const DuiCellRange columnSelection = spreadsheet.Selection().Normalized();
    assert(columnSelection.first.column == 1 && columnSelection.last.column >= 2);
    assert(columnSelection.first.row == 0 && columnSelection.last.row == 99999);

    const core::Color activeColor = theme.Get(core::ThemeSlot::SpreadsheetActiveCell);
    const core::Color selectedHeaderGridLine{219, 219, 219, 255};
    RecordingCanvas columnSelectionCanvas;
    spreadsheet.Paint(columnSelectionCanvas, spreadsheet.Bounds());
    const auto columnTitle = std::find(columnSelectionCanvas.drawnTexts.begin(), columnSelectionCanvas.drawnTexts.end(), "B");
    assert(columnTitle != columnSelectionCanvas.drawnTexts.end());
    assert(columnSelectionCanvas.drawnTextStyles[static_cast<std::size_t>(columnTitle - columnSelectionCanvas.drawnTexts.begin())].color == activeColor);
    assert(std::any_of(columnSelectionCanvas.fills.begin(), columnSelectionCanvas.fills.end(), [selectedHeaderGridLine](const RecordingCanvas::Fill& fill)
    {
        return fill.color == selectedHeaderGridLine && fill.bounds.Width() == 1 && fill.bounds.Height() == 24;
    }));
    assert(std::any_of(columnSelectionCanvas.fills.begin(), columnSelectionCanvas.fills.end(), [activeColor, &spreadsheet](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.Width() == 6 && fill.bounds.Height() == 6
            && fill.bounds.top == spreadsheet.Bounds().top + 24 - 4;
    }));
    assert(!std::any_of(columnSelectionCanvas.fills.begin(), columnSelectionCanvas.fills.end(), [activeColor, &spreadsheet](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.Width() > 100 && fill.bounds.Height() == 3
            && fill.bounds.bottom >= spreadsheet.Bounds().bottom - 3;
    }));

    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 55})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {10, 103})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {10, 103})));
    RecordingCanvas rowSelectionCanvas;
    spreadsheet.Paint(rowSelectionCanvas, spreadsheet.Bounds());
    const auto rowTitle = std::find(rowSelectionCanvas.drawnTexts.begin(), rowSelectionCanvas.drawnTexts.end(), "2");
    assert(rowTitle != rowSelectionCanvas.drawnTexts.end());
    assert(rowSelectionCanvas.drawnTextStyles[static_cast<std::size_t>(rowTitle - rowSelectionCanvas.drawnTexts.begin())].color == activeColor);
    assert(std::any_of(rowSelectionCanvas.fills.begin(), rowSelectionCanvas.fills.end(), [selectedHeaderGridLine](const RecordingCanvas::Fill& fill)
    {
        return fill.color == selectedHeaderGridLine && fill.bounds.Width() == 42 && fill.bounds.Height() == 1;
    }));
    assert(std::any_of(rowSelectionCanvas.fills.begin(), rowSelectionCanvas.fills.end(), [activeColor, &spreadsheet](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.Width() == 6 && fill.bounds.Height() == 6
            && fill.bounds.left == spreadsheet.Bounds().left + 42 - 4;
    }));
    assert(!std::any_of(rowSelectionCanvas.fills.begin(), rowSelectionCanvas.fills.end(), [activeColor, &spreadsheet](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.Width() == 3 && fill.bounds.Height() > 100
            && fill.bounds.right >= spreadsheet.Bounds().right - 3;
    }));

    spreadsheet.SetSelection({{1, 1}, {2, 2}});
    RecordingCanvas selectionCanvas;
    spreadsheet.Paint(selectionCanvas, spreadsheet.Bounds());
    const core::Color selectionColor = theme.Get(core::ThemeSlot::SpreadsheetSelection);
    const core::Color selectionGridLine{208, 208, 208, 255};
    assert(std::count_if(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [white](const RecordingCanvas::Fill& fill)
    {
        return fill.color == white && fill.bounds.Width() == 96 && fill.bounds.Height() == 24;
    }) == 1);
    assert(std::count_if(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [selectionColor, &spreadsheet](const RecordingCanvas::Fill& fill)
    {
        return fill.color == selectionColor && fill.bounds.Width() == 96 && fill.bounds.Height() == 24
            && fill.bounds.top >= spreadsheet.Bounds().top + 24;
    }) == 3);
    assert(std::any_of(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [selectionColor, &spreadsheet](const RecordingCanvas::Fill& fill)
    {
        return fill.color == selectionColor && fill.bounds.Width() == 96 && fill.bounds.Height() == 24
            && fill.bounds.top == spreadsheet.Bounds().top;
    }));
    assert(!selectionCanvas.filledPaths.empty());
    const core::Color selectAllTriangleColor{128, 128, 128, 255};
    assert(std::find(selectionCanvas.pathFillColors.begin(), selectionCanvas.pathFillColors.end(),
                     selectAllTriangleColor) != selectionCanvas.pathFillColors.end());
    assert(std::any_of(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [selectionGridLine](const RecordingCanvas::Fill& fill)
    {
        return fill.color == selectionGridLine
            && ((fill.bounds.Width() == 1 && fill.bounds.Height() == 24)
                || (fill.bounds.Width() == 96 && fill.bounds.Height() == 1));
    }));
    const auto activeStrokes = std::count_if(selectionCanvas.strokeColors.begin(), selectionCanvas.strokeColors.end(),
                                             [activeColor](core::Color color) { return color == activeColor; });
    assert(activeStrokes == 0);
    assert(std::count_if(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.Width() > 100 && fill.bounds.Height() == 3;
    }) == 2);
    assert(std::any_of(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.left == 136 && fill.bounds.top == 46
            && fill.bounds.Width() == 195 && fill.bounds.Height() == 3;
    }));
    assert(std::count_if(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.Width() == 3 && fill.bounds.Height() > 20;
    }) == 2);
    assert(std::any_of(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.left == 136 && fill.bounds.top == 46
            && fill.bounds.Width() == 3 && fill.bounds.Height() == 51;
    }));
    assert(std::count_if(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.Width() == 96 && fill.bounds.Height() == 2;
    }) == 2);
    assert(std::count_if(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.Width() == 2 && fill.bounds.Height() == 24;
    }) == 2);
    assert(std::any_of(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.Width() == 6 && fill.bounds.Height() == 6;
    }));
    assert(std::any_of(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [white](const RecordingCanvas::Fill& fill)
    {
        return fill.color == white && fill.bounds.Width() == 8 && fill.bounds.Height() == 8;
    }));
    const int frozenBoundary = 42 + 96;
    const int frozenRowBoundary = 24 + 24;
    assert(std::any_of(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.left == frozenBoundary - 1
            && fill.bounds.Width() == 1 && fill.bounds.Height() > 100;
    }));
    assert(std::any_of(selectionCanvas.fills.begin(), selectionCanvas.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.top == frozenRowBoundary - 1
            && fill.bounds.Width() > 100 && fill.bounds.Height() == 1;
    }));

    spreadsheet.SetSelection({{1, 0}, {2, 1}});
    RecordingCanvas crossingCanvas;
    spreadsheet.Paint(crossingCanvas, spreadsheet.Bounds());
    assert(!std::any_of(crossingCanvas.fills.begin(), crossingCanvas.fills.end(), [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.Width() == 3
            && fill.bounds.left < frozenBoundary && fill.bounds.right > frozenBoundary;
    }));

    WorkbookModel moveModel;
    controls::list::DuiSpreadsheet moveSpreadsheet;
    moveSpreadsheet.SetWorkbookModel(&moveModel);
    moveSpreadsheet.Layout({0, 0, 500, 300});
    const DuiWorksheetId moveSheet = moveSpreadsheet.ActiveWorksheet();
    assert(moveModel.SetCellText(moveSheet, {1, 1}, "A"));
    assert(moveModel.SetCellText(moveSheet, {1, 2}, "B"));
    assert(moveModel.SetCellText(moveSheet, {2, 1}, "C"));
    assert(moveModel.SetCellText(moveSheet, {2, 2}, "D"));
    assert(moveModel.SetCellText(moveSheet, {2, 3}, "X"));
    assert(moveModel.SetCellText(moveSheet, {3, 2}, "Y"));
    assert(moveModel.SetCellText(moveSheet, {3, 3}, "Z"));
    moveSpreadsheet.SetSelection({{1, 1}, {2, 2}});
    assert(moveSpreadsheet.HitTest({180, 47}) == &moveSpreadsheet);
    assert(moveSpreadsheet.PointerCursor() == core::DuiPointerCursor::Move);
    assert(moveSpreadsheet.HitTest({180, 60}) == &moveSpreadsheet);
    assert(moveSpreadsheet.PointerCursor() == core::DuiPointerCursor::Arrow);
    assert(moveSpreadsheet.HitTest({329, 95}) == &moveSpreadsheet);
    assert(moveSpreadsheet.PointerCursor() == core::DuiPointerCursor::Arrow);
    assert(moveSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {180, 47})));
    assert(moveSpreadsheet.Captured());
    assert(moveSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {276, 84})));
    assert(moveSpreadsheet.Selection().Normalized() == (DuiCellRange{{1, 1}, {2, 2}}));
    assert(moveModel.CellText(moveSheet, {1, 1}) == "A");
    assert(moveModel.CellText(moveSheet, {2, 3}) == "X");
    RecordingCanvas movingSelectionCanvas;
    moveSpreadsheet.Paint(movingSelectionCanvas, moveSpreadsheet.Bounds());
    const core::Color movePreviewColor{128, 128, 128, 255};
    assert(std::count_if(movingSelectionCanvas.fills.begin(), movingSelectionCanvas.fills.end(),
                         [movePreviewColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == movePreviewColor
            && ((fill.bounds.Width() == 2 && fill.bounds.Height() == 3)
                || (fill.bounds.Width() == 3 && fill.bounds.Height() == 2));
    }) > 20);
    assert(std::any_of(movingSelectionCanvas.fills.begin(), movingSelectionCanvas.fills.end(),
                       [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.left == 136 && fill.bounds.top == 46
            && fill.bounds.Width() == 195 && fill.bounds.Height() == 3;
    }));
    assert(!std::any_of(movingSelectionCanvas.fills.begin(), movingSelectionCanvas.fills.end(),
                        [activeColor](const RecordingCanvas::Fill& fill)
    {
        return fill.color == activeColor && fill.bounds.left == 232 && fill.bounds.top == 70
            && fill.bounds.Width() == 195 && fill.bounds.Height() == 3;
    }));
    assert(moveSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {276, 84})));
    assert(!moveSpreadsheet.Captured());
    assert(moveSpreadsheet.Selection().Normalized() == (DuiCellRange{{2, 2}, {3, 3}}));
    assert(moveSpreadsheet.ActiveCell() == (DuiCellAddress{3, 3}));
    assert(moveModel.CellText(moveSheet, {1, 1}).empty());
    assert(moveModel.CellText(moveSheet, {1, 2}).empty());
    assert(moveModel.CellText(moveSheet, {2, 1}).empty());
    assert(moveModel.CellText(moveSheet, {2, 2}) == "A");
    assert(moveModel.CellText(moveSheet, {2, 3}) == "B");
    assert(moveModel.CellText(moveSheet, {3, 2}) == "C");
    assert(moveModel.CellText(moveSheet, {3, 3}) == "D");
    moveSpreadsheet.Undo();
    assert(moveModel.CellText(moveSheet, {1, 1}) == "A");
    assert(moveModel.CellText(moveSheet, {1, 2}) == "B");
    assert(moveModel.CellText(moveSheet, {2, 1}) == "C");
    assert(moveModel.CellText(moveSheet, {2, 2}) == "D");
    assert(moveModel.CellText(moveSheet, {2, 3}) == "X");
    assert(moveModel.CellText(moveSheet, {3, 2}) == "Y");
    assert(moveModel.CellText(moveSheet, {3, 3}) == "Z");
    moveSpreadsheet.Redo();
    assert(moveModel.CellText(moveSheet, {2, 2}) == "A");
    assert(moveModel.CellText(moveSheet, {2, 3}) == "B");
    assert(moveModel.CellText(moveSheet, {3, 2}) == "C");
    assert(moveModel.CellText(moveSheet, {3, 3}) == "D");
    int rejectedMoveErrors{};
    moveSpreadsheet.SetWorkbookWriteErrorHandler(
        [&rejectedMoveErrors](const DuiSpreadsheetWriteError&) { ++rejectedMoveErrors; });
    moveModel.SetCellWriteFailureAfter(2);
    moveSpreadsheet.Undo();
    assert(rejectedMoveErrors == 1);
    assert(moveModel.CellText(moveSheet, {1, 1}).empty());
    assert(moveModel.CellText(moveSheet, {2, 2}) == "A");
    assert(moveModel.CellText(moveSheet, {2, 3}) == "B");
    assert(moveModel.CellText(moveSheet, {3, 2}) == "C");
    assert(moveModel.CellText(moveSheet, {3, 3}) == "D");
    moveModel.SetCellWriteFailureAfter(std::nullopt);
    moveSpreadsheet.Undo();
    assert(moveModel.CellText(moveSheet, {1, 1}) == "A");
    assert(moveModel.CellText(moveSheet, {2, 2}) == "D");
    moveSpreadsheet.Redo();
    assert(moveModel.CellText(moveSheet, {1, 1}).empty());
    assert(moveModel.CellText(moveSheet, {2, 2}) == "A");
    moveModel.SetCellReadOnly(true);
    assert(moveSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {276, 71})));
    assert(moveSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {372, 108})));
    assert(!moveSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {372, 108})));
    assert(rejectedMoveErrors == 2);
    assert(moveSpreadsheet.Selection().Normalized() == (DuiCellRange{{2, 2}, {3, 3}}));
    assert(moveModel.CellText(moveSheet, {2, 2}) == "A");
    assert(moveModel.CellText(moveSheet, {2, 3}) == "B");
    assert(moveModel.CellText(moveSheet, {3, 2}) == "C");
    assert(moveModel.CellText(moveSheet, {3, 3}) == "D");
    moveModel.SetCellReadOnly(false);
    assert(moveSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {276, 71})));
    assert(moveSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {372, 108})));
    assert(moveSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerCancel, {372, 108})));
    assert(!moveSpreadsheet.Captured());
    assert(moveSpreadsheet.Selection().Normalized() == (DuiCellRange{{2, 2}, {3, 3}}));
    assert(moveModel.CellText(moveSheet, {2, 2}) == "A");

    spreadsheet.SetSelection({{1, 1}, {1, 1}});
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerWheel, {200, 100}, {}, core::modifier::Shift, {}, -1)));
    RecordingCanvas scrolledCanvas;
    spreadsheet.Paint(scrolledCanvas, spreadsheet.Bounds());
    assert(!std::any_of(scrolledCanvas.fills.begin(), scrolledCanvas.fills.end(), [white, selectionColor](const RecordingCanvas::Fill& fill)
    {
        return (fill.color == white || fill.color == selectionColor)
            && fill.bounds.top >= 24 && fill.bounds.bottom <= 258
            && fill.bounds.left < frozenBoundary && fill.bounds.right > frozenBoundary;
    }));

    assert(spreadsheet.FrozenRowCount() == 1 && spreadsheet.FrozenColumnCount() == 1);
    int frozenRowCountReads{};
    int frozenColumnCountReads{};
    model.SetRowCountReadHandler([&frozenRowCountReads] { ++frozenRowCountReads; });
    model.SetColumnCountReadHandler([&frozenColumnCountReads] { ++frozenColumnCountReads; });
    assert(spreadsheet.FrozenRowCount() == 1 && spreadsheet.FrozenColumnCount() == 1);
    assert(frozenRowCountReads == 0 && frozenColumnCountReads == 0);
    model.SetRowCountReadHandler({});
    model.SetColumnCountReadHandler({});
    spreadsheet.SetSelection({{4, 3}, {5, 4}});
    spreadsheet.SetFrozenPanes(4, 3);
    assert(spreadsheet.FrozenRowCount() == 4 && spreadsheet.FrozenColumnCount() == 3);
    spreadsheet.SetFrozenPanes(4, 0);
    assert(spreadsheet.FrozenRowCount() == 4 && spreadsheet.FrozenColumnCount() == 0);
    spreadsheet.SetFrozenPanes(0, 3);
    assert(spreadsheet.FrozenRowCount() == 0 && spreadsheet.FrozenColumnCount() == 3);
    spreadsheet.ClearFrozenPanes();
    assert(spreadsheet.FrozenRowCount() == 0 && spreadsheet.FrozenColumnCount() == 0);
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 10})));
    const DuiCellAddress allFirst{0, 0};
    const DuiCellAddress allLast{99999, 999};
    assert(spreadsheet.Selection().Normalized().first == allFirst);
    assert(spreadsheet.Selection().Normalized().last == allLast);
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerWheel, {200, 100}, {}, core::modifier::Shift, {}, 1)));

    const int originalColumnWidth = model.ColumnWidth(spreadsheet.ActiveWorksheet(), 0);
    assert(spreadsheet.HitTest({140, 60}) == &spreadsheet);
    assert(spreadsheet.PointerCursor() == core::DuiPointerCursor::Arrow);
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {140, 60})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {160, 60})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {160, 60})));
    assert(model.ColumnWidth(spreadsheet.ActiveWorksheet(), 0) == originalColumnWidth);
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {100, 60})));
    assert(spreadsheet.Captured());
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerCancel, {100, 60})));
    assert(!spreadsheet.Captured());
    assert(!spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {120, 70})));
    assert(spreadsheet.HitTest({140, 10}) == &spreadsheet);
    assert(spreadsheet.PointerCursor() == core::DuiPointerCursor::ResizeHorizontal);
    assert(spreadsheet.HitTest({100, 60}) == &spreadsheet);
    assert(spreadsheet.PointerCursor() == core::DuiPointerCursor::Arrow);
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {140, 10})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {160, 10})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {160, 10})));
    assert(model.ColumnWidth(spreadsheet.ActiveWorksheet(), 0) > 96);
    const int originalRowHeight = model.RowHeight(spreadsheet.ActiveWorksheet(), 0);
    assert(spreadsheet.HitTest({100, 50}) == &spreadsheet);
    assert(spreadsheet.PointerCursor() == core::DuiPointerCursor::Arrow);
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {100, 50})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {100, 66})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {100, 66})));
    assert(model.RowHeight(spreadsheet.ActiveWorksheet(), 0) == originalRowHeight);
    assert(spreadsheet.HitTest({10, 50}) == &spreadsheet);
    assert(spreadsheet.PointerCursor() == core::DuiPointerCursor::ResizeVertical);
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 50})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {10, 66})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {10, 66})));
    assert(model.RowHeight(spreadsheet.ActiveWorksheet(), 0) > 24);
    spreadsheet.Undo();
    assert(model.RowHeight(spreadsheet.ActiveWorksheet(), 0) == 24);
    spreadsheet.Undo();
    assert(model.ColumnWidth(spreadsheet.ActiveWorksheet(), 0) == 96);
    spreadsheet.Redo();
    spreadsheet.Redo();
    assert(model.RowHeight(spreadsheet.ActiveWorksheet(), 0) > 24);

    spreadsheet.SetActiveCell({2, 3});
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDoubleClick, {350, 100})));
    assert(spreadsheet.Editing() && !input.borderVisible);
    spreadsheet.CancelEdit();
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Function2)));
    assert(spreadsheet.Editing() && !input.borderVisible);
    spreadsheet.CancelEdit();
    core::Event textEvent;
    textEvent.type = core::EventType::TextInput;
    textEvent.text = "alpha";
    assert(spreadsheet.OnEvent(textEvent));
    assert(spreadsheet.Editing() && input.visible && input.text == "alpha");
    input.text = "beta";
    RecordingCanvas proxyEditCanvas;
    spreadsheet.Paint(proxyEditCanvas, spreadsheet.Bounds());
    assert(std::find(proxyEditCanvas.drawnTexts.begin(), proxyEditCanvas.drawnTexts.end(), "beta")
           != proxyEditCanvas.drawnTexts.end());
    input.Submit();
    assert(!spreadsheet.Editing());
    assert(spreadsheet.ActiveCell() == (DuiCellAddress{3, 3}));
    assert(model.CellText(spreadsheet.ActiveWorksheet(), {2, 3}) == "beta");
    spreadsheet.Undo();
    assert(model.CellText(spreadsheet.ActiveWorksheet(), {2, 3}).empty());
    spreadsheet.Redo();
    assert(model.CellText(spreadsheet.ActiveWorksheet(), {2, 3}) == "beta");

    WorkbookModel multiEditModel;
    controls::list::DuiSpreadsheet multiEditSpreadsheet;
    TextInputMock multiEditInput;
    multiEditSpreadsheet.SetWorkbookModel(&multiEditModel);
    multiEditSpreadsheet.SetTextInput(&multiEditInput);
    multiEditSpreadsheet.Layout({0, 0, 500, 300});
    assert(multiEditSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {180, 60})));
    assert(multiEditSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {276, 108})));
    assert(multiEditSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {276, 108})));
    const DuiCellRange multiEditRange{{1, 1}, {3, 2}};
    assert(multiEditSpreadsheet.Selection().Normalized() == multiEditRange);
    const auto typeAndSubmit = [&multiEditSpreadsheet, &multiEditInput](std::string text)
    {
        core::Event event;
        event.type = core::EventType::TextInput;
        event.text = std::move(text);
        assert(multiEditSpreadsheet.OnEvent(event));
        assert(multiEditSpreadsheet.Editing());
        multiEditInput.Submit();
        assert(!multiEditSpreadsheet.Editing());
    };
    typeAndSubmit("1");
    assert(multiEditModel.CellText(multiEditSpreadsheet.ActiveWorksheet(), {1, 1}) == "1");
    assert(multiEditSpreadsheet.ActiveCell() == (DuiCellAddress{2, 1}));
    assert(multiEditSpreadsheet.Selection().Normalized() == multiEditRange);
    const auto firstEditBounds = multiEditSpreadsheet.VisibleCellBounds({1, 1});
    const auto nextEditBounds = multiEditSpreadsheet.VisibleCellBounds({2, 1});
    assert(firstEditBounds.has_value() && nextEditBounds.has_value());
    RecordingCanvas multiEditCanvas;
    multiEditSpreadsheet.Paint(multiEditCanvas, multiEditSpreadsheet.Bounds());
    assert(std::any_of(multiEditCanvas.fills.begin(), multiEditCanvas.fills.end(),
                       [white, nextEditBounds](const RecordingCanvas::Fill& fill)
    {
        return fill.color == white && fill.bounds == *nextEditBounds;
    }));
    assert(!std::any_of(multiEditCanvas.fills.begin(), multiEditCanvas.fills.end(),
                        [white, firstEditBounds](const RecordingCanvas::Fill& fill)
    {
        return fill.color == white && fill.bounds == *firstEditBounds;
    }));
    typeAndSubmit("2");
    assert(multiEditModel.CellText(multiEditSpreadsheet.ActiveWorksheet(), {2, 1}) == "2");
    assert(multiEditSpreadsheet.ActiveCell() == (DuiCellAddress{3, 1}));
    typeAndSubmit("3");
    assert(multiEditModel.CellText(multiEditSpreadsheet.ActiveWorksheet(), {3, 1}) == "3");
    assert(multiEditSpreadsheet.ActiveCell() == (DuiCellAddress{1, 2}));
    assert(multiEditSpreadsheet.Selection().Normalized() == multiEditRange);

    assert(multiEditSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {276, 108})));
    assert(multiEditSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {180, 60})));
    assert(multiEditSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {180, 60})));
    typeAndSubmit("reverse");
    assert(multiEditModel.CellText(multiEditSpreadsheet.ActiveWorksheet(), {3, 2}) == "reverse");
    assert(multiEditSpreadsheet.Selection().Normalized() == multiEditRange);

    WorkbookModel rejectingModel;
    controls::list::DuiSpreadsheet rejectingSpreadsheet;
    TextInputMock rejectingInput;
    rejectingSpreadsheet.SetWorkbookModel(&rejectingModel);
    rejectingSpreadsheet.SetTextInput(&rejectingInput);
    rejectingSpreadsheet.Layout({0, 0, 500, 300});
    rejectingSpreadsheet.SetActiveCell({2, 3});
    assert(rejectingSpreadsheet.BeginEdit());
    rejectingInput.text = "rejected value";
    rejectingModel.SetCellWritesAllowed(false);
    rejectingSpreadsheet.CommitEdit();
    assert(rejectingSpreadsheet.Editing() && rejectingInput.visible);
    assert(rejectingInput.text == "rejected value");
    assert(rejectingModel.CellText(rejectingSpreadsheet.ActiveWorksheet(), {2, 3}).empty());
    assert(rejectingSpreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Enter)));
    assert(rejectingSpreadsheet.Editing() && rejectingSpreadsheet.ActiveCell() == (DuiCellAddress{2, 3}));
    assert(rejectingSpreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Tab)));
    assert(rejectingSpreadsheet.Editing() && rejectingSpreadsheet.ActiveCell() == (DuiCellAddress{2, 3}));
    assert(rejectingSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {150, 75})));
    assert(rejectingSpreadsheet.Editing() && rejectingSpreadsheet.ActiveCell() == (DuiCellAddress{2, 3}));
    assert(!rejectingSpreadsheet.SetActiveWorksheet(2));
    assert(rejectingSpreadsheet.ActiveWorksheet() == 1 && rejectingSpreadsheet.Editing());
    assert(!rejectingSpreadsheet.RemoveActiveWorksheet());
    assert(rejectingModel.WorksheetCount() == 2 && rejectingSpreadsheet.Editing());
    rejectingModel.SetCellWritesAllowed(true);
    assert(rejectingSpreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Enter)));
    assert(!rejectingSpreadsheet.Editing());
    assert(rejectingSpreadsheet.ActiveCell() == (DuiCellAddress{3, 3}));
    assert(rejectingModel.CellText(rejectingSpreadsheet.ActiveWorksheet(), {2, 3}) == "rejected value");
    rejectingModel.SetCellWritesAllowed(false);
    rejectingSpreadsheet.Undo();
    assert(rejectingModel.CellText(rejectingSpreadsheet.ActiveWorksheet(), {2, 3}) == "rejected value");
    rejectingModel.SetCellWritesAllowed(true);
    rejectingSpreadsheet.Undo();
    assert(rejectingModel.CellText(rejectingSpreadsheet.ActiveWorksheet(), {2, 3}).empty());
    rejectingModel.SetCellWritesAllowed(false);
    rejectingSpreadsheet.Redo();
    assert(rejectingModel.CellText(rejectingSpreadsheet.ActiveWorksheet(), {2, 3}).empty());
    rejectingModel.SetCellWritesAllowed(true);
    rejectingSpreadsheet.Redo();
    assert(rejectingModel.CellText(rejectingSpreadsheet.ActiveWorksheet(), {2, 3}) == "rejected value");
    assert(rejectingSpreadsheet.BeginEdit());
    rejectingInput.text = "pending before add";
    rejectingModel.SetCellWritesAllowed(false);
    assert(rejectingSpreadsheet.AddWorksheet("Blocked") == 0);
    assert(rejectingModel.WorksheetCount() == 2 && rejectingSpreadsheet.ActiveWorksheet() == 1);
    assert(rejectingSpreadsheet.Editing() && rejectingInput.visible);
    rejectingModel.SetCellWritesAllowed(true);
    const DuiWorksheetId addedAfterCommit = rejectingSpreadsheet.AddWorksheet("Allowed");
    assert(addedAfterCommit != 0 && rejectingSpreadsheet.ActiveWorksheet() == addedAfterCommit);
    assert(!rejectingSpreadsheet.Editing() && rejectingModel.WorksheetCount() == 3);
    assert(rejectingModel.CellText(1, {3, 3}) == "pending before add");

    WorkbookModel batchModel;
    controls::list::DuiSpreadsheet batchSpreadsheet;
    ClipboardMock batchClipboard;
    batchSpreadsheet.SetWorkbookModel(&batchModel);
    batchSpreadsheet.SetClipboard(&batchClipboard);
    batchSpreadsheet.SetSelection({{0, 0}, {0, 0}});
    assert(batchModel.SetCellText(batchSpreadsheet.ActiveWorksheet(), {0, 0}, "delete value"));
    batchModel.SetCellWritesAllowed(false);
    assert(!batchSpreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Delete)));
    assert(batchModel.CellText(batchSpreadsheet.ActiveWorksheet(), {0, 0}) == "delete value");
    batchModel.SetCellWritesAllowed(true);
    assert(batchSpreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Delete)));
    assert(batchModel.CellText(batchSpreadsheet.ActiveWorksheet(), {0, 0}).empty());
    assert(batchModel.SetCellText(batchSpreadsheet.ActiveWorksheet(), {0, 0}, "cut value"));
    batchModel.SetCellWritesAllowed(false);
    assert(!batchSpreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, 'X', core::modifier::Control)));
    assert(batchModel.CellText(batchSpreadsheet.ActiveWorksheet(), {0, 0}) == "cut value");
    batchClipboard.text = "paste value";
    assert(!batchSpreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, 'V', core::modifier::Control)));
    assert(batchModel.CellText(batchSpreadsheet.ActiveWorksheet(), {0, 0}) == "cut value");
    batchModel.SetCellWritesAllowed(true);
    assert(batchSpreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, 'V', core::modifier::Control)));
    assert(batchModel.CellText(batchSpreadsheet.ActiveWorksheet(), {0, 0}) == "paste value");

    WorkbookModel diagnosticModel;
    controls::list::DuiSpreadsheet diagnosticSpreadsheet;
    TextInputMock diagnosticInput;
    std::optional<DuiSpreadsheetWriteError> diagnosticError;
    diagnosticSpreadsheet.SetWorkbookModel(&diagnosticModel);
    diagnosticSpreadsheet.SetTextInput(&diagnosticInput);
    diagnosticSpreadsheet.SetWorkbookWriteErrorHandler(
        [&diagnosticError](const DuiSpreadsheetWriteError& error) { diagnosticError = error; });
    diagnosticSpreadsheet.Layout({0, 0, 500, 300});
    diagnosticSpreadsheet.SetActiveCell({4, 5});
    diagnosticModel.SetCellWriteFailure(
        DuiWorkbookWriteResult{DuiWorkbookWriteErrorCode::ReadOnly, "单元格受保护"});
    assert(diagnosticSpreadsheet.BeginEdit());
    diagnosticInput.text = "blocked";
    diagnosticSpreadsheet.CommitEdit();
    assert(diagnosticSpreadsheet.Editing());
    assert(diagnosticError.has_value());
    assert(diagnosticError->worksheet == diagnosticSpreadsheet.ActiveWorksheet());
    assert(diagnosticError->cell == (DuiCellAddress{4, 5}));
    assert(diagnosticError->result.code == DuiWorkbookWriteErrorCode::ReadOnly);
    assert(diagnosticError->result.message == "单元格受保护");

    WorkbookModel dimensionDiagnosticModel;
    controls::list::DuiSpreadsheet dimensionDiagnosticSpreadsheet;
    std::optional<DuiSpreadsheetWriteError> dimensionDiagnosticError;
    dimensionDiagnosticSpreadsheet.SetWorkbookModel(&dimensionDiagnosticModel);
    dimensionDiagnosticSpreadsheet.Layout({0, 0, 500, 300});
    dimensionDiagnosticSpreadsheet.SetWorkbookWriteErrorHandler(
        [&dimensionDiagnosticError](const DuiSpreadsheetWriteError& error) { dimensionDiagnosticError = error; });
    dimensionDiagnosticModel.SetDimensionWriteFailure(
        DuiWorkbookWriteResult{DuiWorkbookWriteErrorCode::ReadOnly, "列宽受保护"});
    assert(dimensionDiagnosticSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {140, 10})));
    assert(!dimensionDiagnosticSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {180, 10})));
    assert(dimensionDiagnosticError.has_value());
    assert(dimensionDiagnosticError->operation == DuiSpreadsheetWriteOperation::ColumnWidth);
    assert(dimensionDiagnosticError->cell == (DuiCellAddress{0, 0}));
    assert(dimensionDiagnosticError->result.code == DuiWorkbookWriteErrorCode::ReadOnly);
    assert(dimensionDiagnosticError->result.message == "列宽受保护");

    WorkbookModel dimensionDestructionModel;
    dimensionDestructionModel.SetDimensionWriteFailure(
        DuiWorkbookWriteResult{DuiWorkbookWriteErrorCode::ReadOnly, "列宽受保护"});
    auto dimensionDestructionSpreadsheet = std::make_unique<controls::list::DuiSpreadsheet>();
    dimensionDestructionSpreadsheet->SetWorkbookModel(&dimensionDestructionModel);
    dimensionDestructionSpreadsheet->Layout({0, 0, 500, 300});
    dimensionDestructionSpreadsheet->SetWorkbookWriteErrorHandler(
        [&dimensionDestructionSpreadsheet](const DuiSpreadsheetWriteError&)
        {
            dimensionDestructionSpreadsheet.reset();
        });
    assert(dimensionDestructionSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {140, 10})));
    assert(dimensionDestructionSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerMove, {180, 10})));
    assert(dimensionDestructionSpreadsheet == nullptr);

    WorkbookModel replacementModel;
    diagnosticSpreadsheet.SetWorkbookWriteErrorHandler(
        [&diagnosticSpreadsheet, &replacementModel](const DuiSpreadsheetWriteError&)
        {
            diagnosticSpreadsheet.SetWorkbookModel(&replacementModel);
        });
    diagnosticSpreadsheet.CommitEdit();
    assert(diagnosticSpreadsheet.WorkbookModel() == &replacementModel);
    assert(!diagnosticSpreadsheet.Editing());

    WorkbookModel destructionModel;
    destructionModel.SetCellWriteFailure(
        DuiWorkbookWriteResult{DuiWorkbookWriteErrorCode::ReadOnly, "单元格受保护"});
    TextInputMock destructionInput;
    auto destructionSpreadsheet = std::make_unique<controls::list::DuiSpreadsheet>();
    destructionSpreadsheet->SetWorkbookModel(&destructionModel);
    destructionSpreadsheet->SetTextInput(&destructionInput);
    destructionSpreadsheet->Layout({0, 0, 500, 300});
    destructionSpreadsheet->SetActiveCell({4, 5});
    destructionSpreadsheet->SetWorkbookWriteErrorHandler(
        [&destructionSpreadsheet](const DuiSpreadsheetWriteError&) { destructionSpreadsheet.reset(); });
    assert(destructionSpreadsheet->BeginEdit());
    destructionInput.text = "blocked";
    destructionSpreadsheet->CommitEdit();
    assert(!destructionSpreadsheet);

    spreadsheet.SetSelection({{2, 3}, {2, 3}});
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, 'C', core::modifier::Control)));
    assert(clipboard.text == "beta");
    clipboard.text = "one\ttwo\r\nthree\tfour";
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, 'V', core::modifier::Control)));
    assert(model.CellText(spreadsheet.ActiveWorksheet(), {3, 4}) == "four");

    const DuiWorksheetId third = spreadsheet.AddWorksheet("Data");
    assert(third != 0 && spreadsheet.ActiveWorksheet() == third);
    assert(spreadsheet.RenameActiveWorksheet("Summary"));
    assert(model.WorksheetName(third) == "Summary");
    assert(spreadsheet.RemoveActiveWorksheet());
    assert(model.WorksheetCount() == 2);
    assert(spreadsheet.Accessibility().role == core::DuiAccessibilityRole::Table);

    spreadsheet.SetActiveCell({6, 5});
    assert(model.SetCellText(spreadsheet.ActiveWorksheet(), spreadsheet.ActiveCell(), "Accessible"));
    const auto spreadsheetAccessibility = spreadsheet.Accessibility();
    assert(spreadsheetAccessibility.value == "Accessible");
    assert(core::HasAccessibilityPattern(spreadsheetAccessibility.patterns,
                                         core::DuiAccessibilityPattern::Value));
    assert(core::HasAccessibilityPattern(spreadsheetAccessibility.patterns,
                                         core::DuiAccessibilityPattern::Selection));
    assert(core::HasAccessibilityPattern(spreadsheetAccessibility.patterns,
                                         core::DuiAccessibilityPattern::Grid));
    assert(core::HasAccessibilityPattern(spreadsheetAccessibility.patterns,
                                         core::DuiAccessibilityPattern::Table));
    assert(spreadsheetAccessibility.description.find("F7") != std::string::npos);
    assert(static_cast<core::Control&>(spreadsheet).PerformAccessibilityAction(
        core::DuiAccessibilityAction::SetValue, "Updated by UIA"));
    assert(model.CellText(spreadsheet.ActiveWorksheet(), spreadsheet.ActiveCell()) == "Updated by UIA");
    spreadsheet.Undo();
    assert(model.CellText(spreadsheet.ActiveWorksheet(), spreadsheet.ActiveCell()) == "Accessible");

    spreadsheet.SetActiveCell({0, 0});
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Function2)));
    assert(spreadsheet.Editing() && input.visible);
    spreadsheet.ReloadFromModel();
    assert(!spreadsheet.Editing() && !input.visible);

    const int resizeBoundary = spreadsheet.Bounds().left + 42 + model.ColumnWidth(spreadsheet.ActiveWorksheet(), 0);
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {resizeBoundary, 10})));
    WorkbookModel replacement;
    spreadsheet.SetWorkbookModel(&replacement);
    assert(replacement.SetRowHeight(replacement.WorksheetIdAt(0), 0, 60));
    replacement.NotifyChanged();
    assert(spreadsheet.HitTest({10, 82}) == &spreadsheet);
    assert(spreadsheet.PointerCursor() == core::DuiPointerCursor::ResizeVertical);
    assert(!spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {resizeBoundary + 20, 10})));
    assert(replacement.ColumnWidth(spreadsheet.ActiveWorksheet(), 0) == 96);

    spreadsheet.SetActiveCell({0, 0});
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Function2)));
    assert(spreadsheet.Editing());
    assert(spreadsheet.RemoveActiveWorksheet());
    assert(!spreadsheet.Editing() && !input.visible);
    assert(replacement.WorksheetCount() == 1);

    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {140, 10})));
    assert(spreadsheet.Captured());
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {180, 10})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerCancel, {180, 10})));
    assert(!spreadsheet.Captured());
    assert(replacement.ColumnWidth(spreadsheet.ActiveWorksheet(), 0) == 96);
    assert(!spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {200, 10})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 50})));
    assert(spreadsheet.Captured());
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {10, 74})));
    assert(spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerCancel, {10, 74})));
    assert(!spreadsheet.Captured());
    assert(replacement.RowHeight(spreadsheet.ActiveWorksheet(), 0) == 24);
    assert(!spreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {10, 90})));

    WorkbookModel cancelFailureModel;
    controls::list::DuiSpreadsheet cancelFailureSpreadsheet;
    cancelFailureSpreadsheet.SetWorkbookModel(&cancelFailureModel);
    cancelFailureSpreadsheet.Layout({0, 0, 500, 300});
    const int cancelFailureBoundary = cancelFailureSpreadsheet.Bounds().left + 42
        + cancelFailureModel.ColumnWidth(cancelFailureSpreadsheet.ActiveWorksheet(), 0);
    assert(cancelFailureSpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {cancelFailureBoundary, 10})));
    assert(cancelFailureSpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::PointerMove, {cancelFailureBoundary + 40, 10})));
    cancelFailureModel.SetDimensionWritesAllowed(false);
    assert(!cancelFailureSpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::PointerCancel, {cancelFailureBoundary + 40, 10})));
    assert(!cancelFailureSpreadsheet.Captured());
    assert(cancelFailureModel.ColumnWidth(cancelFailureSpreadsheet.ActiveWorksheet(), 0) == 136);

    WorkbookModel notifyingModel(true);
    controls::list::DuiSpreadsheet notifyingSpreadsheet;
    notifyingSpreadsheet.SetWorkbookModel(&notifyingModel);
    notifyingSpreadsheet.Layout({0, 0, 500, 300});
    const int notifyingResizeBoundary = notifyingSpreadsheet.Bounds().left + 42
        + notifyingModel.ColumnWidth(notifyingSpreadsheet.ActiveWorksheet(), 0);
    assert(notifyingSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerDown, {notifyingResizeBoundary, 10})));
    assert(notifyingSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerMove, {notifyingResizeBoundary + 40, 10})));
    assert(notifyingSpreadsheet.Captured());
    const int resizedColumnWidth = notifyingModel.ColumnWidth(notifyingSpreadsheet.ActiveWorksheet(), 0);
    assert(resizedColumnWidth == 136);
    assert(notifyingSpreadsheet.OnEvent(test::MakeEvent(core::EventType::PointerUp, {notifyingResizeBoundary + 40, 10})));
    assert(!notifyingSpreadsheet.Captured());
    notifyingSpreadsheet.Undo();
    assert(notifyingModel.ColumnWidth(notifyingSpreadsheet.ActiveWorksheet(), 0) == 96);
    notifyingSpreadsheet.Redo();
    assert(notifyingModel.ColumnWidth(notifyingSpreadsheet.ActiveWorksheet(), 0) == resizedColumnWidth);

    WorkbookModel subscribeDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> subscribeDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    subscribeDestroyingModel.SetSubscribeHandler([&subscribeDestroyingSpreadsheet]
    {
        subscribeDestroyingSpreadsheet.reset();
    });
    subscribeDestroyingSpreadsheet->SetWorkbookModel(&subscribeDestroyingModel);
    assert(subscribeDestroyingSpreadsheet == nullptr);
    assert(subscribeDestroyingModel.SubscriptionCount() == 0);

    WorkbookModel subscribeReplacingModel;
    WorkbookModel subscribeReplacementModel;
    controls::list::DuiSpreadsheet subscribeReplacingSpreadsheet;
    subscribeReplacingModel.SetSubscribeHandler([&subscribeReplacingSpreadsheet, &subscribeReplacementModel]
    {
        subscribeReplacingSpreadsheet.SetWorkbookModel(&subscribeReplacementModel);
    });
    subscribeReplacingSpreadsheet.SetWorkbookModel(&subscribeReplacingModel);
    assert(subscribeReplacingSpreadsheet.WorkbookModel() == &subscribeReplacementModel);
    assert(subscribeReplacingModel.SubscriptionCount() == 0);
    assert(subscribeReplacementModel.SubscriptionCount() == 1);

    WorkbookModel subscribeNotifyingModel;
    controls::list::DuiSpreadsheet subscribeNotifyingSpreadsheet;
    const DuiWorksheetId removedDuringSubscribe = subscribeNotifyingModel.WorksheetIdAt(0);
    subscribeNotifyingModel.SetSubscribeHandler([&subscribeNotifyingModel, removedDuringSubscribe]
    {
        assert(subscribeNotifyingModel.RemoveWorksheet(removedDuringSubscribe));
        subscribeNotifyingModel.NotifyChanged();
    });
    subscribeNotifyingSpreadsheet.SetWorkbookModel(&subscribeNotifyingModel);
    assert(subscribeNotifyingSpreadsheet.ActiveWorksheet() == subscribeNotifyingModel.WorksheetIdAt(0));
    assert(subscribeNotifyingModel.SubscriptionCount() == 1);

    WorkbookModel unsubscribeDestroyingModel;
    WorkbookModel unsubscribeReplacementModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> unsubscribeDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    unsubscribeDestroyingSpreadsheet->SetWorkbookModel(&unsubscribeDestroyingModel);
    unsubscribeDestroyingModel.SetUnsubscribeHandler([&unsubscribeDestroyingSpreadsheet]
    {
        unsubscribeDestroyingSpreadsheet.reset();
    });
    unsubscribeDestroyingSpreadsheet->SetWorkbookModel(&unsubscribeReplacementModel);
    assert(unsubscribeDestroyingSpreadsheet == nullptr);
    assert(unsubscribeDestroyingModel.SubscriptionCount() == 0);
    assert(unsubscribeReplacementModel.SubscriptionCount() == 0);

    {
        const std::size_t subscriptionsBeforeScoped = replacement.SubscriptionCount();
        controls::list::DuiSpreadsheet scopedSpreadsheet;
        scopedSpreadsheet.SetWorkbookModel(&replacement);
        scopedSpreadsheet.SetTextInput(&input);
        assert(replacement.SubscriptionCount() == subscriptionsBeforeScoped + 1);
        scopedSpreadsheet.SetWorkbookModel(nullptr);
        assert(replacement.SubscriptionCount() == subscriptionsBeforeScoped);
        scopedSpreadsheet.SetWorkbookModel(&replacement);
        assert(replacement.SubscriptionCount() == subscriptionsBeforeScoped + 1);
    }
    assert(replacement.SubscriptionCount() == 1);

    WorkbookModel unsupportedOperationsModel;
    controls::list::DuiSpreadsheet unsupportedOperationsSpreadsheet;
    unsupportedOperationsSpreadsheet.SetWorkbookModel(&unsupportedOperationsModel);
    int operationErrors{};
    unsupportedOperationsSpreadsheet.SetWorkbookOperationErrorHandler(
        [&operationErrors](const DuiWorkbookOperationResult& result)
        {
            assert(result.code == DuiWorkbookOperationErrorCode::Unsupported);
            ++operationErrors;
        });
    const DuiWorkbookFindResult unsupportedFind = unsupportedOperationsSpreadsheet.FindNext("missing");
    assert(!unsupportedFind.operation && operationErrors == 1 && unsupportedOperationsModel.FindCalls() == 0);

    WorkbookModel operationModel;
    operationModel.SetDimensions(20, 10);
    operationModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    operationModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    operationModel.SetCapabilities(DuiWorkbookCapability::Find | DuiWorkbookCapability::Replace
                                   | DuiWorkbookCapability::Sort | DuiWorkbookCapability::Filter);
    controls::list::DuiSpreadsheet operationSpreadsheet;
    operationSpreadsheet.SetWorkbookModel(&operationModel);
    operationSpreadsheet.Layout({0, 0, 500, 300});

    operationModel.SetFindResult({{}, DuiCellAddress{5, 3}});
    DuiWorkbookFindOptions findOptions;
    findOptions.matchCase = true;
    const DuiWorkbookFindResult found = operationSpreadsheet.FindNext("needle", findOptions);
    const DuiCellAddress expectedFoundCell{5, 3};
    assert(found.operation && found.cell == expectedFoundCell);
    assert(operationSpreadsheet.ActiveCell() == expectedFoundCell);
    assert(operationModel.FindCalls() == 1 && operationModel.LastFindQuery() == "needle");
    const DuiCellRange expectedFindRange{{0, 0}, {19, 9}};
    assert(operationModel.LastFindRange() == expectedFindRange);

    operationModel.SetReplaceResult({{}, 2, DuiCellAddress{2, 1}});
    const DuiWorkbookReplaceResult replaced = operationSpreadsheet.ReplaceText(
        "old", "new", true, {}, DuiCellRange{{1, 0}, {6, 4}});
    assert(replaced.operation && replaced.replacedCount == 2 && operationModel.ReplaceCalls() == 1);
    const DuiCellAddress expectedReplacementCell{2, 1};
    assert(operationSpreadsheet.ActiveCell() == expectedReplacementCell);

    const DuiCellRange sortRange{{1, 0}, {10, 4}};
    const std::vector<DuiWorkbookSortKey> sortKeys{{2, DuiWorkbookSortDirection::Descending}};
    assert(operationSpreadsheet.ApplySort(sortKeys, sortRange));
    assert(operationModel.SortCalls() == 1 && operationModel.LastSortKeys() == sortKeys);
    assert(operationSpreadsheet.SortKeys() == sortKeys);

    const std::vector<DuiWorkbookFilter> filters{
        {2, DuiWorkbookFilterOperator::Contains, "ready", false}};
    assert(operationSpreadsheet.SetFilters(filters));
    assert(operationModel.FilterCalls() == 1 && operationModel.LastFilters() == filters);
    assert(operationSpreadsheet.Filters() == filters);
    RecordingCanvas operationCanvas;
    operationSpreadsheet.Paint(operationCanvas, operationSpreadsheet.Bounds());
    assert(operationCanvas.pathFillColors.size() >= 3);
    assert(operationSpreadsheet.ClearFilters());
    assert(operationSpreadsheet.Filters().empty() && operationModel.FilterCalls() == 2);

    operationModel.SetPreferredColumnWidth(180);
    operationModel.SetPreferredRowHeight(48);
    assert(operationSpreadsheet.AutoFitColumn(0));
    assert(operationSpreadsheet.AutoFitRow(0));
    assert(operationModel.PreferredColumnWidthCalls() == 1
           && operationModel.PreferredRowHeightCalls() == 1);
    assert(operationModel.ColumnWidth(operationSpreadsheet.ActiveWorksheet(), 0) == 180);
    assert(operationModel.RowHeight(operationSpreadsheet.ActiveWorksheet(), 0) == 48);

    operationModel.SetPreferredRowHeight(std::nullopt);
    int fallbackRowCellReads{};
    operationModel.SetCellPresentationReadHandler([&fallbackRowCellReads] { ++fallbackRowCellReads; });
    assert(operationSpreadsheet.AutoFitRow(1));
    assert(fallbackRowCellReads == 0);
    operationModel.SetCellPresentationReadHandler({});

    operationModel.SetPreferredColumnWidth(210);
    operationSpreadsheet.Layout({0, 0, 500, 300});
    const int autoFitColumnBoundary = operationSpreadsheet.Bounds().left + 42 + 180;
    assert(operationSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::PointerDoubleClick, {autoFitColumnBoundary, 10})));
    assert(operationModel.ColumnWidth(operationSpreadsheet.ActiveWorksheet(), 0) == 210);
    operationModel.SetPreferredRowHeight(56);
    const int autoFitRowBoundary = operationSpreadsheet.Bounds().top + 24 + 48;
    assert(operationSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::PointerDoubleClick, {10, autoFitRowBoundary})));
    assert(operationModel.RowHeight(operationSpreadsheet.ActiveWorksheet(), 0) == 56);

    int findRequests{};
    int replaceRequests{};
    operationSpreadsheet.SetFindRequestedHandler([&findRequests] { ++findRequests; });
    operationSpreadsheet.SetReplaceRequestedHandler([&replaceRequests] { ++replaceRequests; });
    assert(operationSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::KeyDown, {}, 'F', core::modifier::Control)));
    assert(operationSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::KeyDown, {}, 'H', core::modifier::Control)));
    assert(findRequests == 1 && replaceRequests == 1);

    UiHostFactoryMock spreadsheetMenuFactory;
    operationSpreadsheet.SetPopupContext(spreadsheetMenuFactory, {});
    operationSpreadsheet.SetTextMeasurer(&operationCanvas);
    assert(operationSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::PointerDown, {60, 30}, 0, 0, 0, 0,
        core::PointerButton::Secondary)));
    assert(spreadsheetMenuFactory.popup != nullptr && !operationSpreadsheet.Captured());
    assert(spreadsheetMenuFactory.popup->Dispatch(test::MakeEvent(
        core::EventType::PointerDown, {20, 160})));
    assert(findRequests == 2);

    WorkbookModel compactMenuModel;
    controls::list::DuiSpreadsheet compactMenuSpreadsheet;
    UiHostFactoryMock compactMenuFactory;
    compactMenuSpreadsheet.SetWorkbookModel(&compactMenuModel);
    compactMenuSpreadsheet.SetPopupContext(compactMenuFactory, {});
    compactMenuSpreadsheet.Layout({0, 0, 500, 300});
    compactMenuSpreadsheet.ClearFrozenPanes();
    compactMenuSpreadsheet.SetActiveCell({2, 2});
    const auto freezeCell = compactMenuSpreadsheet.VisibleCellBounds({2, 2});
    assert(freezeCell.has_value());
    const core::Point freezePoint{freezeCell->left + 2, freezeCell->top + 2};
    assert(compactMenuSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::PointerDown, freezePoint, 0, 0, 0, 0, core::PointerButton::Secondary)));
    assert(compactMenuFactory.popup != nullptr);
    assert(compactMenuFactory.popup->options.size.height == 244);
    assert(compactMenuFactory.popup->Dispatch(test::MakeEvent(core::EventType::PointerDown, {20, 160})));
    assert(compactMenuSpreadsheet.FrozenRowCount() == 2
           && compactMenuSpreadsheet.FrozenColumnCount() == 2);
    assert(compactMenuSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::PointerDown, freezePoint, 0, 0, 0, 0, core::PointerButton::Secondary)));
    assert(compactMenuFactory.popup->options.size.height == 276);
    assert(compactMenuFactory.popup->Dispatch(test::MakeEvent(core::EventType::PointerDown, {20, 192})));
    assert(compactMenuSpreadsheet.FrozenRowCount() == 0
           && compactMenuSpreadsheet.FrozenColumnCount() == 0);

    input.LoseFocus();
    assert(!input.visible);
    return 0;
}
