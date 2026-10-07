/**
 * 文件名：DuiRichTextInput.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明平台无关的富文本输入能力。
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/ui/DuiTextInput.hpp"

namespace ysDui::ui {

struct DuiTextRange final
{
    int start{};
    int end{};
    constexpr bool operator==(const DuiTextRange&) const = default;
};

struct DuiRichTextFormat final
{
    core::Color color{30, 30, 30, 255};
    bool bold{};
    bool italic{};
    bool underline{};
    constexpr bool operator==(const DuiRichTextFormat&) const = default;
};

class DuiRichTextInput : public DuiTextInput
{
public:
    ~DuiRichTextInput() override = default;
    virtual void SetSelection(DuiTextRange range) = 0;
    [[nodiscard]] virtual DuiTextRange Selection() const = 0;
    virtual void SelectAll() = 0;
    virtual void ReplaceSelection(std::string text) = 0;
    virtual void AppendText(std::string text) = 0;
    [[nodiscard]] virtual bool CanUndo() const = 0;
    virtual void Undo() = 0;
    virtual void Cut() = 0;
    virtual void Copy() = 0;
    virtual void Paste() = 0;
    virtual void ClearSelection() = 0;
    virtual void SetSelectionFormat(DuiRichTextFormat format) = 0;
    virtual void SetAutomaticLinkDetection(bool enabled) = 0;
    virtual void SetLinkActivatedHandler(std::function<void(std::string)> handler) = 0;
    [[nodiscard]] virtual bool InsertImage(const render::DuiImage& image,
                                           core::Size maximumSize) = 0;
    virtual void InsertQuoteBlock(std::string sender, std::string body) = 0;
    virtual void InsertFileCard(std::string fileName, std::uint64_t sizeBytes) = 0;
};

} // namespace ysDui::ui
