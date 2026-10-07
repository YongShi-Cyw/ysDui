/**
 * 文件名：DuiNativeViewRef.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明不暴露平台句柄的原生视图引用。
 */
#pragma once

#include <memory>

namespace ysDui::ui {

namespace detail {
class NativeViewRefAccess;
}

/** 原生视图的跨平台引用；具体平台互操作层负责创建。 */
class NativeViewRef final
{
public:
    NativeViewRef();
    ~NativeViewRef();
    NativeViewRef(const NativeViewRef& other);
    NativeViewRef& operator=(const NativeViewRef& other);
    NativeViewRef(NativeViewRef&&) noexcept;
    NativeViewRef& operator=(NativeViewRef&&) noexcept;

    /** @return 引用是否仍指向有效的原生视图。 */
    [[nodiscard]] bool Valid() const;
    /** @return 引用是否为空。 */
    [[nodiscard]] bool Empty() const;

private:
    class Impl;
    explicit NativeViewRef(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;

    friend class detail::NativeViewRefAccess;
};

} // namespace ysDui::ui
