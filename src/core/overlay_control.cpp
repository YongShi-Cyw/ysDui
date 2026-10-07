/**
 * 文件名：overlay_control.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现宿主内视觉覆盖层的透明度与点击穿透行为。
 */
#include "ysDui/core/DuiOverlayControl.hpp"

#include <algorithm>
#include <utility>

namespace ysDui::core {

class DuiOverlayControl::Impl final
{
public:
    double alpha{};  // 覆盖层绘制透明度，始终保持在 0 到 1。
};

DuiOverlayControl::DuiOverlayControl() : overlay_(std::make_unique<Impl>())
{
}

DuiOverlayControl::~DuiOverlayControl() = default;
DuiOverlayControl::DuiOverlayControl(DuiOverlayControl&&) noexcept = default;
DuiOverlayControl& DuiOverlayControl::operator=(DuiOverlayControl&&) noexcept = default;

Control* DuiOverlayControl::HitTest(Point)
{
    return nullptr;
}

void DuiOverlayControl::SetOverlayAlpha(double alpha)
{
    overlay_->alpha = (std::clamp)(alpha, 0.0, 1.0);
}

double DuiOverlayControl::OverlayAlpha() const
{
    return overlay_->alpha;
}

} // namespace ysDui::core
