/**
 * 文件名：DuiHostRefAccess.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：为平台后端提供宿主弱引用的私有绑定入口。
 */
#pragma once

#include <memory>

#include "ysDui/ui/DuiHostRef.hpp"

namespace ysDui::ui::detail {

class HostBinding
{
public:
    virtual ~HostBinding() = default;
    [[nodiscard]] virtual bool Valid() const = 0;
};

class HostRefAccess final
{
public:
    [[nodiscard]] static HostRef Create(const std::shared_ptr<const HostBinding>& binding);
    [[nodiscard]] static std::shared_ptr<const HostBinding> Lock(const HostRef& reference);
};

} // namespace ysDui::ui::detail
