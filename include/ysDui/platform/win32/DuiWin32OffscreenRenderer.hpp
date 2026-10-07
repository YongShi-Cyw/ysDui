/**
 * 文件名：DuiWin32OffscreenRenderer.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-27
 * 用途：声明 Win32 后端的离屏 Canvas 渲染能力，用于生成平台无关的像素缓冲。
 */
#pragma once

#include <functional>
#include <optional>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiPixelBuffer.hpp"

namespace ysDui::platform::win32 {

/**
 * Win32 离屏渲染器。
 * 用途：将平台无关的 Canvas 绘制回调渲染为 BGRA 预乘像素，不暴露设备上下文或位图句柄。
 */
class DuiWin32OffscreenRenderer final
{
public:
    /**
     * 离屏渲染指定尺寸的 Canvas 内容。
     * @param size 输出像素尺寸，宽高必须为正数。
     * @param paint 使用平台无关 Canvas 的绘制回调。
     * @return 成功时返回 BGRA 预乘像素，失败时返回空值。
     */
    [[nodiscard]] static std::optional<render::DuiPixelBuffer> Render(
        core::Size size, std::function<void(render::Canvas&, core::Rect)> paint);
};

} // namespace ysDui::platform::win32
