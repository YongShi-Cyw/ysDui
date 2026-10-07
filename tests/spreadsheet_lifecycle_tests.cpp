/**
 * 文件名：spreadsheet_lifecycle_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：验证工作表模型、输入代理和回调重入生命周期。
 */
#include "spreadsheet_test_support.hpp"

using namespace ysDui;
using namespace ysDui::controls::list;
using namespace ysDui::test::spreadsheet;

int main()
{
    WorkbookModel replacement;

    WorkbookModel destroyingModel(true);
    std::unique_ptr<controls::list::DuiSpreadsheet> destroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock destroyingInput;
    const auto destroyingSubscription = destroyingModel.SubscribeChanged([&destroyingSpreadsheet]
    {
        destroyingSpreadsheet.reset();
    });
    assert(static_cast<bool>(destroyingSubscription));
    destroyingSpreadsheet->SetWorkbookModel(&destroyingModel);
    destroyingSpreadsheet->SetTextInput(&destroyingInput);
    destroyingSpreadsheet->SetActiveCell({0, 0});
    assert(destroyingSpreadsheet->BeginEdit());
    destroyingInput.text = "destroy during model write";
    destroyingSpreadsheet->CommitEdit();
    assert(destroyingSpreadsheet == nullptr);

    WorkbookModel destroyingResizeModel(true);
    std::unique_ptr<controls::list::DuiSpreadsheet> destroyingResizeSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    const auto destroyingResizeSubscription = destroyingResizeModel.SubscribeChanged(
        [&destroyingResizeSpreadsheet]
        {
            destroyingResizeSpreadsheet.reset();
        });
    assert(static_cast<bool>(destroyingResizeSubscription));
    destroyingResizeSpreadsheet->SetWorkbookModel(&destroyingResizeModel);
    destroyingResizeSpreadsheet->Layout({0, 0, 500, 300});
    const int destroyingResizeBoundary = destroyingResizeSpreadsheet->Bounds().left + 42
        + destroyingResizeModel.ColumnWidth(destroyingResizeSpreadsheet->ActiveWorksheet(), 0);
    assert(destroyingResizeSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {destroyingResizeBoundary, 10})));
    assert(destroyingResizeSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerMove, {destroyingResizeBoundary + 40, 10})));
    assert(destroyingResizeSpreadsheet == nullptr);

    WorkbookModel replacementDuringWrite;
    assert(replacementDuringWrite.SetCellText(replacementDuringWrite.WorksheetIdAt(0), {0, 0},
                                              "replacement value"));
    WorkbookModel switchingModel(true);
    std::unique_ptr<controls::list::DuiSpreadsheet> switchingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock switchingInput;
    const auto switchingSubscription = switchingModel.SubscribeChanged([&switchingSpreadsheet,
                                                                          &replacementDuringWrite]
    {
        switchingSpreadsheet->SetWorkbookModel(&replacementDuringWrite);
    });
    assert(static_cast<bool>(switchingSubscription));
    switchingSpreadsheet->SetWorkbookModel(&switchingModel);
    switchingSpreadsheet->SetTextInput(&switchingInput);
    switchingSpreadsheet->SetActiveCell({0, 0});
    assert(switchingSpreadsheet->BeginEdit());
    switchingInput.text = "old model value";
    switchingSpreadsheet->CommitEdit();
    assert(switchingSpreadsheet->WorkbookModel() == &replacementDuringWrite);
    switchingSpreadsheet->Undo();
    assert(replacementDuringWrite.CellText(replacementDuringWrite.WorksheetIdAt(0), {0, 0})
           == "replacement value");

    controls::list::DuiSpreadsheet reentrantInputSpreadsheet;
    TextInputMock reentrantInput;
    TextInputMock outerReplacementInput;
    TextInputMock callbackReplacementInput;
    reentrantInputSpreadsheet.SetWorkbookModel(&replacement);
    reentrantInputSpreadsheet.SetTextInput(&reentrantInput);
    reentrantInput.visibleChanged = [&reentrantInputSpreadsheet, &reentrantInput, &callbackReplacementInput](bool visible)
    {
        if (!visible)
        {
            reentrantInput.visibleChanged = {};
            reentrantInputSpreadsheet.SetTextInput(&callbackReplacementInput);
        }
    };
    reentrantInputSpreadsheet.SetTextInput(&outerReplacementInput);
    assert(reentrantInputSpreadsheet.BeginEdit());
    assert(callbackReplacementInput.visible);
    assert(!outerReplacementInput.visible);

    controls::list::DuiSpreadsheet handlerReentrantInputSpreadsheet;
    TextInputMock handlerReentrantInput;
    TextInputMock handlerOuterReplacementInput;
    TextInputMock handlerCallbackReplacementInput;
    int staleFocusHandlerUpdates{};
    handlerReentrantInputSpreadsheet.SetWorkbookModel(&replacement);
    handlerReentrantInputSpreadsheet.SetTextInput(&handlerReentrantInput);
    handlerReentrantInput.focusLostHandlerChanged = [&staleFocusHandlerUpdates]
    {
        ++staleFocusHandlerUpdates;
    };
    handlerReentrantInput.changedHandlerChanged = [&handlerReentrantInputSpreadsheet,
                                                    &handlerReentrantInput,
                                                    &handlerCallbackReplacementInput]
    {
        handlerReentrantInput.changedHandlerChanged = {};
        handlerReentrantInputSpreadsheet.SetTextInput(&handlerCallbackReplacementInput);
    };
    handlerReentrantInputSpreadsheet.SetTextInput(&handlerOuterReplacementInput);
    assert(staleFocusHandlerUpdates == 0);
    assert(handlerReentrantInputSpreadsheet.BeginEdit());
    assert(handlerCallbackReplacementInput.visible);
    assert(!handlerOuterReplacementInput.visible);

    std::unique_ptr<controls::list::DuiSpreadsheet> nonEditingInputDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock nonEditingInputDestroyingInput;
    TextInputMock nonEditingReplacementInput;
    nonEditingInputDestroyingSpreadsheet->SetWorkbookModel(&replacement);
    nonEditingInputDestroyingSpreadsheet->SetTextInput(&nonEditingInputDestroyingInput);
    nonEditingInputDestroyingInput.visibleChanged = [&nonEditingInputDestroyingSpreadsheet](bool visible)
    {
        if (!visible)
            nonEditingInputDestroyingSpreadsheet.reset();
    };
    nonEditingInputDestroyingSpreadsheet->SetTextInput(&nonEditingReplacementInput);
    assert(nonEditingInputDestroyingSpreadsheet == nullptr);

    std::unique_ptr<controls::list::DuiSpreadsheet> inputDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock inputDestroyingInput;
    inputDestroyingSpreadsheet->SetWorkbookModel(&replacement);
    inputDestroyingSpreadsheet->SetTextInput(&inputDestroyingInput);
    inputDestroyingSpreadsheet->SetActiveCell({0, 0});
    assert(inputDestroyingSpreadsheet->BeginEdit());
    inputDestroyingInput.visibleChanged = [&inputDestroyingSpreadsheet](bool visible)
    {
        if (!visible)
            inputDestroyingSpreadsheet.reset();
    };
    inputDestroyingSpreadsheet->SetTextInput(nullptr);
    assert(inputDestroyingSpreadsheet == nullptr);

    std::unique_ptr<controls::list::DuiSpreadsheet> reloadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock reloadDestroyingInput;
    reloadDestroyingSpreadsheet->SetWorkbookModel(&replacement);
    reloadDestroyingSpreadsheet->SetTextInput(&reloadDestroyingInput);
    reloadDestroyingSpreadsheet->SetActiveCell({0, 0});
    assert(reloadDestroyingSpreadsheet->BeginEdit());
    reloadDestroyingInput.visibleChanged = [&reloadDestroyingSpreadsheet](bool visible)
    {
        if (!visible)
            reloadDestroyingSpreadsheet.reset();
    };
    reloadDestroyingSpreadsheet->ReloadFromModel();
    assert(reloadDestroyingSpreadsheet == nullptr);

    std::unique_ptr<controls::list::DuiSpreadsheet> commitDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock commitDestroyingInput;
    commitDestroyingSpreadsheet->SetWorkbookModel(&replacement);
    commitDestroyingSpreadsheet->SetTextInput(&commitDestroyingInput);
    commitDestroyingSpreadsheet->SetActiveCell({0, 0});
    assert(commitDestroyingSpreadsheet->BeginEdit());
    commitDestroyingInput.text = "destroy during editor hide";
    commitDestroyingInput.visibleChanged = [&commitDestroyingSpreadsheet](bool visible)
    {
        if (!visible)
            commitDestroyingSpreadsheet.reset();
    };
    commitDestroyingSpreadsheet->CommitEdit();
    assert(commitDestroyingSpreadsheet == nullptr);

    WorkbookModel commitHideReplacingModel;
    WorkbookModel commitHideReplacementModel;
    assert(commitHideReplacingModel.SetCellText(commitHideReplacingModel.WorksheetIdAt(0), {0, 0},
                                                "original before hide"));
    assert(commitHideReplacementModel.SetCellText(commitHideReplacementModel.WorksheetIdAt(0), {0, 0},
                                                  "replacement before hide"));
    controls::list::DuiSpreadsheet commitHideReplacingSpreadsheet;
    TextInputMock commitHideReplacingInput;
    commitHideReplacingSpreadsheet.SetWorkbookModel(&commitHideReplacingModel);
    commitHideReplacingSpreadsheet.SetTextInput(&commitHideReplacingInput);
    assert(commitHideReplacingSpreadsheet.BeginEdit());
    commitHideReplacingInput.text = "pending during hide";
    commitHideReplacingInput.visibleChanged = [&commitHideReplacingSpreadsheet,
                                                &commitHideReplacingInput,
                                                &commitHideReplacementModel](bool visible)
    {
        if (!visible)
        {
            commitHideReplacingInput.visibleChanged = {};
            commitHideReplacingSpreadsheet.SetWorkbookModel(&commitHideReplacementModel);
        }
    };
    commitHideReplacingSpreadsheet.CommitEdit();
    assert(commitHideReplacingSpreadsheet.WorkbookModel() == &commitHideReplacementModel);
    assert(commitHideReplacingModel.CellText(commitHideReplacingModel.WorksheetIdAt(0), {0, 0})
           == "original before hide");
    assert(commitHideReplacementModel.CellText(commitHideReplacementModel.WorksheetIdAt(0), {0, 0})
           == "replacement before hide");

    WorkbookModel rejectedRestoreDestroyingModel;
    rejectedRestoreDestroyingModel.SetCellWritesAllowed(false);
    std::unique_ptr<controls::list::DuiSpreadsheet> rejectedRestoreDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock rejectedRestoreDestroyingInput;
    rejectedRestoreDestroyingSpreadsheet->SetWorkbookModel(&rejectedRestoreDestroyingModel);
    rejectedRestoreDestroyingSpreadsheet->SetTextInput(&rejectedRestoreDestroyingInput);
    assert(rejectedRestoreDestroyingSpreadsheet->BeginEdit());
    rejectedRestoreDestroyingInput.text = "rejected restore";
    rejectedRestoreDestroyingInput.visibleChanged = [&rejectedRestoreDestroyingSpreadsheet](bool visible)
    {
        if (visible)
            rejectedRestoreDestroyingSpreadsheet.reset();
    };
    rejectedRestoreDestroyingSpreadsheet->CommitEdit();
    assert(rejectedRestoreDestroyingSpreadsheet == nullptr);

    std::unique_ptr<controls::list::DuiSpreadsheet> uiaDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock uiaDestroyingInput;
    uiaDestroyingSpreadsheet->SetWorkbookModel(&replacement);
    uiaDestroyingSpreadsheet->SetTextInput(&uiaDestroyingInput);
    uiaDestroyingSpreadsheet->SetActiveCell({0, 0});
    assert(uiaDestroyingSpreadsheet->BeginEdit());
    uiaDestroyingInput.visibleChanged = [&uiaDestroyingSpreadsheet](bool visible)
    {
        if (!visible)
            uiaDestroyingSpreadsheet.reset();
    };
    assert(!static_cast<core::Control&>(*uiaDestroyingSpreadsheet).PerformAccessibilityAction(
        core::DuiAccessibilityAction::SetValue, "destroy during UIA write"));
    assert(uiaDestroyingSpreadsheet == nullptr);

    std::unique_ptr<controls::list::DuiSpreadsheet> beginDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock beginDestroyingInput;
    beginDestroyingSpreadsheet->SetWorkbookModel(&replacement);
    beginDestroyingSpreadsheet->SetTextInput(&beginDestroyingInput);
    beginDestroyingSpreadsheet->SetActiveCell({0, 0});
    beginDestroyingInput.changed = [&beginDestroyingSpreadsheet]
    {
        beginDestroyingSpreadsheet.reset();
    };
    assert(!beginDestroyingSpreadsheet->BeginEdit());
    assert(beginDestroyingSpreadsheet == nullptr);

    WorkbookModel beginReadDestroyModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> beginReadDestroySpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock beginReadDestroyInput;
    beginReadDestroySpreadsheet->SetWorkbookModel(&beginReadDestroyModel);
    beginReadDestroySpreadsheet->SetTextInput(&beginReadDestroyInput);
    beginReadDestroySpreadsheet->SetActiveCell({0, 0});
    beginReadDestroyModel.SetCellReadHandler([&beginReadDestroySpreadsheet]
    {
        beginReadDestroySpreadsheet.reset();
    });
    assert(!beginReadDestroySpreadsheet->BeginEdit());
    assert(beginReadDestroySpreadsheet == nullptr);

    WorkbookModel focusLostDuringBeginModel;
    controls::list::DuiSpreadsheet focusLostDuringBeginSpreadsheet;
    TextInputMock focusLostDuringBeginInput;
    focusLostDuringBeginSpreadsheet.SetWorkbookModel(&focusLostDuringBeginModel);
    focusLostDuringBeginSpreadsheet.SetTextInput(&focusLostDuringBeginInput);
    focusLostDuringBeginInput.focusRequested = [&focusLostDuringBeginInput]
    {
        focusLostDuringBeginInput.LoseFocus();
    };
    assert(!focusLostDuringBeginSpreadsheet.BeginEdit());
    assert(!focusLostDuringBeginSpreadsheet.Editing());
    assert(!focusLostDuringBeginInput.visible);

    WorkbookModel reloadReplacingModel;
    WorkbookModel reloadReplacementModel;
    controls::list::DuiSpreadsheet reloadReplacingSpreadsheet;
    reloadReplacingSpreadsheet.SetWorkbookModel(&reloadReplacingModel);
    reloadReplacingSpreadsheet.Layout({0, 0, 500, 300});
    assert(reloadReplacingModel.SetRowHeight(reloadReplacingModel.WorksheetIdAt(0), 0, 60));
    reloadReplacingModel.SetRowHeightReadHandler([&reloadReplacingSpreadsheet, &reloadReplacementModel]
    {
        reloadReplacingSpreadsheet.SetWorkbookModel(&reloadReplacementModel);
    });
    reloadReplacingSpreadsheet.ReloadFromModel();
    assert(reloadReplacingSpreadsheet.WorkbookModel() == &reloadReplacementModel);
    assert(reloadReplacingSpreadsheet.HitTest({10, 48}) == &reloadReplacingSpreadsheet);
    assert(reloadReplacingSpreadsheet.PointerCursor() == core::DuiPointerCursor::ResizeVertical);

    WorkbookModel reloadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> reloadReadingDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    reloadReadingDestroyingSpreadsheet->SetWorkbookModel(&reloadDestroyingModel);
    reloadDestroyingModel.SetRowHeightReadHandler([&reloadReadingDestroyingSpreadsheet]
    {
        reloadReadingDestroyingSpreadsheet.reset();
    });
    reloadReadingDestroyingSpreadsheet->ReloadFromModel();
    assert(reloadReadingDestroyingSpreadsheet == nullptr);

    WorkbookModel sparseReloadReplacingModel;
    WorkbookModel sparseReloadReplacementModel;
    sparseReloadReplacingModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    sparseReloadReplacingModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    controls::list::DuiSpreadsheet sparseReloadReplacingSpreadsheet;
    sparseReloadReplacingSpreadsheet.SetWorkbookModel(&sparseReloadReplacingModel);
    sparseReloadReplacingModel.SetSparseColumnSizeReadHandler(
        [&sparseReloadReplacingSpreadsheet, &sparseReloadReplacementModel]
    {
        sparseReloadReplacingSpreadsheet.SetWorkbookModel(&sparseReloadReplacementModel);
    });
    sparseReloadReplacingSpreadsheet.ReloadFromModel();
    assert(sparseReloadReplacingSpreadsheet.WorkbookModel() == &sparseReloadReplacementModel);

    WorkbookModel sparseInitialLoadDestroyingModel;
    sparseInitialLoadDestroyingModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    sparseInitialLoadDestroyingModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    std::unique_ptr<controls::list::DuiSpreadsheet> sparseInitialLoadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    sparseInitialLoadDestroyingModel.SetSparseRowSizeReadHandler([&sparseInitialLoadDestroyingSpreadsheet]
    {
        sparseInitialLoadDestroyingSpreadsheet.reset();
    });
    sparseInitialLoadDestroyingSpreadsheet->SetWorkbookModel(&sparseInitialLoadDestroyingModel);
    assert(sparseInitialLoadDestroyingSpreadsheet == nullptr);

    WorkbookModel initialLoadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> initialLoadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    initialLoadDestroyingModel.SetRowHeightReadHandler([&initialLoadDestroyingSpreadsheet]
    {
        initialLoadDestroyingSpreadsheet.reset();
    });
    initialLoadDestroyingSpreadsheet->SetWorkbookModel(&initialLoadDestroyingModel);
    assert(initialLoadDestroyingSpreadsheet == nullptr);

    WorkbookModel layoutDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> layoutDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    layoutDestroyingSpreadsheet->SetWorkbookModel(&layoutDestroyingModel);
    layoutDestroyingModel.SetWorksheetCountReadHandler([&layoutDestroyingSpreadsheet]
    {
        layoutDestroyingSpreadsheet.reset();
    });
    layoutDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(layoutDestroyingSpreadsheet == nullptr);

    WorkbookModel replacingInputDuringLayoutModel;
    controls::list::DuiSpreadsheet replacingInputDuringLayoutSpreadsheet;
    TextInputMock replacingInputDuringLayoutInput;
    TextInputMock replacementDuringLayoutInput;
    replacingInputDuringLayoutSpreadsheet.SetWorkbookModel(&replacingInputDuringLayoutModel);
    replacingInputDuringLayoutSpreadsheet.SetTextInput(&replacingInputDuringLayoutInput);
    replacingInputDuringLayoutSpreadsheet.SetActiveCell({0, 0});
    replacingInputDuringLayoutInput.boundsChanged = [&replacingInputDuringLayoutSpreadsheet, &replacementDuringLayoutInput]
    {
        replacingInputDuringLayoutSpreadsheet.SetTextInput(&replacementDuringLayoutInput);
    };
    assert(!replacingInputDuringLayoutSpreadsheet.BeginEdit());
    assert(!replacingInputDuringLayoutSpreadsheet.Editing());
    assert(!replacementDuringLayoutInput.focused);

    WorkbookModel commitReadDestroyModel;
    assert(commitReadDestroyModel.SetCellText(commitReadDestroyModel.WorksheetIdAt(0), {0, 0}, "before read"));
    std::unique_ptr<controls::list::DuiSpreadsheet> commitReadDestroySpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock commitReadDestroyInput;
    commitReadDestroySpreadsheet->SetWorkbookModel(&commitReadDestroyModel);
    commitReadDestroySpreadsheet->SetTextInput(&commitReadDestroyInput);
    commitReadDestroySpreadsheet->SetActiveCell({0, 0});
    assert(commitReadDestroySpreadsheet->BeginEdit());
    commitReadDestroyInput.text = "after read";
    commitReadDestroyModel.SetCellReadHandler([&commitReadDestroySpreadsheet]
    {
        commitReadDestroySpreadsheet.reset();
    });
    commitReadDestroySpreadsheet->CommitEdit();
    assert(commitReadDestroySpreadsheet == nullptr);

    WorkbookModel uiaReadDestroyModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> uiaReadDestroySpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    uiaReadDestroySpreadsheet->SetWorkbookModel(&uiaReadDestroyModel);
    uiaReadDestroySpreadsheet->SetActiveCell({0, 0});
    uiaReadDestroyModel.SetCellReadHandler([&uiaReadDestroySpreadsheet]
    {
        uiaReadDestroySpreadsheet.reset();
    });
    assert(!static_cast<core::Control&>(*uiaReadDestroySpreadsheet).PerformAccessibilityAction(
        core::DuiAccessibilityAction::SetValue, "destroy during cell read"));
    assert(uiaReadDestroySpreadsheet == nullptr);

    WorkbookModel deletingTargetModel(true);
    controls::list::DuiSpreadsheet deletingTargetSpreadsheet;
    TextInputMock deletingTargetInput;
    int deletingTargetSheetChanged{};
    const auto deletingTargetSubscription = deletingTargetModel.SubscribeChanged([&deletingTargetModel]
    {
        deletingTargetModel.RemoveWorksheet(2);
    });
    assert(static_cast<bool>(deletingTargetSubscription));
    deletingTargetSpreadsheet.SetWorkbookModel(&deletingTargetModel);
    deletingTargetSpreadsheet.SetTextInput(&deletingTargetInput);
    deletingTargetSpreadsheet.SetActiveCell({0, 0});
    deletingTargetSpreadsheet.SetWorksheetChangedHandler([&deletingTargetSheetChanged](DuiWorksheetId)
    {
        ++deletingTargetSheetChanged;
    });
    assert(deletingTargetSpreadsheet.BeginEdit());
    deletingTargetInput.text = "delete target during switch";
    assert(!deletingTargetSpreadsheet.SetActiveWorksheet(2));
    assert(deletingTargetSpreadsheet.ActiveWorksheet() == 1);
    assert(deletingTargetModel.WorksheetCount() == 1 && deletingTargetSheetChanged == 0);

    WorkbookModel deletingRenameModel(true);
    controls::list::DuiSpreadsheet deletingRenameSpreadsheet;
    DuiWorksheetId deletingRenameChangedSheet{};
    const auto deletingRenameSubscription = deletingRenameModel.SubscribeChanged([&deletingRenameModel]
    {
        deletingRenameModel.RemoveWorksheet(1);
    });
    assert(static_cast<bool>(deletingRenameSubscription));
    deletingRenameSpreadsheet.SetWorkbookModel(&deletingRenameModel);
    deletingRenameSpreadsheet.SetWorksheetChangedHandler([&deletingRenameChangedSheet](DuiWorksheetId sheet)
    {
        deletingRenameChangedSheet = sheet;
    });
    assert(!deletingRenameSpreadsheet.RenameActiveWorksheet("Deleted during rename"));
    assert(deletingRenameSpreadsheet.ActiveWorksheet() == 2);
    assert(deletingRenameChangedSheet == 2 && deletingRenameModel.WorksheetCount() == 1);

    WorkbookModel deletingAddedModel(true);
    controls::list::DuiSpreadsheet deletingAddedSpreadsheet;
    const auto deletingAddedSubscription = deletingAddedModel.SubscribeChanged([&deletingAddedModel]
    {
        if (deletingAddedModel.WorksheetCount() > 2)
            deletingAddedModel.RemoveWorksheet(
                deletingAddedModel.WorksheetIdAt(deletingAddedModel.WorksheetCount() - 1));
    });
    assert(static_cast<bool>(deletingAddedSubscription));
    deletingAddedSpreadsheet.SetWorkbookModel(&deletingAddedModel);
    assert(deletingAddedSpreadsheet.AddWorksheet("Deleted by callback") == 0);
    assert(deletingAddedSpreadsheet.ActiveWorksheet() == 1);
    assert(deletingAddedModel.WorksheetCount() == 2);

    std::unique_ptr<controls::list::DuiSpreadsheet> callbackSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    callbackSpreadsheet->SetWorkbookModel(&replacement);
    callbackSpreadsheet->SetActiveCellChangedHandler([&callbackSpreadsheet](DuiCellAddress)
    {
        callbackSpreadsheet.reset();
    });
    callbackSpreadsheet->SetActiveCell({1, 1});
    assert(callbackSpreadsheet == nullptr);

    WorkbookModel activeCallbackSourceModel;
    WorkbookModel activeCallbackReplacementModel;
    controls::list::DuiSpreadsheet activeCallbackReplacingSpreadsheet;
    activeCallbackReplacingSpreadsheet.SetWorkbookModel(&activeCallbackSourceModel);
    int staleSelectionNotifications{};
    activeCallbackReplacingSpreadsheet.SetActiveCellChangedHandler(
        [&activeCallbackReplacingSpreadsheet, &activeCallbackReplacementModel](DuiCellAddress)
        {
            activeCallbackReplacingSpreadsheet.SetWorkbookModel(&activeCallbackReplacementModel);
        });
    activeCallbackReplacingSpreadsheet.SetSelectionChangedHandler([&staleSelectionNotifications](DuiCellRange)
    {
        ++staleSelectionNotifications;
    });
    activeCallbackReplacingSpreadsheet.SetActiveCell({1, 1});
    assert(activeCallbackReplacingSpreadsheet.WorkbookModel() == &activeCallbackReplacementModel);
    assert(staleSelectionNotifications == 0);

    WorkbookModel activeCallbackReentrantModel;
    controls::list::DuiSpreadsheet activeCallbackReentrantSpreadsheet;
    activeCallbackReentrantSpreadsheet.SetWorkbookModel(&activeCallbackReentrantModel);
    int reentrantSelectionNotifications{};
    activeCallbackReentrantSpreadsheet.SetActiveCellChangedHandler(
        [&activeCallbackReentrantSpreadsheet](DuiCellAddress)
        {
            activeCallbackReentrantSpreadsheet.SetActiveCellChangedHandler({});
            activeCallbackReentrantSpreadsheet.SetActiveCell({2, 2});
        });
    activeCallbackReentrantSpreadsheet.SetSelectionChangedHandler(
        [&reentrantSelectionNotifications](DuiCellRange)
        {
            ++reentrantSelectionNotifications;
        });
    activeCallbackReentrantSpreadsheet.SetActiveCell({1, 1});
    assert(activeCallbackReentrantSpreadsheet.ActiveCell() == (DuiCellAddress{2, 2}));
    assert(activeCallbackReentrantSpreadsheet.Selection().Normalized().first == (DuiCellAddress{2, 2}));
    assert(activeCallbackReentrantSpreadsheet.Selection().Normalized().last == (DuiCellAddress{2, 2}));
    assert(reentrantSelectionNotifications == 1);

    WorkbookModel selectionCallbackReentrantModel;
    controls::list::DuiSpreadsheet selectionCallbackReentrantSpreadsheet;
    selectionCallbackReentrantSpreadsheet.SetWorkbookModel(&selectionCallbackReentrantModel);
    selectionCallbackReentrantSpreadsheet.Layout({0, 0, 500, 300});
    selectionCallbackReentrantSpreadsheet.SetSelectionChangedHandler(
        [&selectionCallbackReentrantSpreadsheet](DuiCellRange)
        {
            selectionCallbackReentrantSpreadsheet.SetSelectionChangedHandler({});
            selectionCallbackReentrantSpreadsheet.SetActiveCell({0, 0});
        });
    selectionCallbackReentrantSpreadsheet.SetActiveCell({99999, 999});
    assert(selectionCallbackReentrantSpreadsheet.ActiveCell() == (DuiCellAddress{0, 0}));
    assert(selectionCallbackReentrantSpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {139, 49})));
    assert(selectionCallbackReentrantSpreadsheet.ActiveCell() == (DuiCellAddress{1, 1}));

    WorkbookModel pointerDownCallbackSourceModel;
    WorkbookModel pointerDownCallbackReplacementModel;
    controls::list::DuiSpreadsheet pointerDownCallbackSpreadsheet;
    pointerDownCallbackSpreadsheet.SetWorkbookModel(&pointerDownCallbackSourceModel);
    pointerDownCallbackSpreadsheet.Layout({0, 0, 500, 300});
    pointerDownCallbackSpreadsheet.SetActiveCellChangedHandler(
        [&pointerDownCallbackSpreadsheet, &pointerDownCallbackReplacementModel](DuiCellAddress)
        {
            pointerDownCallbackSpreadsheet.SetWorkbookModel(&pointerDownCallbackReplacementModel);
        });
    assert(pointerDownCallbackSpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {150, 60})));
    assert(pointerDownCallbackSpreadsheet.WorkbookModel() == &pointerDownCallbackReplacementModel);
    assert(!pointerDownCallbackSpreadsheet.Captured());

    WorkbookModel doubleClickCallbackSourceModel;
    WorkbookModel doubleClickCallbackReplacementModel;
    controls::list::DuiSpreadsheet doubleClickCallbackSpreadsheet;
    TextInputMock doubleClickCallbackInput;
    doubleClickCallbackSpreadsheet.SetWorkbookModel(&doubleClickCallbackSourceModel);
    doubleClickCallbackSpreadsheet.SetTextInput(&doubleClickCallbackInput);
    doubleClickCallbackSpreadsheet.Layout({0, 0, 500, 300});
    doubleClickCallbackSpreadsheet.SetActiveCellChangedHandler(
        [&doubleClickCallbackSpreadsheet, &doubleClickCallbackReplacementModel](DuiCellAddress)
        {
            doubleClickCallbackSpreadsheet.SetWorkbookModel(&doubleClickCallbackReplacementModel);
        });
    assert(doubleClickCallbackSpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::PointerDoubleClick, {150, 60})));
    assert(doubleClickCallbackSpreadsheet.WorkbookModel() == &doubleClickCallbackReplacementModel);
    assert(!doubleClickCallbackSpreadsheet.Editing());

    std::unique_ptr<controls::list::DuiSpreadsheet> selectionSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    selectionSpreadsheet->SetWorkbookModel(&replacement);
    selectionSpreadsheet->SetSelectionChangedHandler([&selectionSpreadsheet](DuiCellRange)
    {
        selectionSpreadsheet.reset();
    });
    selectionSpreadsheet->SetSelection({{1, 1}, {2, 2}});
    assert(selectionSpreadsheet == nullptr);

    WorkbookModel worksheetModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> worksheetSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    worksheetSpreadsheet->SetWorkbookModel(&worksheetModel);
    worksheetSpreadsheet->SetWorksheetChangedHandler([&worksheetSpreadsheet](DuiWorksheetId)
    {
        worksheetSpreadsheet.reset();
    });
    assert(worksheetSpreadsheet->SetActiveWorksheet(2));
    assert(worksheetSpreadsheet == nullptr);

    std::unique_ptr<controls::list::DuiSpreadsheet> removeWorksheetSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    removeWorksheetSpreadsheet->SetWorkbookModel(&worksheetModel);
    removeWorksheetSpreadsheet->SetWorksheetChangedHandler([&removeWorksheetSpreadsheet](DuiWorksheetId)
    {
        removeWorksheetSpreadsheet.reset();
    });
    assert(removeWorksheetSpreadsheet->RemoveActiveWorksheet());
    assert(removeWorksheetSpreadsheet == nullptr);

    std::unique_ptr<controls::list::DuiSpreadsheet> selectAllSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    selectAllSpreadsheet->SetWorkbookModel(&replacement);
    selectAllSpreadsheet->Layout({0, 0, 500, 300});
    selectAllSpreadsheet->SetSelectionChangedHandler([&selectAllSpreadsheet](DuiCellRange)
    {
        selectAllSpreadsheet.reset();
    });
    assert(selectAllSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 10})));
    assert(selectAllSpreadsheet == nullptr);

    std::unique_ptr<controls::list::DuiSpreadsheet> columnHeaderSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    columnHeaderSpreadsheet->SetWorkbookModel(&replacement);
    columnHeaderSpreadsheet->Layout({0, 0, 500, 300});
    columnHeaderSpreadsheet->SetSelectionChangedHandler([&columnHeaderSpreadsheet](DuiCellRange)
    {
        columnHeaderSpreadsheet.reset();
    });
    assert(columnHeaderSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {150, 10})));
    assert(columnHeaderSpreadsheet == nullptr);

    WorkbookModel headerCallbackSourceModel;
    WorkbookModel headerCallbackReplacementModel;
    controls::list::DuiSpreadsheet headerCallbackReplacingSpreadsheet;
    headerCallbackReplacingSpreadsheet.SetWorkbookModel(&headerCallbackSourceModel);
    headerCallbackReplacingSpreadsheet.Layout({0, 0, 500, 300});
    headerCallbackReplacingSpreadsheet.SetSelectionChangedHandler(
        [&headerCallbackReplacingSpreadsheet, &headerCallbackReplacementModel](DuiCellRange)
        {
            headerCallbackReplacingSpreadsheet.SetWorkbookModel(&headerCallbackReplacementModel);
        });
    assert(headerCallbackReplacingSpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {150, 10})));
    assert(headerCallbackReplacingSpreadsheet.WorkbookModel() == &headerCallbackReplacementModel);
    assert(!headerCallbackReplacingSpreadsheet.Captured());

    WorkbookModel headerCallbackCancelModel;
    controls::list::DuiSpreadsheet headerCallbackCancelSpreadsheet;
    headerCallbackCancelSpreadsheet.SetWorkbookModel(&headerCallbackCancelModel);
    headerCallbackCancelSpreadsheet.Layout({0, 0, 500, 300});
    headerCallbackCancelSpreadsheet.SetSelectionChangedHandler(
        [&headerCallbackCancelSpreadsheet](DuiCellRange)
        {
            headerCallbackCancelSpreadsheet.SetSelectionChangedHandler({});
            assert(headerCallbackCancelSpreadsheet.OnEvent(
                test::MakeEvent(core::EventType::PointerCancel)));
        });
    assert(headerCallbackCancelSpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {150, 10})));
    assert(!headerCallbackCancelSpreadsheet.Captured());

    std::unique_ptr<controls::list::DuiSpreadsheet> rowHeaderSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    rowHeaderSpreadsheet->SetWorkbookModel(&replacement);
    rowHeaderSpreadsheet->Layout({0, 0, 500, 300});
    rowHeaderSpreadsheet->SetSelectionChangedHandler([&rowHeaderSpreadsheet](DuiCellRange)
    {
        rowHeaderSpreadsheet.reset();
    });
    assert(rowHeaderSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 60})));
    assert(rowHeaderSpreadsheet == nullptr);

    std::unique_ptr<controls::list::DuiSpreadsheet> draggingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    draggingSpreadsheet->SetWorkbookModel(&replacement);
    draggingSpreadsheet->Layout({0, 0, 500, 300});
    int selectionNotifications = 0;
    draggingSpreadsheet->SetSelectionChangedHandler([&draggingSpreadsheet, &selectionNotifications](DuiCellRange)
    {
        if (++selectionNotifications == 2)
            draggingSpreadsheet.reset();
    });
    assert(draggingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {150, 60})));
    assert(draggingSpreadsheet != nullptr);
    assert(draggingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerMove, {260, 84})));
    assert(draggingSpreadsheet == nullptr);

    WorkbookModel paintDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> paintDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    paintDestroyingSpreadsheet->SetWorkbookModel(&paintDestroyingModel);
    paintDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    paintDestroyingModel.SetCellPresentationReadHandler([&paintDestroyingSpreadsheet]
    {
        paintDestroyingSpreadsheet.reset();
    });
    RecordingCanvas paintDestroyingCanvas;
    paintDestroyingSpreadsheet->Paint(paintDestroyingCanvas, paintDestroyingSpreadsheet->Bounds());
    assert(paintDestroyingSpreadsheet == nullptr);

    WorkbookModel accessibilityDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> accessibilityDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    accessibilityDestroyingSpreadsheet->SetWorkbookModel(&accessibilityDestroyingModel);
    accessibilityDestroyingSpreadsheet->SetActiveCell({0, 0});
    accessibilityDestroyingModel.SetCellPresentationReadHandler([&accessibilityDestroyingSpreadsheet]
    {
        accessibilityDestroyingSpreadsheet.reset();
    });
    const core::DuiAccessibilityData accessibility =
        static_cast<core::Control&>(*accessibilityDestroyingSpreadsheet).Accessibility();
    assert(accessibilityDestroyingSpreadsheet == nullptr);
    assert(accessibility.role == core::DuiAccessibilityRole::Table);

    WorkbookModel sheetBarDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> sheetBarDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    sheetBarDestroyingSpreadsheet->SetWorkbookModel(&sheetBarDestroyingModel);
    sheetBarDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    sheetBarDestroyingModel.SetWorksheetCountReadHandler([&sheetBarDestroyingSpreadsheet]
    {
        sheetBarDestroyingSpreadsheet.reset();
    });
    assert(sheetBarDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {10, 290})));
    assert(sheetBarDestroyingSpreadsheet == nullptr);

    WorkbookModel sheetContextDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> sheetContextDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    sheetContextDestroyingSpreadsheet->SetWorkbookModel(&sheetContextDestroyingModel);
    sheetContextDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    sheetContextDestroyingModel.SetWorksheetCountReadHandler([&sheetContextDestroyingSpreadsheet]
    {
        sheetContextDestroyingSpreadsheet.reset();
    });
    assert(sheetContextDestroyingSpreadsheet->OnEvent(test::MakeEvent(
        core::EventType::PointerDown, {120, 285}, 0, 0, 0, 0, core::PointerButton::Secondary)));
    assert(sheetContextDestroyingSpreadsheet == nullptr);

    WorkbookModel renameSubmitModel;
    WorkbookModel renameReplacementModel;
    controls::list::DuiSpreadsheet renameSubmitSpreadsheet;
    TextInputMock renameSubmitInput;
    renameSubmitSpreadsheet.SetWorkbookModel(&renameSubmitModel);
    renameSubmitSpreadsheet.SetTextInput(&renameSubmitInput);
    renameSubmitSpreadsheet.Layout({0, 0, 500, 300});
    assert(renameSubmitSpreadsheet.OnEvent(test::MakeEvent(
        core::EventType::PointerDoubleClick, {120, 285})));
    renameSubmitInput.text = "Renamed";
    renameSubmitModel.SetRenameWorksheetHandler([&renameSubmitSpreadsheet, &renameReplacementModel]
    {
        renameSubmitSpreadsheet.SetWorkbookModel(&renameReplacementModel);
    });
    renameSubmitInput.Submit();
    assert(renameSubmitSpreadsheet.WorkbookModel() == &renameReplacementModel);
    assert(!renameSubmitInput.visible);

    WorkbookModel setActiveDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> setActiveDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    setActiveDestroyingSpreadsheet->SetWorkbookModel(&setActiveDestroyingModel);
    setActiveDestroyingModel.SetWorksheetCountReadHandler([&setActiveDestroyingSpreadsheet]
    {
        setActiveDestroyingSpreadsheet.reset();
    });
    assert(setActiveDestroyingSpreadsheet->SetActiveWorksheet(2));
    assert(setActiveDestroyingSpreadsheet == nullptr);

    WorkbookModel addWorksheetDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> addWorksheetDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    addWorksheetDestroyingSpreadsheet->SetWorkbookModel(&addWorksheetDestroyingModel);
    addWorksheetDestroyingModel.SetWorksheetCountReadHandler([&addWorksheetDestroyingSpreadsheet]
    {
        addWorksheetDestroyingSpreadsheet.reset();
    });
    assert(addWorksheetDestroyingSpreadsheet->AddWorksheet() == 0);
    assert(addWorksheetDestroyingSpreadsheet == nullptr);

    WorkbookModel removeWorksheetDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> removeWorksheetDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    removeWorksheetDestroyingSpreadsheet->SetWorkbookModel(&removeWorksheetDestroyingModel);
    removeWorksheetDestroyingModel.SetWorksheetCountReadHandler([&removeWorksheetDestroyingSpreadsheet]
    {
        removeWorksheetDestroyingSpreadsheet.reset();
    });
    assert(removeWorksheetDestroyingSpreadsheet->RemoveActiveWorksheet());
    assert(removeWorksheetDestroyingSpreadsheet == nullptr);

    WorkbookModel renameWorksheetDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> renameWorksheetDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    renameWorksheetDestroyingSpreadsheet->SetWorkbookModel(&renameWorksheetDestroyingModel);
    renameWorksheetDestroyingModel.SetWorksheetCountReadHandler([&renameWorksheetDestroyingSpreadsheet]
    {
        renameWorksheetDestroyingSpreadsheet.reset();
    });
    assert(!renameWorksheetDestroyingSpreadsheet->RenameActiveWorksheet("Renamed"));
    assert(renameWorksheetDestroyingSpreadsheet == nullptr);

    WorkbookModel activeCellDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> activeCellDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    activeCellDestroyingSpreadsheet->SetWorkbookModel(&activeCellDestroyingModel);
    activeCellDestroyingModel.SetRowCountReadHandler([&activeCellDestroyingSpreadsheet]
    {
        activeCellDestroyingSpreadsheet.reset();
    });
    activeCellDestroyingSpreadsheet->SetActiveCell({1, 1});
    assert(activeCellDestroyingSpreadsheet == nullptr);

    WorkbookModel selectionDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> selectionDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    selectionDestroyingSpreadsheet->SetWorkbookModel(&selectionDestroyingModel);
    selectionDestroyingModel.SetRowCountReadHandler([&selectionDestroyingSpreadsheet]
    {
        selectionDestroyingSpreadsheet.reset();
    });
    selectionDestroyingSpreadsheet->SetSelection({{1, 1}, {2, 2}});
    assert(selectionDestroyingSpreadsheet == nullptr);

    WorkbookModel selectionColumnDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> selectionColumnDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    selectionColumnDestroyingSpreadsheet->SetWorkbookModel(&selectionColumnDestroyingModel);
    selectionColumnDestroyingModel.SetColumnCountReadHandler([&selectionColumnDestroyingSpreadsheet]
    {
        selectionColumnDestroyingSpreadsheet.reset();
    });
    selectionColumnDestroyingSpreadsheet->SetSelection({{1, 1}, {2, 2}});
    assert(selectionColumnDestroyingSpreadsheet == nullptr);

    WorkbookModel moveReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> moveReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    moveReadDestroyingSpreadsheet->SetWorkbookModel(&moveReadDestroyingModel);
    moveReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    moveReadDestroyingSpreadsheet->SetSelection({{1, 1}, {2, 2}});
    assert(moveReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {180, 47})));
    assert(moveReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerMove, {276, 84})));
    moveReadDestroyingModel.SetCellReadHandler([&moveReadDestroyingSpreadsheet]
    {
        moveReadDestroyingSpreadsheet.reset();
    });
    assert(moveReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerUp, {276, 84})));
    assert(moveReadDestroyingSpreadsheet == nullptr);

    WorkbookModel selectAllReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> selectAllReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    selectAllReadDestroyingSpreadsheet->SetWorkbookModel(&selectAllReadDestroyingModel);
    selectAllReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    selectAllReadDestroyingModel.SetRowCountReadHandler([&selectAllReadDestroyingSpreadsheet]
    {
        selectAllReadDestroyingSpreadsheet.reset();
    });
    assert(selectAllReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 10})));
    assert(selectAllReadDestroyingSpreadsheet == nullptr);

    WorkbookModel columnHeaderReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> columnHeaderReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    columnHeaderReadDestroyingSpreadsheet->SetWorkbookModel(&columnHeaderReadDestroyingModel);
    columnHeaderReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    columnHeaderReadDestroyingModel.SetRowCountReadHandler([&columnHeaderReadDestroyingSpreadsheet]
    {
        columnHeaderReadDestroyingSpreadsheet.reset();
    });
    assert(columnHeaderReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {50, 10})));
    assert(columnHeaderReadDestroyingSpreadsheet == nullptr);

    WorkbookModel rowHeaderReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> rowHeaderReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    rowHeaderReadDestroyingSpreadsheet->SetWorkbookModel(&rowHeaderReadDestroyingModel);
    rowHeaderReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    rowHeaderReadDestroyingModel.SetColumnCountReadHandler([&rowHeaderReadDestroyingSpreadsheet]
    {
        rowHeaderReadDestroyingSpreadsheet.reset();
    });
    assert(rowHeaderReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 55})));
    assert(rowHeaderReadDestroyingSpreadsheet == nullptr);

    WorkbookModel resizeMoveReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> resizeMoveReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    resizeMoveReadDestroyingSpreadsheet->SetWorkbookModel(&resizeMoveReadDestroyingModel);
    resizeMoveReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(resizeMoveReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {140, 10})));
    resizeMoveReadDestroyingModel.SetColumnWidthReadHandler([&resizeMoveReadDestroyingSpreadsheet]
    {
        resizeMoveReadDestroyingSpreadsheet.reset();
    });
    assert(resizeMoveReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerMove, {160, 10})));
    assert(resizeMoveReadDestroyingSpreadsheet == nullptr);

    WorkbookModel rowResizeMoveReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> rowResizeMoveReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    rowResizeMoveReadDestroyingSpreadsheet->SetWorkbookModel(&rowResizeMoveReadDestroyingModel);
    rowResizeMoveReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(rowResizeMoveReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 50})));
    rowResizeMoveReadDestroyingModel.SetRowHeightReadHandler([&rowResizeMoveReadDestroyingSpreadsheet]
    {
        rowResizeMoveReadDestroyingSpreadsheet.reset();
    });
    assert(rowResizeMoveReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerMove, {10, 66})));
    assert(rowResizeMoveReadDestroyingSpreadsheet == nullptr);

    WorkbookModel rowSelectionMoveReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> rowSelectionMoveReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    rowSelectionMoveReadDestroyingSpreadsheet->SetWorkbookModel(&rowSelectionMoveReadDestroyingModel);
    rowSelectionMoveReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(rowSelectionMoveReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 55})));
    rowSelectionMoveReadDestroyingModel.SetRowCountReadHandler([&rowSelectionMoveReadDestroyingSpreadsheet]
    {
        rowSelectionMoveReadDestroyingSpreadsheet.reset();
    });
    assert(rowSelectionMoveReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerMove, {10, 100})));
    assert(rowSelectionMoveReadDestroyingSpreadsheet == nullptr);

    WorkbookModel columnSelectionMoveReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> columnSelectionMoveReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    columnSelectionMoveReadDestroyingSpreadsheet->SetWorkbookModel(&columnSelectionMoveReadDestroyingModel);
    columnSelectionMoveReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(columnSelectionMoveReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {50, 10})));
    columnSelectionMoveReadDestroyingModel.SetColumnCountReadHandler([&columnSelectionMoveReadDestroyingSpreadsheet]
    {
        columnSelectionMoveReadDestroyingSpreadsheet.reset();
    });
    assert(columnSelectionMoveReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerMove, {150, 10})));
    assert(columnSelectionMoveReadDestroyingSpreadsheet == nullptr);

    WorkbookModel resizePointerUpReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> resizePointerUpReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    resizePointerUpReadDestroyingSpreadsheet->SetWorkbookModel(&resizePointerUpReadDestroyingModel);
    resizePointerUpReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(resizePointerUpReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 50})));
    assert(resizePointerUpReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerMove, {10, 66})));
    bool resizePointerUpRead{};
    resizePointerUpReadDestroyingModel.SetRowHeightReadHandler([&resizePointerUpRead]
    {
        resizePointerUpRead = true;
    });
    assert(resizePointerUpReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerUp, {10, 66})));
    assert(resizePointerUpReadDestroyingSpreadsheet != nullptr && !resizePointerUpRead);

    WorkbookModel resizePointerCancelReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> resizePointerCancelReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    resizePointerCancelReadDestroyingSpreadsheet->SetWorkbookModel(&resizePointerCancelReadDestroyingModel);
    resizePointerCancelReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(resizePointerCancelReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerDown, {10, 50})));
    assert(resizePointerCancelReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerMove, {10, 66})));
    resizePointerCancelReadDestroyingModel.SetRowHeightReadHandler([&resizePointerCancelReadDestroyingSpreadsheet]
    {
        resizePointerCancelReadDestroyingSpreadsheet.reset();
    });
    assert(resizePointerCancelReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::PointerCancel, {10, 66})));
    assert(resizePointerCancelReadDestroyingSpreadsheet == nullptr);

    WorkbookModel resizeCancelLayoutDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> resizeCancelLayoutDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    resizeCancelLayoutDestroyingSpreadsheet->SetWorkbookModel(&resizeCancelLayoutDestroyingModel);
    resizeCancelLayoutDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(resizeCancelLayoutDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {140, 10})));
    assert(resizeCancelLayoutDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerMove, {160, 10})));
    resizeCancelLayoutDestroyingModel.SetWorksheetCountReadHandler([&resizeCancelLayoutDestroyingSpreadsheet]
    {
        resizeCancelLayoutDestroyingSpreadsheet.reset();
    });
    assert(resizeCancelLayoutDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerCancel, {160, 10})));
    assert(resizeCancelLayoutDestroyingSpreadsheet == nullptr);

    WorkbookModel keyboardNavigationReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> keyboardNavigationReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    keyboardNavigationReadDestroyingSpreadsheet->SetWorkbookModel(&keyboardNavigationReadDestroyingModel);
    keyboardNavigationReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    keyboardNavigationReadDestroyingModel.SetRowCountReadHandler([&keyboardNavigationReadDestroyingSpreadsheet]
    {
        keyboardNavigationReadDestroyingSpreadsheet.reset();
    });
    assert(keyboardNavigationReadDestroyingSpreadsheet->OnEvent(test::MakeEvent(core::EventType::KeyDown, {}, core::key::Down)));
    assert(keyboardNavigationReadDestroyingSpreadsheet == nullptr);

    WorkbookModel copyReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> copyReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    ClipboardMock copyReadDestroyingClipboard;
    copyReadDestroyingSpreadsheet->SetWorkbookModel(&copyReadDestroyingModel);
    copyReadDestroyingSpreadsheet->SetClipboard(&copyReadDestroyingClipboard);
    copyReadDestroyingModel.SetCellReadHandler([&copyReadDestroyingSpreadsheet]
    {
        copyReadDestroyingSpreadsheet.reset();
    });
    assert(copyReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::KeyDown, {}, 'C', core::modifier::Control)));
    assert(copyReadDestroyingSpreadsheet == nullptr);

    WorkbookModel cutReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> cutReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    ClipboardMock cutReadDestroyingClipboard;
    cutReadDestroyingSpreadsheet->SetWorkbookModel(&cutReadDestroyingModel);
    cutReadDestroyingSpreadsheet->SetClipboard(&cutReadDestroyingClipboard);
    assert(cutReadDestroyingModel.SetCellText(cutReadDestroyingSpreadsheet->ActiveWorksheet(), {0, 0}, "cut"));
    int cutCellReads{};
    cutReadDestroyingModel.SetCellReadHandler([&cutReadDestroyingSpreadsheet, &cutCellReads]
    {
        if (++cutCellReads == 2)
            cutReadDestroyingSpreadsheet.reset();
    });
    assert(cutReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::KeyDown, {}, 'X', core::modifier::Control)));
    assert(cutReadDestroyingSpreadsheet == nullptr);

    WorkbookModel pasteDimensionReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> pasteDimensionReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    ClipboardMock pasteDimensionReadDestroyingClipboard;
    pasteDimensionReadDestroyingClipboard.text = "paste";
    pasteDimensionReadDestroyingSpreadsheet->SetWorkbookModel(&pasteDimensionReadDestroyingModel);
    pasteDimensionReadDestroyingSpreadsheet->SetClipboard(&pasteDimensionReadDestroyingClipboard);
    pasteDimensionReadDestroyingModel.SetRowCountReadHandler([&pasteDimensionReadDestroyingSpreadsheet]
    {
        pasteDimensionReadDestroyingSpreadsheet.reset();
    });
    assert(pasteDimensionReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::KeyDown, {}, 'V', core::modifier::Control)));
    assert(pasteDimensionReadDestroyingSpreadsheet == nullptr);

    WorkbookModel pasteCellReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> pasteCellReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    ClipboardMock pasteCellReadDestroyingClipboard;
    pasteCellReadDestroyingClipboard.text = "paste";
    pasteCellReadDestroyingSpreadsheet->SetWorkbookModel(&pasteCellReadDestroyingModel);
    pasteCellReadDestroyingSpreadsheet->SetClipboard(&pasteCellReadDestroyingClipboard);
    pasteCellReadDestroyingModel.SetCellReadHandler([&pasteCellReadDestroyingSpreadsheet]
    {
        pasteCellReadDestroyingSpreadsheet.reset();
    });
    assert(pasteCellReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::KeyDown, {}, 'V', core::modifier::Control)));
    assert(pasteCellReadDestroyingSpreadsheet == nullptr);

    WorkbookModel deleteReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> deleteReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    deleteReadDestroyingSpreadsheet->SetWorkbookModel(&deleteReadDestroyingModel);
    assert(deleteReadDestroyingModel.SetCellText(deleteReadDestroyingSpreadsheet->ActiveWorksheet(), {0, 0},
                                                 "delete"));
    deleteReadDestroyingModel.SetCellReadHandler([&deleteReadDestroyingSpreadsheet]
    {
        deleteReadDestroyingSpreadsheet.reset();
    });
    assert(deleteReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::KeyDown, {}, core::key::Delete)));
    assert(deleteReadDestroyingSpreadsheet == nullptr);

    WorkbookModel undoMetricsReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> undoMetricsReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    undoMetricsReadDestroyingSpreadsheet->SetWorkbookModel(&undoMetricsReadDestroyingModel);
    undoMetricsReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(undoMetricsReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {140, 10})));
    assert(undoMetricsReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerMove, {160, 10})));
    assert(undoMetricsReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerUp, {160, 10})));
    undoMetricsReadDestroyingModel.SetColumnWidthReadHandler([&undoMetricsReadDestroyingSpreadsheet]
    {
        undoMetricsReadDestroyingSpreadsheet.reset();
    });
    undoMetricsReadDestroyingSpreadsheet->Undo();
    assert(undoMetricsReadDestroyingSpreadsheet == nullptr);

    WorkbookModel redoMetricsReadDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> redoMetricsReadDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    redoMetricsReadDestroyingSpreadsheet->SetWorkbookModel(&redoMetricsReadDestroyingModel);
    redoMetricsReadDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(redoMetricsReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerDown, {140, 10})));
    assert(redoMetricsReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerMove, {160, 10})));
    assert(redoMetricsReadDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::PointerUp, {160, 10})));
    redoMetricsReadDestroyingSpreadsheet->Undo();
    redoMetricsReadDestroyingModel.SetColumnWidthReadHandler([&redoMetricsReadDestroyingSpreadsheet]
    {
        redoMetricsReadDestroyingSpreadsheet.reset();
    });
    redoMetricsReadDestroyingSpreadsheet->Redo();
    assert(redoMetricsReadDestroyingSpreadsheet == nullptr);

    WorkbookModel escapeDestroyingModel;
    std::unique_ptr<controls::list::DuiSpreadsheet> escapeDestroyingSpreadsheet =
        std::make_unique<controls::list::DuiSpreadsheet>();
    TextInputMock escapeDestroyingInput;
    escapeDestroyingSpreadsheet->SetWorkbookModel(&escapeDestroyingModel);
    escapeDestroyingSpreadsheet->SetTextInput(&escapeDestroyingInput);
    assert(escapeDestroyingSpreadsheet->BeginEdit());
    escapeDestroyingInput.visibleChanged = [&escapeDestroyingSpreadsheet](bool visible)
    {
        if (!visible)
            escapeDestroyingSpreadsheet.reset();
    };
    assert(escapeDestroyingSpreadsheet->OnEvent(
        test::MakeEvent(core::EventType::KeyDown, {}, core::key::Escape)));
    assert(escapeDestroyingSpreadsheet == nullptr);

    WorkbookModel cancelInputDestroyingModel;
    controls::list::DuiSpreadsheet cancelInputDestroyingSpreadsheet;
    auto cancelDestroyingInput = std::make_unique<TextInputMock>();
    cancelInputDestroyingSpreadsheet.SetWorkbookModel(&cancelInputDestroyingModel);
    cancelInputDestroyingSpreadsheet.SetTextInput(cancelDestroyingInput.get());
    assert(cancelInputDestroyingSpreadsheet.BeginEdit());
    cancelDestroyingInput->visibleChanged = [&cancelDestroyingInput](bool visible)
    {
        if (!visible)
            cancelDestroyingInput.reset();
    };
    cancelInputDestroyingSpreadsheet.CancelEdit();
    assert(!cancelInputDestroyingSpreadsheet.Editing() && cancelDestroyingInput == nullptr);

    auto detachDestroyingSpreadsheet = std::make_unique<controls::list::DuiSpreadsheet>();
    auto detachDestroyingInput = std::make_unique<TextInputMock>();
    detachDestroyingSpreadsheet->SetTextInput(detachDestroyingInput.get());
    detachDestroyingInput->changedHandlerChanged = [&detachDestroyingInput]
    {
        detachDestroyingInput.reset();
    };
    detachDestroyingSpreadsheet.reset();
    assert(detachDestroyingInput == nullptr);

    auto inputOutlivingSpreadsheet = std::make_unique<controls::list::DuiSpreadsheet>();
    {
        TextInputMock temporaryInput;
        inputOutlivingSpreadsheet->SetTextInput(&temporaryInput);
    }
    inputOutlivingSpreadsheet.reset();

    WorkbookModel findDestroyingModel;
    findDestroyingModel.SetCapabilities(DuiWorkbookCapability::Find);
    findDestroyingModel.SetFindResult({{}, DuiCellAddress{1, 1}});
    auto findDestroyingSpreadsheet = std::make_unique<controls::list::DuiSpreadsheet>();
    findDestroyingSpreadsheet->SetWorkbookModel(&findDestroyingModel);
    findDestroyingModel.SetOperationHandler([&findDestroyingSpreadsheet]
    {
        findDestroyingSpreadsheet.reset();
    });
    const DuiWorkbookFindResult destroyedFind = findDestroyingSpreadsheet->FindNext("value");
    assert(destroyedFind.operation && findDestroyingSpreadsheet == nullptr);

    WorkbookModel replaceDestroyingModel;
    replaceDestroyingModel.SetCapabilities(DuiWorkbookCapability::Replace);
    replaceDestroyingModel.SetReplaceResult({{}, 1, DuiCellAddress{1, 1}});
    auto replaceDestroyingSpreadsheet = std::make_unique<controls::list::DuiSpreadsheet>();
    replaceDestroyingSpreadsheet->SetWorkbookModel(&replaceDestroyingModel);
    replaceDestroyingModel.SetOperationHandler([&replaceDestroyingSpreadsheet]
    {
        replaceDestroyingSpreadsheet.reset();
    });
    const DuiWorkbookReplaceResult destroyedReplace = replaceDestroyingSpreadsheet->ReplaceText(
        "before", "after", true);
    assert(destroyedReplace.operation && replaceDestroyingSpreadsheet == nullptr);

    WorkbookModel autoFitDestroyingModel;
    autoFitDestroyingModel.SetPreferredColumnWidth(180);
    auto autoFitDestroyingSpreadsheet = std::make_unique<controls::list::DuiSpreadsheet>();
    autoFitDestroyingSpreadsheet->SetWorkbookModel(&autoFitDestroyingModel);
    autoFitDestroyingModel.SetOperationHandler([&autoFitDestroyingSpreadsheet]
    {
        autoFitDestroyingSpreadsheet.reset();
    });
    const DuiWorkbookOperationResult destroyedAutoFit = autoFitDestroyingSpreadsheet->AutoFitColumn(0);
    assert(!destroyedAutoFit && autoFitDestroyingSpreadsheet == nullptr);

    WorkbookModel requestDestroyingModel;
    requestDestroyingModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    requestDestroyingModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    auto requestDestroyingSpreadsheet = std::make_unique<controls::list::DuiSpreadsheet>();
    requestDestroyingSpreadsheet->SetWorkbookModel(&requestDestroyingModel);
    requestDestroyingModel.SetRequestCellRangesHandler([&requestDestroyingSpreadsheet]
    {
        requestDestroyingSpreadsheet.reset();
    });
    requestDestroyingSpreadsheet->Layout({0, 0, 500, 300});
    assert(requestDestroyingSpreadsheet == nullptr && requestDestroyingModel.RequestCellRangesCalls() == 1);

    WorkbookModel requestReplacingModel;
    requestReplacingModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    requestReplacingModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    WorkbookModel replacementRequestModel;
    replacementRequestModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    replacementRequestModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    controls::list::DuiSpreadsheet requestReplacingSpreadsheet;
    requestReplacingSpreadsheet.SetWorkbookModel(&requestReplacingModel);
    requestReplacingModel.SetRequestCellRangesHandler([&requestReplacingSpreadsheet, &replacementRequestModel]
    {
        requestReplacingSpreadsheet.SetWorkbookModel(&replacementRequestModel);
    });
    requestReplacingSpreadsheet.Layout({0, 0, 500, 300});
    assert(requestReplacingSpreadsheet.WorkbookModel() == &replacementRequestModel);
    assert(requestReplacingModel.RequestCellRangesCalls() == 1);
    assert(replacementRequestModel.RequestCellRangesCalls() == 1);

    WorkbookModel synchronousRequestModel;
    synchronousRequestModel.SetSparseRowSizes(DuiSparseAxisSizes{24, {}});
    synchronousRequestModel.SetSparseColumnSizes(DuiSparseAxisSizes{96, {}});
    controls::list::DuiSpreadsheet synchronousRequestSpreadsheet;
    synchronousRequestSpreadsheet.SetWorkbookModel(&synchronousRequestModel);
    synchronousRequestSpreadsheet.Layout({0, 0, 500, 300});
    synchronousRequestModel.ResetCellRangeRequests();
    synchronousRequestModel.SetRequestCellRangesHandler([&synchronousRequestModel]
    {
        synchronousRequestModel.NotifyChanged();
    });
    assert(synchronousRequestSpreadsheet.OnEvent(
        test::MakeEvent(core::EventType::PointerWheel, {200, 100}, {}, core::modifier::Shift, {}, -1)));
    assert(synchronousRequestModel.RequestCellRangesCalls() == 1);
    return 0;
}
