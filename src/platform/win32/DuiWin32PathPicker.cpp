#include "ysDui/platform/win32/DuiWin32PathPicker.hpp"
#include "DuiWin32Utf8.hpp"

#include <windows.h>
#include <shlobj.h>

#include <string>
#include <utility>

#include "ysDui/platform/win32/DuiFileDialog.hpp"

namespace ysDui::platform::win32 {
class Win32PathPicker::Impl {};

Win32PathPicker::Win32PathPicker() : impl_(std::make_unique<Impl>()) {}
Win32PathPicker::~Win32PathPicker() = default;
Win32PathPicker::Win32PathPicker(Win32PathPicker&&) noexcept = default;
Win32PathPicker& Win32PathPicker::operator=(Win32PathPicker&&) noexcept = default;

ui::DuiPathPickerResult Win32PathPicker::Browse(ui::DuiPathPickerMode mode, std::string_view initialPath)
{
    if (mode == ui::DuiPathPickerMode::OpenFile || mode == ui::DuiPathPickerMode::SaveFile) {
        DuiFileDialog dialog;
        DuiFileDialogOptions options;
        options.mode = mode == ui::DuiPathPickerMode::SaveFile ? DuiFileDialogMode::Save : DuiFileDialogMode::Open;
        if (mode == ui::DuiPathPickerMode::SaveFile)
            options.initialFileName = std::string(initialPath);
        else
            options.initialDirectory = std::string(initialPath);
        const DuiFileDialogResult result = dialog.Show(options);
        return {result.accepted && !result.paths.empty(), result.paths.empty() ? std::string{} : result.paths.front()};
    }
    BROWSEINFOW definition{};
    definition.hwndOwner = ::GetActiveWindow();
    definition.lpszTitle = L"Select folder";
    definition.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE item = ::SHBrowseForFolderW(&definition);
    if (item == nullptr) return {};
    wchar_t path[MAX_PATH]{};
    const bool accepted = ::SHGetPathFromIDListW(item, path) != FALSE;
    ::CoTaskMemFree(item);
    return {accepted, accepted ? detail::WideToUtf8(path) : std::string{}};
}

} // namespace ysDui::platform::win32
