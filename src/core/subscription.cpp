/**
 * 文件名：subscription.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-30
 * 用途：实现移动式 RAII 订阅句柄的退订与所有权转移。
 */
#include "ysDui/core/DuiSubscription.hpp"

#include <utility>

namespace ysDui::core {

class DuiSubscription::Impl final
{
public:
    explicit Impl(std::function<void()> callback) : unsubscribe(std::move(callback)) {}

    std::function<void()> unsubscribe;
};

DuiSubscription::DuiSubscription() noexcept = default;
DuiSubscription::DuiSubscription(std::function<void()> unsubscribe)
    : subscription_(std::make_unique<Impl>(std::move(unsubscribe)))
{
}

DuiSubscription::~DuiSubscription()
{
    Reset();
}

DuiSubscription::DuiSubscription(DuiSubscription&&) noexcept = default;

DuiSubscription& DuiSubscription::operator=(DuiSubscription&& other) noexcept
{
    if (this == &other)
        return *this;
    Reset();
    subscription_ = std::move(other.subscription_);
    return *this;
}

void DuiSubscription::Reset() noexcept
{
    if (!subscription_)
        return;
    auto unsubscribe = std::move(subscription_->unsubscribe);
    subscription_.reset();
    unsubscribe();
}

DuiSubscription::operator bool() const noexcept
{
    return subscription_ != nullptr;
}

DuiSubscription DuiSubscription::FromUnsubscribe(std::function<void()> unsubscribe)
{
    return unsubscribe ? DuiSubscription(std::move(unsubscribe)) : DuiSubscription{};
}

} // namespace ysDui::core
