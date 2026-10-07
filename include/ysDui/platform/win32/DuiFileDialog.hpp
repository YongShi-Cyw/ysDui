#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ysDui::platform::win32 {

enum class DuiFileDialogMode {
    Open,
    Save,
};

struct DuiFileFilter final {
    std::string description;
    std::string pattern;
};

struct DuiFileDialogOptions final {
    DuiFileDialogMode mode{DuiFileDialogMode::Open};
    std::string title;
    std::string initialDirectory;
    std::string initialFileName;
    std::string defaultExtension;
    std::vector<DuiFileFilter> filters;
    bool allowMultiple{};
};

struct DuiFileDialogResult final {
    bool accepted{};
    std::vector<std::string> paths;
};

class DuiFileDialog final {
public:
    DuiFileDialog();
    ~DuiFileDialog();
    DuiFileDialog(const DuiFileDialog&) = delete;
    DuiFileDialog& operator=(const DuiFileDialog&) = delete;
    DuiFileDialog(DuiFileDialog&&) noexcept;
    DuiFileDialog& operator=(DuiFileDialog&&) noexcept;

    [[nodiscard]] DuiFileDialogResult Show(const DuiFileDialogOptions& options) const;

private:
    class Impl;
    std::unique_ptr<Impl> dialog_;
};

} // namespace ysDui::platform::win32
