/**
 * 文件名：DuiHostRef.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：实现跨平台宿主弱引用。
 */
#include "ysDui/ui/DuiHostRef.hpp"

#include <utility>

#include "DuiHostRefAccess.hpp"

namespace ysDui::ui {

class HostRef::Impl
{
public:
    std::weak_ptr<const detail::HostBinding> binding;
};

HostRef::HostRef() : impl_(std::make_unique<Impl>()) {}
HostRef::~HostRef() = default;
HostRef::HostRef(const HostRef& other) : impl_(std::make_unique<Impl>())
{
    if (other.impl_)
        *impl_ = *other.impl_;
}
HostRef& HostRef::operator=(const HostRef& other)
{
    if (this != &other)
    {
        if (!impl_)
            impl_ = std::make_unique<Impl>();
        *impl_ = other.impl_ ? *other.impl_ : Impl{};
    }
    return *this;
}
HostRef::HostRef(HostRef&&) noexcept = default;
HostRef& HostRef::operator=(HostRef&&) noexcept = default;
HostRef::HostRef(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
bool HostRef::Valid() const
{
    const auto binding = impl_ ? impl_->binding.lock() : nullptr;
    return binding && binding->Valid();
}
bool HostRef::Empty() const { return !impl_ || impl_->binding.expired(); }

HostRef detail::HostRefAccess::Create(const std::shared_ptr<const HostBinding>& binding)
{
    auto impl = std::make_unique<HostRef::Impl>();
    impl->binding = binding;
    return HostRef(std::move(impl));
}

std::shared_ptr<const detail::HostBinding> detail::HostRefAccess::Lock(const HostRef& reference)
{
    return reference.impl_ ? reference.impl_->binding.lock() : nullptr;
}

} // namespace ysDui::ui
