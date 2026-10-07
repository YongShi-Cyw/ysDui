/**
 * 文件名：DuiSpreadsheetSheets.cpp
 * 开发者：青蓝
 * 开发时间：2026-08-16
 * 用途：实现工作表标签重命名与结构管理菜单。
 */
#include "DuiSpreadsheetInternal.hpp"

#include <string>

#include "ysDui/controls/list/DuiWorkbookSheetOperations.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"

namespace ysDui::controls::list {

std::optional<bool> DuiSpreadsheet::WorksheetNameExists(
    Impl& data, IDuiWorkbookModel* expectedModel, DuiWorksheetId expectedSheet,
    std::string_view name, DuiWorksheetId excludedSheet)
{
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    const int count = expectedModel->WorksheetCount();
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return std::nullopt;
    for (int index = 0; index < count; ++index)
    {
        const DuiWorksheetId sheet = expectedModel->WorksheetIdAt(index);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return std::nullopt;
        if (sheet == excludedSheet)
            continue;
        const std::string existingName = expectedModel->WorksheetName(sheet);
        if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
            return std::nullopt;
        if (existingName == name)
            return true;
    }
    return false;
}

std::optional<std::string> DuiSpreadsheet::UniqueWorksheetName(
    Impl& data, IDuiWorkbookModel* expectedModel, DuiWorksheetId expectedSheet,
    std::string baseName, int firstSuffix)
{
    int suffix = firstSuffix;
    for (;;)
    {
        const std::string candidate = suffix > 0
            ? baseName + " (" + std::to_string(suffix) + ")"
            : baseName;
        const auto exists = WorksheetNameExists(
            data, expectedModel, expectedSheet, candidate);
        if (!exists)
            return std::nullopt;
        if (!*exists)
            return candidate;
        suffix = suffix > 0 ? suffix + 1 : 2;
    }
}

bool DuiSpreadsheet::BeginWorksheetRename(DuiWorksheetId sheet, core::Rect bounds)
{
    auto& data = *spreadsheet_;
    if (data.model == nullptr || sheet == 0 || !data.HasTextInput())
        return false;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    ui::DuiTextInput* const expectedInput = data.textInput;
    const auto contextMatches = [&]
    {
        return !lifetime.expired() && data.model == expectedModel && data.textInput == expectedInput
            && data.HasTextInput();
    };
    CommitEdit();
    if (!contextMatches() || data.editing)
        return false;
    const std::string name = expectedModel->WorksheetName(sheet);
    if (!contextMatches())
        return false;
    data.renameSheet = sheet;
    data.renameSheetBounds = {bounds.left + 4, bounds.top + 1, bounds.right - 4, bounds.bottom - 1};
    data.renamingSheet = true;
    expectedInput->SetText(name);
    if (!contextMatches()) return false;
    expectedInput->SetOptions({false, false, false, false, 0});
    if (!contextMatches()) return false;
    expectedInput->SetBorderVisible(false);
    if (!contextMatches()) return false;
    expectedInput->SetVisible(true);
    if (!contextMatches()) return false;
    PlaceTextInput();
    if (!contextMatches()) return false;
    expectedInput->Focus();
    return contextMatches() && data.renamingSheet && data.renameSheet == sheet;
}

void DuiSpreadsheet::CommitWorksheetRename()
{
    auto& data = *spreadsheet_;
    if (!data.renamingSheet || data.model == nullptr || !data.HasTextInput())
        return;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    ui::DuiTextInput* const expectedInput = data.textInput;
    const DuiWorksheetId sheet = data.renameSheet;
    const std::string name = expectedInput->Text();
    if (lifetime.expired() || data.model != expectedModel || data.textInput != expectedInput
        || !data.HasTextInput() || !data.renamingSheet || data.renameSheet != sheet)
        return;
    if (name.empty())
    {
        CancelWorksheetRename();
        return;
    }
    const auto nameExists = WorksheetNameExists(
        data, expectedModel, data.sheet, name, sheet);
    if (!nameExists || *nameExists)
        return;
    bool renamed{};
    {
        Impl::ModelUpdateScope update(data);
        renamed = expectedModel->RenameWorksheet(sheet, name);
    }
    if (lifetime.expired() || data.model != expectedModel || data.textInput != expectedInput
        || !data.HasTextInput() || !data.renamingSheet || data.renameSheet != sheet || !renamed)
        return;
    data.renamingSheet = false;
    data.renameSheet = 0;
    data.renameSheetBounds = {};
    expectedInput->SetVisible(false);
}

void DuiSpreadsheet::CancelWorksheetRename()
{
    auto& data = *spreadsheet_;
    if (!data.renamingSheet)
        return;
    data.renamingSheet = false;
    data.renameSheet = 0;
    data.renameSheetBounds = {};
    if (data.HasTextInput())
        data.textInput->SetVisible(false);
}

bool DuiSpreadsheet::ShowSheetContextMenu(DuiWorksheetId sheet, core::Rect anchor)
{
    auto& data = *spreadsheet_;
    if (!data.popupFactory || !data.sheetContextMenu || data.model == nullptr || sheet == 0)
        return false;
    data.contextSheet = sheet;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    DuiMenu& menu = *data.sheetContextMenu;
    menu.ClearItems();
    menu.AddItem(static_cast<std::uint32_t>(SheetCommand::Insert), "Insert worksheet");
    menu.AddItem(static_cast<std::uint32_t>(SheetCommand::Delete), "Delete worksheet");
    menu.AddItem(static_cast<std::uint32_t>(SheetCommand::Duplicate), "Create copy");
    const bool supportsStructureOperations =
        dynamic_cast<IDuiWorkbookSheetOperations*>(expectedModel) != nullptr;
    const int worksheetCount = expectedModel->WorksheetCount();
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return true;
    menu.SetItemEnabled(static_cast<std::uint32_t>(SheetCommand::Delete), worksheetCount > 1);
    menu.SetItemEnabled(static_cast<std::uint32_t>(SheetCommand::Insert), supportsStructureOperations);
    menu.SetItemEnabled(static_cast<std::uint32_t>(SheetCommand::Duplicate), supportsStructureOperations);
    return menu.Show(*data.popupFactory, data.popupOwner, anchor);
}

void DuiSpreadsheet::InvokeSheetContextMenuCommand(std::uint32_t command)
{
    auto& data = *spreadsheet_;
    if (data.model == nullptr || data.contextSheet == 0)
        return;
    const DuiWorksheetId target = data.contextSheet;
    const std::weak_ptr<int> lifetime = data.objectLifetime;
    IDuiWorkbookModel* const expectedModel = data.model;
    const DuiWorksheetId expectedSheet = data.sheet;
    if (static_cast<SheetCommand>(command) == SheetCommand::Delete)
    {
        if (target != data.sheet && !SetActiveWorksheet(target))
            return;
        if (lifetime.expired())
            return;
        (void)RemoveActiveWorksheet();
        return;
    }

    const int count = expectedModel->WorksheetCount();
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
        return;
    DuiWorksheetId created{};
    auto* const operations = dynamic_cast<IDuiWorkbookSheetOperations*>(expectedModel);
    if (operations == nullptr)
        return;
    {
        Impl::ModelUpdateScope update(data);
        if (static_cast<SheetCommand>(command) == SheetCommand::Insert)
        {
            const auto name = UniqueWorksheetName(
                data, expectedModel, expectedSheet, "Sheet" + std::to_string(count + 1));
            if (!name)
                return;
            int targetIndex = -1;
            for (int index = 0; index < count; ++index)
            {
                const DuiWorksheetId sheet = expectedModel->WorksheetIdAt(index);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return;
                if (sheet == target)
                    targetIndex = index;
            }
            if (targetIndex < 0)
                return;
            if (targetIndex + 1 < count)
            {
                const DuiWorksheetId next = expectedModel->WorksheetIdAt(targetIndex + 1);
                if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                    return;
                created = operations->InsertWorksheetBefore(next, *name);
            }
            else
                created = expectedModel->AddWorksheet(*name);
        }
        else if (static_cast<SheetCommand>(command) == SheetCommand::Duplicate)
        {
            const std::string sourceName = expectedModel->WorksheetName(target);
            if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet))
                return;
            const auto name = UniqueWorksheetName(
                data, expectedModel, expectedSheet, sourceName, 2);
            if (!name)
                return;
            created = operations->DuplicateWorksheet(target, *name);
        }
    }
    if (lifetime.expired() || !data.HasModelContext(expectedModel, expectedSheet) || created == 0)
        return;
    (void)SetActiveWorksheet(created);
}

} // namespace ysDui::controls::list
