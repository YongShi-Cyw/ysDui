/**
 * 文件名：DuiHostRef.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明不暴露原生句柄的跨平台宿主弱引用。
 */
#pragma once

#include <memory>

namespace ysDui::ui {

namespace detail {
class HostRefAccess;
}

/**
 * 跨平台宿主弱引用。
 * 引用不延长宿主生命周期；宿主关闭或销毁后 Valid() 返回 false。
 */
class HostRef final
{
public:
    HostRef();
    ~HostRef();
    HostRef(const HostRef& other);
    HostRef& operator=(const HostRef& other);
    HostRef(HostRef&&) noexcept;
    HostRef& operator=(HostRef&&) noexcept;

    /** @return 引用是否仍指向一个有效宿主。 */
    [[nodiscard]] bool Valid() const;
    /** @return 引用是否为空。 */
    [[nodiscard]] bool Empty() const;

private:
    class Impl;
    explicit HostRef(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;

    friend class detail::HostRefAccess;
};

} // namespace ysDui::ui
