#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/ui/DuiHostRef.hpp"

namespace ysDui::ui {
class IUiHostFactory;
}

namespace ysDui::controls::window {

enum class DuiMessageBoxType {
    Error,
    Warning,
    Information,
    Question,
};

struct DuiMessageBoxButton final {
    std::string text;
    int result{};
};

class DuiMessageBox final {
public:
    DuiMessageBox();
    ~DuiMessageBox();
    DuiMessageBox(const DuiMessageBox&) = delete;
    DuiMessageBox& operator=(const DuiMessageBox&) = delete;
    DuiMessageBox(DuiMessageBox&&) noexcept;
    DuiMessageBox& operator=(DuiMessageBox&&) noexcept;

    void SetTitle(std::string value);
    void SetMessage(std::string value);
    void SetType(DuiMessageBoxType value);
    void SetButtons(std::vector<DuiMessageBoxButton> value);
    void SetDefaultButton(int result);
    void SetCloseResult(int result);
    void SetClosedHandler(std::function<void(int)> handler);
    bool Show(ui::IUiHostFactory& factory, ui::HostRef owner, core::Rect anchor);
    void Close(int result);
    [[nodiscard]] int Result() const;
    [[nodiscard]] bool Visible() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::controls::window
