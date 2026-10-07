/**
 * 文件名：DuiCanvas.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明平台无关的二维绘制协议（矩形、路径、文本、图像）及画布变换栈。
 */
#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiCanvasTransform.hpp"
#include "ysDui/render/DuiGradient.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/render/DuiTextMetrics.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"
#include "ysDui/render/DuiPath.hpp"
#include "ysDui/render/DuiImage.hpp"

namespace ysDui::render {

class Canvas : public DuiTextMeasurer {
public:
    virtual ~Canvas() = default;

    /**
     * 离屏栅格化一块内容为预乘 BGRA 图像（可选能力）。
     * 默认返回空；Win32 后端提供实现。调用方在空结果时应退回不依赖栅格化的绘制路径。
     * @param size 输出图像尺寸（DIP/像素，宽高须为正）
     * @param paint 使用另一块 Canvas 的绘制回调；坐标系原点在图像左上角
     */
    [[nodiscard]] virtual std::shared_ptr<const DuiImage> Rasterize(
        core::Size size, const std::function<void(Canvas&, core::Rect)>& paint) const;

    /**
     * 压入相对当前变换的相似变换（缩放 + 平移）。
     * 之后的几何、描边宽度与绘制字号均按合成后的变换换算；测量文本不受影响。
     */
    void PushTransform(DuiCanvasTransform transform);
    /** 弹出最近一次 PushTransform；栈空时忽略。 */
    void PopTransform();
    /** @return 当前合成变换；栈空时为单位变换。 */
    [[nodiscard]] DuiCanvasTransform CurrentTransform() const;

    virtual void PushClip(core::Rect bounds) = 0;
    virtual void PopClip() = 0;
    virtual void FillRect(core::Rect bounds, core::Color color) = 0;
    virtual void FillRoundedRect(core::Rect bounds, int radius, core::Color color) = 0;
    virtual void FillLinearGradient(core::Rect bounds, int radius,
                                    const DuiLinearGradient& gradient) = 0;
    virtual void FillRadialGradient(core::Rect bounds, int radius,
                                    const DuiRadialGradient& gradient) = 0;
    virtual void StrokeRoundedRect(core::Rect bounds, int radius,
                                   core::Color color, float width) = 0;
    virtual void FillEllipse(core::Rect bounds, core::Color color) = 0;
    virtual void StrokeArc(core::Rect bounds, float start, float sweep,
                           core::Color color, float width) = 0;
    virtual void FillPath(const DuiPath& path, core::Color color) = 0;
    virtual void StrokePath(const DuiPath& path, core::Color color, float width) = 0;
    /**
     * 描画三次贝塞尔曲线（起点、控制点1、控制点2、终点）。
     * 用于需要光滑曲线的场合；无此指令的后端可折线逼近。
     */
    virtual void StrokeCubicBezier(core::Point p0, core::Point p1, core::Point p2, core::Point p3,
                                   core::Color color, float width) = 0;
    virtual void DrawText(std::string_view text, core::Rect bounds,
                          const DuiTextStyle& style, DuiTextAlignment alignment,
                          bool wordWrap) = 0;
    DuiTextMetrics MeasureText(std::string_view text, const DuiTextStyle& style,
                               const DuiTextMeasureOptions& options) override = 0;
    virtual void DrawImage(const DuiImage& image, core::Rect destination) = 0;
    virtual void DrawImage(const DuiImage& image, core::Rect source,
                           core::Rect destination) = 0;
    virtual void DrawImageEllipse(const DuiImage& image, core::Rect bounds) = 0;
    virtual void DrawImageRounded(const DuiImage& image, core::Rect bounds, int radius) = 0;

protected:
    /** 把调用方坐标映射到当前变换后的 DIP。后端绘制前应调用。 */
    [[nodiscard]] core::Point TransformPoint(core::Point point) const;
    [[nodiscard]] core::Rect TransformRect(core::Rect bounds) const;
    [[nodiscard]] int TransformLength(int length) const;
    [[nodiscard]] float TransformStroke(float width) const;
    [[nodiscard]] DuiTextStyle TransformText(const DuiTextStyle& style) const;
    [[nodiscard]] DuiPath TransformPath(const DuiPath& path) const;

private:
    std::vector<DuiCanvasTransform> transformStack_;
};

} // namespace ysDui::render
