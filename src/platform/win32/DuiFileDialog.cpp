#include "ysDui/platform/win32/DuiFileDialog.hpp"
#include "DuiWin32Utf8.hpp"

#include <algorithm>
#include <windows.h>
#include <commdlg.h>

#include <string_view>
#include <utility>
#include <vector>

namespace ysDui::platform::win32 {
namespace {
std::vector<wchar_t> buildFilter(const std::vector<DuiFileFilter>& filters)
{
    std::vector<wchar_t> result;
    for (const auto& filter : filters)
    {
        const std::wstring description = detail::Utf8ToWide(filter.description);
        const std::wstring pattern = detail::Utf8ToWide(filter.pattern);
        result.insert(result.end(), description.begin(), description.end());
        result.push_back(L'\0');
        result.insert(result.end(), pattern.begin(), pattern.end());
        result.push_back(L'\0');
    }
    if (!result.empty()) result.push_back(L'\0');
    return result;
}

DuiFileDialogResult readSelection(const std::vector<wchar_t>& buffer)
{
    DuiFileDialogResult result;
    if (buffer.empty() || buffer.front() == L'\0') return result;
    const wchar_t* current = buffer.data();
    const std::wstring first(current);
    current += first.size() + 1;
    result.accepted = true;
    if (*current == L'\0') {
        result.paths.push_back(detail::WideToUtf8(first));
        return result;
    }
    while (*current != L'\0') {
        const std::wstring leaf(current);
        result.paths.push_back(detail::WideToUtf8(first + L"\\" + leaf));
        current += leaf.size() + 1;
    }
    return result;
}
} // namespace

class DuiFileDialog::Impl {};

DuiFileDialog::DuiFileDialog() : dialog_(std::make_unique<Impl>()) {}
DuiFileDialog::~DuiFileDialog() = default;
DuiFileDialog::DuiFileDialog(DuiFileDialog&&) noexcept = default;
DuiFileDialog& DuiFileDialog::operator=(DuiFileDialog&&) noexcept = default;

DuiFileDialogResult DuiFileDialog::Show(const DuiFileDialogOptions& options) const
{
    constexpr std::size_t bufferSize = 32768;
    std::vector<wchar_t> pathBuffer(bufferSize);
    const std::wstring title = detail::Utf8ToWide(options.title);
    const std::wstring initialDirectory = detail::Utf8ToWide(options.initialDirectory);
    const std::wstring initialFileName = detail::Utf8ToWide(options.initialFileName);
    const std::wstring defaultExtension = detail::Utf8ToWide(options.defaultExtension);
    const std::vector<wchar_t> filter = buildFilter(options.filters);

    OPENFILENAMEW native{};
    native.lStructSize = sizeof(native);
    native.hwndOwner = ::GetActiveWindow();
    native.lpstrFilter = filter.empty() ? nullptr : filter.data();
    native.lpstrFile = pathBuffer.data();
    native.nMaxFile = static_cast<DWORD>(pathBuffer.size());
    native.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
    native.lpstrTitle = title.empty() ? nullptr : title.c_str();
    native.lpstrDefExt = defaultExtension.empty() ? nullptr : defaultExtension.c_str();
    native.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST;
    if (!initialFileName.empty())
        std::copy_n(initialFileName.c_str(), (std::min)(initialFileName.size(), pathBuffer.size() - 1), pathBuffer.data());
    if (options.mode == DuiFileDialogMode::Open) {
        native.Flags |= OFN_FILEMUSTEXIST;
        if (options.allowMultiple) native.Flags |= OFN_ALLOWMULTISELECT;
        if (!::GetOpenFileNameW(&native)) return {};
    } else {
        native.Flags |= OFN_OVERWRITEPROMPT;
        if (!::GetSaveFileNameW(&native)) return {};
    }
    return readSelection(pathBuffer);
}

} // namespace ysDui::platform::win32
