/**
 * 文件名：accessibility.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现平台无关无障碍节点的公开规则和树导航。
 */
#include "ysDui/core/DuiAccessibility.hpp"

#include "ysDui/core/DuiControl.hpp"

namespace ysDui::core {
namespace {

bool HasExposedChild(const Control* control)
{
    if (!control)
        return false;
    for (const auto& child : control->Children()) {
        if (IsAccessibilityNodeExposed(child.get()))
            return true;
    }
    return false;
}

Control* FindSibling(Control* control, bool forward)
{
    Control* parent = control ? control->Parent() : nullptr;
    if (!parent)
        return nullptr;
    const auto& children = parent->Children();
    for (std::size_t index = 0; index < children.size(); ++index) {
        if (children[index].get() != control)
            continue;
        if (forward) {
            for (++index; index < children.size(); ++index) {
                if (IsAccessibilityNodeExposed(children[index].get()))
                    return children[index].get();
            }
        } else {
            while (index > 0) {
                --index;
                if (IsAccessibilityNodeExposed(children[index].get()))
                    return children[index].get();
            }
        }
        return nullptr;
    }
    return nullptr;
}

} // namespace

bool IsAccessibilityNodeExposed(const Control* control)
{
    return control && (control->Accessibility().role != DuiAccessibilityRole::None || HasExposedChild(control));
}

DuiAccessibilityRole EffectiveAccessibilityRole(const Control* control)
{
    if (!control)
        return DuiAccessibilityRole::None;
    const DuiAccessibilityRole role = control->Accessibility().role;
    return role != DuiAccessibilityRole::None ? role : HasExposedChild(control) ? DuiAccessibilityRole::Group
                                                                              : DuiAccessibilityRole::None;
}

Control* AccessibilityParent(Control* control)
{
    return NearestAccessibleAncestor(control ? control->Parent() : nullptr);
}

Control* AccessibilityFirstChild(Control* control)
{
    if (!control)
        return nullptr;
    for (const auto& child : control->Children()) {
        if (IsAccessibilityNodeExposed(child.get()))
            return child.get();
    }
    return nullptr;
}

Control* AccessibilityLastChild(Control* control)
{
    if (!control)
        return nullptr;
    const auto& children = control->Children();
    for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator) {
        if (IsAccessibilityNodeExposed(iterator->get()))
            return iterator->get();
    }
    return nullptr;
}

Control* AccessibilityNextSibling(Control* control)
{
    return FindSibling(control, true);
}

Control* AccessibilityPreviousSibling(Control* control)
{
    return FindSibling(control, false);
}

Control* DeepestAccessibleDescendant(Control* control)
{
    if (!control)
        return nullptr;
    if (IsAccessibilityNodeExposed(control))
        return control;
    for (const auto& child : control->Children()) {
        if (Control* exposed = DeepestAccessibleDescendant(child.get()))
            return exposed;
    }
    return nullptr;
}

Control* NearestAccessibleAncestor(Control* control)
{
    for (Control* current = control; current; current = current->Parent()) {
        if (IsAccessibilityNodeExposed(current))
            return current;
    }
    return nullptr;
}

} // namespace ysDui::core
