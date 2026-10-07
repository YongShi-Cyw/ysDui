#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiRichTextInput.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

class DuiRichEditHost final : public core::Control, public render::DuiRenderable {
public:
    DuiRichEditHost();
    ~DuiRichEditHost() override;
    DuiRichEditHost(const DuiRichEditHost&) = delete;
    DuiRichEditHost& operator=(const DuiRichEditHost&) = delete;
    DuiRichEditHost(DuiRichEditHost&&) noexcept;
    DuiRichEditHost& operator=(DuiRichEditHost&&) noexcept;

    void SetRichTextInput(ui::DuiRichTextInput* input);
    void SetText(std::string text, bool notify = false);
    [[nodiscard]] const std::string& Text() const;
    void SetPlaceholder(std::string text);
    void SetOptions(ui::DuiTextInputOptions options);
    [[nodiscard]] ui::DuiTextInputOptions Options() const;
    void SetSelection(ui::DuiTextRange range);
    [[nodiscard]] ui::DuiTextRange Selection() const;
    void SelectAll();
    void ReplaceSelection(std::string text);
    void AppendText(std::string text);
    [[nodiscard]] bool CanUndo() const;
    void Undo();
    void Cut();
    void Copy();
    void Paste();
    void ClearSelection();
    void SetSelectionFormat(ui::DuiRichTextFormat format);
    void SetAutomaticLinkDetection(bool enabled);
    [[nodiscard]] bool InsertImage(const render::DuiImage& image, core::Size maximumSize = {});
    void InsertQuoteBlock(std::string sender, std::string body);
    void InsertFileCard(std::string fileName, std::uint64_t sizeBytes);
    void SetTextChangedHandler(std::function<void(std::string_view)> handler);
    void SetLinkActivatedHandler(std::function<void(std::string_view)> handler);
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    void SetVisible(bool visible);
    void SetEnabled(bool enabled);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> richEditHost_;
};

} // namespace ysDui::controls::input
