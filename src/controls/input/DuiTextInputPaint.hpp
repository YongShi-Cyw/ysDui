/**
 * 文件名：DuiTextInputPaint.hpp
 * 开发者：青蓝
 * 开发时间：2026-08-03
 * 用途：为使用平台输入代理的控件统一绘制文本与单行光标。
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/ui/DuiTextInput.hpp"

namespace ysDui::controls::detail {

inline std::size_t Utf8CharacterCount(std::string_view text)
{
    std::size_t count{};
    for (const unsigned char value : text)
    {
        if ((value & 0xC0U) != 0x80U)
            ++count;
    }
    return count;
}

inline std::string PasswordMask(std::string_view text)
{
    return std::string(Utf8CharacterCount(text), '*');
}

inline std::size_t ClampUtf8Boundary(std::string_view text, std::size_t position)
{
    position = (std::min)(position, text.size());
    while (position > 0 && position < text.size()
           && (static_cast<unsigned char>(text[position]) & 0xC0U) == 0x80U)
    {
        --position;
    }
    return position;
}

inline void PaintTextInput(render::Canvas& canvas, ui::DuiTextInput* input,
                           std::string_view fallbackValue, std::string_view placeholder,
                           core::Rect bounds, render::DuiTextStyle style,
                           core::Color placeholderColor, render::DuiTextAlignment alignment,
                           bool wordWrap, bool showCaret, core::Color selectionBackground,
                           core::Color selectionText, bool password = false)
{
    const std::string value = input != nullptr ? input->Text() : std::string(fallbackValue);
    const std::string display = value.empty()
        ? std::string(placeholder)
        : password ? PasswordMask(value) : value;
    render::DuiTextStyle displayStyle = style;
    if (value.empty())
        displayStyle.color = placeholderColor;
    canvas.DrawText(display, bounds, displayStyle, alignment, wordWrap);

    // 选区绘制不依赖 showCaret：只读/多行字段只隐藏插入光标，仍应显示选中内容
    if (input == nullptr || bounds.Width() <= 0 || bounds.Height() <= 0)
        return;

    const std::string renderedValue = password ? PasswordMask(value) : value;
    const int textWidth = canvas.MeasureText(renderedValue, style, {}).size.width;
    int textLeft = bounds.left;
    if (alignment == render::DuiTextAlignment::Center)
        textLeft += (bounds.Width() - textWidth) / 2;
    else if (alignment == render::DuiTextAlignment::End)
        textLeft = bounds.right - textWidth;

    ui::DuiTextSelection selection = input->TextSelection();
    selection.start = ClampUtf8Boundary(value, selection.start);
    selection.end = ClampUtf8Boundary(value, selection.end);
    if (selection.start > selection.end)
        std::swap(selection.start, selection.end);
    if (selection.start != selection.end)
    {
        const std::string renderedPrefix = password
            ? PasswordMask(std::string_view(value).substr(0, selection.start))
            : value.substr(0, selection.start);
        const std::string renderedSelection = password
            ? PasswordMask(std::string_view(value).substr(
                selection.start, selection.end - selection.start))
            : value.substr(selection.start, selection.end - selection.start);
        const int prefixWidth = canvas.MeasureText(renderedPrefix, style, {}).size.width;
        const int selectionWidth = canvas.MeasureText(renderedSelection, style, {}).size.width;
        const int selectionLeft = std::clamp(textLeft + prefixWidth, bounds.left, bounds.right);
        const int selectionRight = std::clamp(selectionLeft + selectionWidth,
                                              selectionLeft, bounds.right);
        const core::Rect selectionBounds{selectionLeft, bounds.top, selectionRight, bounds.bottom};
        if (!selectionBounds.Empty())
        {
            canvas.FillRect(selectionBounds, selectionBackground);
            render::DuiTextStyle selectedStyle = style;
            selectedStyle.color = selectionText;
            canvas.PushClip(selectionBounds);
            canvas.DrawText(renderedSelection,
                            {selectionLeft, bounds.top, bounds.right, bounds.bottom},
                            selectedStyle, render::DuiTextAlignment::Start, false);
            canvas.PopClip();
        }
        return;
    }

    // 仅在无选区且允许显示光标时才绘制插入光标
    if (!showCaret || !input->CaretVisible())
        return;

    const std::size_t caretPosition = ClampUtf8Boundary(value, input->CaretPosition());
    const std::string renderedPrefix = password
        ? PasswordMask(std::string_view(value).substr(0, caretPosition))
        : value.substr(0, caretPosition);
    const int prefixWidth = canvas.MeasureText(renderedPrefix, style, {}).size.width;
    const int x = std::clamp(textLeft + prefixWidth, bounds.left, bounds.right - 1);
    const int measuredHeight = canvas.MeasureText("Mg", style, {}).size.height;
    const int caretHeight = std::clamp(measuredHeight, 1, bounds.Height());
    const int top = bounds.top + (bounds.Height() - caretHeight) / 2;
    const int bottom = top + caretHeight;
    if (bottom > top)
        canvas.FillRect({x, top, x + 1, bottom}, style.color);
}

} // namespace ysDui::controls::detail
