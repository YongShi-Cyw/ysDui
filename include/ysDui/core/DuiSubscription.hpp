/**
 * 文件名：DuiSubscription.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-30
 * 用途：声明发布者销毁后仍可安全释放的移动式 RAII 订阅句柄。
 */
#pragma once

#include <functional>
#include <memory>

namespace ysDui::controls::list {
class DuiMenu;
}

namespace ysDui::core {

class DuiSkinSession;
class DuiTheme;

/**
 * 自动管理一次事件订阅的唯一所有权句柄。
 * 句柄离开作用域或调用 Reset 时自动退订；发布者先销毁时释放句柄仍然安全。
 */
class DuiSubscription final
{
public:
    /** 创建不持有订阅的空句柄。 */
    DuiSubscription() noexcept;
    ~DuiSubscription();

    DuiSubscription(const DuiSubscription&) = delete;
    DuiSubscription& operator=(const DuiSubscription&) = delete;
    DuiSubscription(DuiSubscription&&) noexcept;
    DuiSubscription& operator=(DuiSubscription&& other) noexcept;

    /** 取消当前订阅；重复调用没有效果。 */
    void Reset() noexcept;

    /** @return 当前句柄是否持有尚未重置的订阅。 */
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] static DuiSubscription FromUnsubscribe(std::function<void()> unsubscribe);

private:
    friend class DuiSkinSession;
    friend class DuiTheme;
    friend class ysDui::controls::list::DuiMenu;

    explicit DuiSubscription(std::function<void()> unsubscribe);

    class Impl;
    std::unique_ptr<Impl> subscription_;
};

} // namespace ysDui::core
