/**
 * 文件名：DuiOverlayControl.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明不参与命中测试的宿主内视觉覆盖层基类。
 */
#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"

namespace ysDui::core {

/**
 * 宿主内视觉覆盖层基类。
 * 用途：作为根控件树末尾的视觉元素绘制在其他控件之上，并让输入事件继续传递给下层控件。
 */
class DuiOverlayControl : public Control
{
public:
    /** 构造透明度为零的覆盖层。 */
    DuiOverlayControl();
    /** 销毁覆盖层私有状态。 */
    ~DuiOverlayControl() override;
    DuiOverlayControl(const DuiOverlayControl&) = delete;
    DuiOverlayControl& operator=(const DuiOverlayControl&) = delete;
    DuiOverlayControl(DuiOverlayControl&&) noexcept;
    DuiOverlayControl& operator=(DuiOverlayControl&&) noexcept;

    /**
     * 执行覆盖层命中测试。
     * @param point 待测试的逻辑坐标。
     * @return 始终返回空指针，使事件继续交给下层同级控件。
     */
    [[nodiscard]] Control* HitTest(Point point) override;

protected:
    /**
     * 设置覆盖层透明度。
     * @param alpha 范围为 0 到 1 的透明度；范围外数值会被钳制。
     */
    void SetOverlayAlpha(double alpha);

    /**
     * 获取覆盖层透明度。
     * @return 范围为 0 到 1 的当前透明度。
     */
    [[nodiscard]] double OverlayAlpha() const;

private:
    class Impl;
    std::unique_ptr<Impl> overlay_;
};

} // namespace ysDui::core
