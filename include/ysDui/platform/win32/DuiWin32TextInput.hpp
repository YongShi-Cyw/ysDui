#pragma once

#include <memory>

#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/ui/DuiTextInput.hpp"

namespace ysDui::platform::win32 {

class Win32TextInput final : public ui::DuiTextInput {
public:
    explicit Win32TextInput(NativeWindowHandle parent);
    ~Win32TextInput() override;
    Win32TextInput(const Win32TextInput&) = delete;
    Win32TextInput& operator=(const Win32TextInput&) = delete;
    Win32TextInput(Win32TextInput&&) noexcept;
    Win32TextInput& operator=(Win32TextInput&&) noexcept;
    void SetBounds(core::Rect bounds) override;
    [[nodiscard]] core::Rect Bounds() const override;
    void SetVisible(bool visible) override;
    void SetEnabled(bool enabled) override;
    void SetBorderVisible(bool visible) override;
    void SetOptions(const ui::DuiTextInputOptions& options) override;
    void SetPlaceholder(std::string text) override;
    void SetText(std::string text) override;
    [[nodiscard]] std::string Text() const override;
    [[nodiscard]] std::size_t CaretPosition() const override;
    [[nodiscard]] bool CaretVisible() const override;
    [[nodiscard]] ui::DuiTextSelection TextSelection() const override;
    void Focus() override;
    void FocusAt(core::Point position) override;
    void BeginSelection(core::Point position) override;
    void UpdateSelection(core::Point position) override;
    void EndSelection() override;
    void SelectWordAt(core::Point position) override;
    void SetChangedHandler(std::function<void()> handler) override;
    void SetFocusLostHandler(std::function<void()> handler) override;
    void SetSubmitHandler(std::function<void()> handler) override;
    void SetCancelHandler(std::function<void()> handler) override;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
