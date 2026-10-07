/**
 * 文件名：DuiNativeViewHost.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明原生视图的跨平台承载能力。
 */
#pragma once

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/ui/DuiNativeViewRef.hpp"

namespace ysDui::ui {

struct DuiNativeViewOptions final
{
    bool takeOwnership{};
    bool stripDecorations{};
};

class DuiNativeViewHost
{
public:
    virtual ~DuiNativeViewHost() = default;
    /** 将平台互操作层创建的视图引用附加到当前宿主。 */
    virtual bool Attach(NativeViewRef child, DuiNativeViewOptions options) = 0;
    /** 解除附加并返回原生视图引用。 */
    [[nodiscard]] virtual NativeViewRef Detach() = 0;
    virtual void SetBounds(core::Rect bounds) = 0;
    virtual void SetVisible(bool visible) = 0;
    [[nodiscard]] virtual bool HasContent() const = 0;
};

} // namespace ysDui::ui
