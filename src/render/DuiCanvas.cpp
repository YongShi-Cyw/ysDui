/**
 * 文件名：DuiCanvas.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：实现 Canvas 变换栈及其坐标/字号映射。
 */
#include "ysDui/render/DuiCanvas.hpp"

namespace ysDui::render {

std::shared_ptr<const DuiImage> Canvas::Rasterize(
    core::Size, const std::function<void(Canvas&, core::Rect)>&) const
{
    return {};
}

void Canvas::PushTransform(DuiCanvasTransform transform)
{
    transformStack_.push_back(DuiCanvasTransform::Compose(CurrentTransform(), transform));
}

void Canvas::PopTransform()
{
    if (!transformStack_.empty())
        transformStack_.pop_back();
}

DuiCanvasTransform Canvas::CurrentTransform() const
{
    if (transformStack_.empty())
        return {};
    return transformStack_.back();
}

core::Point Canvas::TransformPoint(core::Point point) const
{
    return CurrentTransform().MapPoint(point);
}

core::Rect Canvas::TransformRect(core::Rect bounds) const
{
    return CurrentTransform().MapRect(bounds);
}

int Canvas::TransformLength(int length) const
{
    return CurrentTransform().MapLength(length);
}

float Canvas::TransformStroke(float width) const
{
    return CurrentTransform().MapStroke(width);
}

DuiTextStyle Canvas::TransformText(const DuiTextStyle& style) const
{
    return CurrentTransform().MapText(style);
}

DuiPath Canvas::TransformPath(const DuiPath& path) const
{
    return CurrentTransform().MapPath(path);
}

} // namespace ysDui::render
