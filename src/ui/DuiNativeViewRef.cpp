/**
 * 文件名：DuiNativeViewRef.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：实现原生视图跨平台引用。
 */
#include "ysDui/ui/DuiNativeViewRef.hpp"

#include <utility>

#include "DuiNativeViewRefAccess.hpp"

namespace ysDui::ui {

class NativeViewRef::Impl
{
public:
    std::shared_ptr<const detail::NativeViewBinding> binding;
};

NativeViewRef::NativeViewRef() : impl_(std::make_unique<Impl>()) {}
NativeViewRef::~NativeViewRef() = default;
NativeViewRef::NativeViewRef(const NativeViewRef& other) : impl_(std::make_unique<Impl>())
{
    if (other.impl_)
        *impl_ = *other.impl_;
}
NativeViewRef& NativeViewRef::operator=(const NativeViewRef& other)
{
    if (this != &other)
    {
        if (!impl_)
            impl_ = std::make_unique<Impl>();
        *impl_ = other.impl_ ? *other.impl_ : Impl{};
    }
    return *this;
}
NativeViewRef::NativeViewRef(NativeViewRef&&) noexcept = default;
NativeViewRef& NativeViewRef::operator=(NativeViewRef&&) noexcept = default;
NativeViewRef::NativeViewRef(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
bool NativeViewRef::Valid() const { return impl_ && impl_->binding && impl_->binding->Valid(); }
bool NativeViewRef::Empty() const { return !impl_ || !impl_->binding; }

NativeViewRef detail::NativeViewRefAccess::Create(std::shared_ptr<const NativeViewBinding> binding)
{
    auto impl = std::make_unique<NativeViewRef::Impl>();
    impl->binding = std::move(binding);
    return NativeViewRef(std::move(impl));
}

std::shared_ptr<const detail::NativeViewBinding> detail::NativeViewRefAccess::Binding(
    const NativeViewRef& reference)
{
    return reference.impl_ ? reference.impl_->binding : nullptr;
}

} // namespace ysDui::ui
