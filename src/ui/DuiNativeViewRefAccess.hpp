/**
 * 文件名：DuiNativeViewRefAccess.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：为平台互操作层提供原生视图引用的私有绑定入口。
 */
#pragma once

#include <memory>

#include "ysDui/ui/DuiNativeViewRef.hpp"

namespace ysDui::ui::detail {

class NativeViewBinding
{
public:
    virtual ~NativeViewBinding() = default;
    [[nodiscard]] virtual bool Valid() const = 0;
};

class NativeViewRefAccess final
{
public:
    [[nodiscard]] static NativeViewRef Create(std::shared_ptr<const NativeViewBinding> binding);
    [[nodiscard]] static std::shared_ptr<const NativeViewBinding> Binding(const NativeViewRef& reference);
};

} // namespace ysDui::ui::detail
