#pragma once

#include <memory>

#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/ui/DuiRichTextInput.hpp"

namespace ysDui::platform::win32 {

class Win32RichTextInput final : public ui::DuiRichTextInput {
public:
    explicit Win32RichTextInput(NativeWindowHandle parent);
    ~Win32RichTextInput() override;
    Win32RichTextInput(const Win32RichTextInput&) = delete;
    Win32RichTextInput& operator=(const Win32RichTextInput&) = delete;
    Win32RichTextInput(Win32RichTextInput&&) noexcept;
    Win32RichTextInput& operator=(Win32RichTextInput&&) noexcept;
    void SetBounds(core::Rect bounds) override;
    [[nodiscard]] core::Rect Bounds() const override;
    void SetVisible(bool visible) override;
    void SetEnabled(bool enabled) override;
    void SetBorderVisible(bool visible) override;
    void SetOptions(const ui::DuiTextInputOptions& options) override;
    void SetPlaceholder(std::string text) override;
    void SetText(std::string text) override;
    [[nodiscard]] std::string Text() const override;
    void Focus() override;
    void SetChangedHandler(std::function<void()> handler) override;
    void SetFocusLostHandler(std::function<void()> handler) override;
    void SetSelection(ui::DuiTextRange range) override;
    [[nodiscard]] ui::DuiTextRange Selection() const override;
    void SelectAll() override;
    void ReplaceSelection(std::string text) override;
    void AppendText(std::string text) override;
    [[nodiscard]] bool CanUndo() const override;
    void Undo() override;
    void Cut() override;
    void Copy() override;
    void Paste() override;
    void ClearSelection() override;
    void SetSelectionFormat(ui::DuiRichTextFormat format) override;
    void SetAutomaticLinkDetection(bool enabled) override;
    void SetLinkActivatedHandler(std::function<void(std::string)> handler) override;
    [[nodiscard]] bool InsertImage(const render::DuiImage& image, core::Size maximumSize) override;
    void InsertQuoteBlock(std::string sender, std::string body) override;
    void InsertFileCard(std::string fileName, std::uint64_t sizeBytes) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
