/**
 * 文件名：DuiTextInputPointer.hpp
 * 开发者：青蓝
 * 开发时间：2026-08-03
 * 用途：在自绘输入控件与平台文本输入代理之间转发鼠标拖选。
 */
#pragma once

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiTextInput.hpp"

namespace ysDui::controls::detail {

inline bool HandleTextInputPointer(core::Control& control, ui::DuiTextInput* input,
                                   core::Rect textBounds, const core::Event& event,
                                   bool& selecting, core::DuiPointerCursor outsideCursor)
{
    if (event.type == core::EventType::PointerMove)
    {
        control.SetPointerCursor(selecting || textBounds.Contains(event.position)
            ? core::DuiPointerCursor::IBeam : outsideCursor);
        if (!selecting || input == nullptr)
            return false;
        input->UpdateSelection(event.position);
        return true;
    }
    if (event.type == core::EventType::PointerLeave && !selecting)
    {
        control.SetPointerCursor(outsideCursor);
        return false;
    }
    if (event.type == core::EventType::PointerDown
        && event.button == core::PointerButton::Primary && input != nullptr
        && textBounds.Contains(event.position))
    {
        input->BeginSelection(event.position);
        selecting = true;
        control.SetCaptured(true);
        return true;
    }
    if (event.type == core::EventType::PointerDoubleClick
        && event.button == core::PointerButton::Primary && input != nullptr
        && textBounds.Contains(event.position))
    {
        input->SelectWordAt(event.position);
        return true;
    }
    if ((event.type == core::EventType::PointerUp || event.type == core::EventType::PointerCancel)
        && selecting)
    {
        if (input != nullptr)
            input->EndSelection();
        selecting = false;
        control.SetCaptured(false);
        return true;
    }
    return false;
}

} // namespace ysDui::controls::detail
