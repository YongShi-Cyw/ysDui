/**
 * 文件名：DuiWin32Canvas.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：实现 Canvas 最小绘制协议到 Win32 GDI/GDI+ 的映射。
 */
#include "DuiWin32Canvas.hpp"

#include "DuiImageAccess.hpp"
#include "DuiWin32GraphicsContext.hpp"
#include "DuiWin32TextRenderer.hpp"
#include "ysDui/platform/win32/DuiWin32OffscreenRenderer.hpp"

#include <windows.h>
#include <gdiplus.h>

#ifdef DrawText
#undef DrawText
#endif

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

namespace ysDui::platform::win32 {
namespace {

class Win32Canvas final : public render::Canvas
{
public:
    Win32Canvas(::HDC context, int dpi)
        : context_(context), scale_(dpi), textRenderer_(reinterpret_cast<std::uintptr_t>(context), dpi)
    {
        (void)EnsureGdiPlus();
    }

    void FillRoundedRect(core::Rect bounds, int radius, core::Color color) override
    {
        bounds = TransformRect(bounds);
        radius = TransformLength(radius);
        if (bounds.Empty()) return;
        bounds = scale_.Scale(bounds);
        radius = scale_.Scale(radius);
        Gdiplus::Graphics graphics(context_);
        ConfigureVectorGraphics(graphics);
        Gdiplus::SolidBrush brush(ToNative(color));
        Gdiplus::GraphicsPath path;
        const int diameter = (std::min)((std::max)(0, radius),
            (std::min)(bounds.Width(), bounds.Height()) / 2) * 2;
        if (diameter == 0)
        {
            graphics.FillRectangle(&brush, bounds.left, bounds.top, bounds.Width(), bounds.Height());
            return;
        }
        AddRoundedRect(path, bounds, diameter);
        graphics.FillPath(&brush, &path);
    }

    void FillLinearGradient(core::Rect bounds, int radius,
                            const render::DuiLinearGradient& gradient) override
    {
        bounds = TransformRect(bounds);
        radius = TransformLength(radius);
        if (bounds.Empty()) return;
        bounds = scale_.Scale(bounds);
        radius = scale_.Scale(radius);
        Gdiplus::Graphics graphics(context_);
        ConfigureVectorGraphics(graphics);
        const Gdiplus::PointF start(static_cast<Gdiplus::REAL>(bounds.left),
                                    static_cast<Gdiplus::REAL>(bounds.top));
        const Gdiplus::PointF end = gradient.vertical
            ? Gdiplus::PointF(static_cast<Gdiplus::REAL>(bounds.left),
                              static_cast<Gdiplus::REAL>(bounds.bottom))
            : Gdiplus::PointF(static_cast<Gdiplus::REAL>(bounds.right),
                              static_cast<Gdiplus::REAL>(bounds.top));
        Gdiplus::LinearGradientBrush brush(start, end, ToNative(gradient.start),
                                           ToNative(gradient.end));
        const int diameter = (std::min)((std::max)(0, radius),
            (std::min)(bounds.Width(), bounds.Height()) / 2) * 2;
        if (diameter == 0)
        {
            graphics.FillRectangle(&brush, bounds.left, bounds.top, bounds.Width(), bounds.Height());
            return;
        }
        Gdiplus::GraphicsPath path;
        AddRoundedRect(path, bounds, diameter);
        graphics.FillPath(&brush, &path);
    }

    void FillRadialGradient(core::Rect bounds, int radius,
                            const render::DuiRadialGradient& gradient) override
    {
        bounds = TransformRect(bounds);
        radius = TransformLength(radius);
        if (bounds.Empty()) return;
        bounds = scale_.Scale(bounds);
        radius = scale_.Scale(radius);
        Gdiplus::Graphics graphics(context_);
        ConfigureVectorGraphics(graphics);
        const int diameter = (std::min)((std::max)(0, radius),
            (std::min)(bounds.Width(), bounds.Height()) / 2) * 2;
        Gdiplus::GraphicsPath path;
        if (diameter == 0)
            path.AddRectangle(Gdiplus::Rect(bounds.left, bounds.top, bounds.Width(), bounds.Height()));
        else
            AddRoundedRect(path, bounds, diameter);
        Gdiplus::PathGradientBrush brush(&path);
        brush.SetCenterColor(ToNative(gradient.center));
        Gdiplus::Color edge = ToNative(gradient.edge);
        int colorCount = 1;
        brush.SetSurroundColors(&edge, &colorCount);
        graphics.FillPath(&brush, &path);
    }

    void StrokeRoundedRect(core::Rect bounds, int radius, core::Color color,
                           float width) override
    {
        bounds = TransformRect(bounds);
        radius = TransformLength(radius);
        width = TransformStroke(width);
        if (width <= 0.0F || bounds.Empty()) return;
        bounds = scale_.Scale(bounds);
        radius = scale_.Scale(radius);
        width = ScaleWidth(width);
        Gdiplus::Graphics graphics(context_);
        ConfigureVectorGraphics(graphics);
        Gdiplus::Pen pen(ToNative(color), width);
        const int clampedRadius = (std::min)((std::max)(0, radius),
            (std::min)(bounds.Width(), bounds.Height()) / 2);
        const float inset = width == 1.0F ? 0.5F : 0.0F;
        const float left = static_cast<float>(bounds.left) + inset;
        const float top = static_cast<float>(bounds.top) + inset;
        const float rectWidth = (std::max)(0.0F,
            static_cast<float>(bounds.Width()) - inset * 2.0F);
        const float rectHeight = (std::max)(0.0F,
            static_cast<float>(bounds.Height()) - inset * 2.0F);
        const float diameter = static_cast<float>(clampedRadius * 2);
        if (diameter == 0.0F)
        {
            // 半径 0 的直角边框改用整数像素四边实心填充，不再走矢量描边。
            // 原因：本后端同时启用了 PixelOffsetModeHalf 与 0.5 内缩，1px 描边会被摊到
            // 相邻两行/列上，落在窗口边缘的那半像素又被裁掉，表现即边框顶边/四角缺像素。
            // 逐边填充坐标全为整数、四角共用端点，结果稳定且不依赖光栅化细节。
            const int thickness = (std::max)(1, static_cast<int>(std::lround(width)));
            Gdiplus::SolidBrush brush(ToNative(color));
            const int x0 = bounds.left;
            const int y0 = bounds.top;
            const int x1 = bounds.right;
            const int y1 = bounds.bottom;
            const int innerHeight = (std::max)(0, y1 - y0 - thickness * 2);
            graphics.FillRectangle(&brush, x0, y0, x1 - x0, thickness); // 上
            graphics.FillRectangle(&brush, x0, y1 - thickness, x1 - x0, thickness); // 下
            graphics.FillRectangle(&brush, x0, y0 + thickness, thickness, innerHeight); // 左
            graphics.FillRectangle(&brush, x1 - thickness, y0 + thickness, thickness,
                                   innerHeight); // 右
            return;
        }
        Gdiplus::GraphicsPath path;
        path.AddArc(left, top, diameter, diameter, 180, 90);
        path.AddArc(left + rectWidth - diameter, top, diameter, diameter, 270, 90);
        path.AddArc(left + rectWidth - diameter, top + rectHeight - diameter,
                    diameter, diameter, 0, 90);
        path.AddArc(left, top + rectHeight - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
        graphics.DrawPath(&pen, &path);
    }

    void FillEllipse(core::Rect bounds, core::Color color) override
    {
        bounds = TransformRect(bounds);
        bounds = scale_.Scale(bounds);
        Gdiplus::Graphics graphics(context_);
        ConfigureVectorGraphics(graphics);
        Gdiplus::SolidBrush brush(ToNative(color));
        graphics.FillEllipse(&brush, bounds.left, bounds.top, bounds.Width(), bounds.Height());
    }

    void StrokeArc(core::Rect bounds, float start, float sweep, core::Color color,
                   float width) override
    {
        bounds = TransformRect(bounds);
        width = TransformStroke(width);
        bounds = scale_.Scale(bounds);
        Gdiplus::Graphics graphics(context_);
        ConfigureVectorGraphics(graphics);
        Gdiplus::Pen pen(ToNative(color), ScaleWidth(width));
        graphics.DrawArc(&pen, bounds.left, bounds.top, bounds.Width(), bounds.Height(), start, sweep);
    }

    void FillPath(const render::DuiPath& path, core::Color color) override
    {
        const render::DuiPath mapped = TransformPath(path);
        Gdiplus::Graphics graphics(context_);
        ConfigureVectorGraphics(graphics);
        Gdiplus::SolidBrush brush(ToNative(color));
        Gdiplus::GraphicsPath native;
        BuildPath(mapped, native);
        graphics.FillPath(&brush, &native);
    }

    void StrokePath(const render::DuiPath& path, core::Color color, float width) override
    {
        width = TransformStroke(width);
        if (width <= 0.0F) return;
        const render::DuiPath mapped = TransformPath(path);
        Gdiplus::Graphics graphics(context_);
        ConfigureVectorGraphics(graphics);
        Gdiplus::Pen pen(ToNative(color), ScaleWidth(width));
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        Gdiplus::GraphicsPath native;
        BuildPath(mapped, native);
        graphics.DrawPath(&pen, &native);
    }

    void StrokeCubicBezier(core::Point p0, core::Point p1, core::Point p2, core::Point p3,
                           core::Color color, float width) override
    {
        width = TransformStroke(width);
        if (width <= 0.0F) return;
        const auto map = [this](core::Point point)
        {
            point = TransformPoint(point);
            return scale_.Scale(point);
        };
        p0 = map(p0);
        p1 = map(p1);
        p2 = map(p2);
        p3 = map(p3);
        Gdiplus::Graphics graphics(context_);
        ConfigureVectorGraphics(graphics);
        Gdiplus::Pen pen(ToNative(color), ScaleWidth(width));
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        Gdiplus::GraphicsPath native;
        native.AddBezier(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
        graphics.DrawPath(&pen, &native);
    }

    void PushClip(core::Rect bounds) override
    {
        bounds = TransformRect(bounds);
        bounds = scale_.Scale(bounds);
        clipStates_.push_back(::SaveDC(context_));
        ::IntersectClipRect(context_, bounds.left, bounds.top, bounds.right, bounds.bottom);
    }

    void PopClip() override
    {
        if (clipStates_.empty()) return;
        ::RestoreDC(context_, clipStates_.back());
        clipStates_.pop_back();
    }

    void FillRect(core::Rect bounds, core::Color color) override
    {
        bounds = TransformRect(bounds);
        bounds = scale_.Scale(bounds);
        // GDI FillRect 忽略 alpha；半透明填充（如框选）必须走 GDI+。
        if (color.alpha < 255)
        {
            Gdiplus::Graphics graphics(context_);
            ConfigureVectorGraphics(graphics);
            Gdiplus::SolidBrush brush(ToNative(color));
            graphics.FillRectangle(&brush, bounds.left, bounds.top, bounds.Width(), bounds.Height());
            return;
        }
        const ::RECT native{bounds.left, bounds.top, bounds.right, bounds.bottom};
        const ::COLORREF value = RGB(color.red, color.green, color.blue);
        ::HBRUSH brush = ::CreateSolidBrush(value);
        if (brush != nullptr)
        {
            ::FillRect(context_, &native, brush);
            ::DeleteObject(brush);
        }
    }

    void DrawText(std::string_view text, core::Rect bounds,
                  const render::DuiTextStyle& style,
                  render::DuiTextAlignment alignment, bool wordWrap) override
    {
        textRenderer_.DrawText(text, TransformRect(bounds), TransformText(style), alignment, wordWrap);
    }

    render::DuiTextMetrics MeasureText(
        std::string_view text, const render::DuiTextStyle& style,
        const render::DuiTextMeasureOptions& options) override
    {
        return textRenderer_.MeasureText(text, style, options);
    }

    void DrawImage(const render::DuiImage& image, core::Rect bounds) override
    {
        DrawImagePixels(image, scale_.Scale(TransformRect(bounds)), nullptr);
    }

    void DrawImage(const render::DuiImage& image, core::Rect source,
                   core::Rect destination) override
    {
        if (destination.Empty() || source.Empty() || image.Empty()
            || image.Format() != render::DuiImageFormat::Bgra8Premultiplied)
        {
            return;
        }
        const core::Size size = image.Size();
        Gdiplus::Bitmap bitmap(size.width, size.height, size.width * 4,
            PixelFormat32bppPARGB,
            const_cast<BYTE*>(reinterpret_cast<const BYTE*>(
                render::DuiImageAccess::Pixels(image).data())));
        Gdiplus::Graphics graphics(context_);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
        destination = scale_.Scale(TransformRect(destination));
        const Gdiplus::Rect target(destination.left, destination.top,
                                   destination.Width(), destination.Height());
        graphics.DrawImage(&bitmap, target, source.left, source.top, source.Width(),
                           source.Height(), Gdiplus::UnitPixel, nullptr);
    }

    void DrawImageEllipse(const render::DuiImage& image, core::Rect bounds) override
    {
        bounds = scale_.Scale(TransformRect(bounds));
        Gdiplus::GraphicsPath clip;
        clip.AddEllipse(bounds.left, bounds.top, bounds.Width(), bounds.Height());
        DrawImagePixels(image, bounds, &clip);
    }

    void DrawImageRounded(const render::DuiImage& image, core::Rect bounds,
                          int radius) override
    {
        bounds = scale_.Scale(TransformRect(bounds));
        radius = scale_.Scale(TransformLength(radius));
        const int diameter = (std::min)((std::max)(0, radius),
            (std::min)(bounds.Width(), bounds.Height()) / 2) * 2;
        if (diameter == 0)
        {
            DrawImagePixels(image, bounds, nullptr);
            return;
        }
        Gdiplus::GraphicsPath clip;
        AddRoundedRect(clip, bounds, diameter);
        DrawImagePixels(image, bounds, &clip);
    }

    [[nodiscard]] std::shared_ptr<const render::DuiImage> Rasterize(
        core::Size size, const std::function<void(Canvas&, core::Rect)>& paint) const override
    {
        const auto buffer = DuiWin32OffscreenRenderer::Render(size, paint);
        if (!buffer.has_value() || buffer->Empty())
            return {};
        std::vector<unsigned char> pixels(buffer->Bytes().begin(), buffer->Bytes().end());
        return render::DuiImage::CreateBgra8Premultiplied(buffer->Size(), std::move(pixels));
    }

private:
    [[nodiscard]] static Gdiplus::Color ToNative(core::Color color)
    {
        return {color.alpha, color.red, color.green, color.blue};
    }

    static void AddRoundedRect(Gdiplus::GraphicsPath& path, core::Rect bounds, int diameter)
    {
        path.AddArc(bounds.left, bounds.top, diameter, diameter, 180, 90);
        path.AddArc(bounds.right - diameter, bounds.top, diameter, diameter, 270, 90);
        path.AddArc(bounds.right - diameter, bounds.bottom - diameter, diameter, diameter, 0, 90);
        path.AddArc(bounds.left, bounds.bottom - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
    }

    [[nodiscard]] float ScaleWidth(float width) const
    {
        return static_cast<float>(width * scale_.Factor());
    }

    static void ConfigureVectorGraphics(Gdiplus::Graphics& graphics)
    {
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    }

    void DrawImagePixels(const render::DuiImage& image, core::Rect bounds,
                         const Gdiplus::GraphicsPath* clip)
    {
        if (bounds.Empty() || image.Empty()
            || image.Format() != render::DuiImageFormat::Bgra8Premultiplied)
        {
            return;
        }
        const core::Size size = image.Size();
        Gdiplus::Bitmap bitmap(size.width, size.height, size.width * 4,
            PixelFormat32bppPARGB,
            const_cast<BYTE*>(reinterpret_cast<const BYTE*>(
                render::DuiImageAccess::Pixels(image).data())));
        Gdiplus::Graphics graphics(context_);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
        if (clip != nullptr) graphics.SetClip(clip);
        graphics.DrawImage(&bitmap, bounds.left, bounds.top, bounds.Width(), bounds.Height());
    }

    void BuildPath(const render::DuiPath& path, Gdiplus::GraphicsPath& result) const
    {
        core::Point current{};
        bool hasCurrent{};
        for (const auto& command : path.Commands())
        {
            switch (command.type)
            {
            case render::DuiPathCommandType::MoveTo:
                result.StartFigure();
                current = scale_.Scale(command.point);
                hasCurrent = true;
                break;
            case render::DuiPathCommandType::LineTo:
                if (hasCurrent)
                {
                    const core::Point next = scale_.Scale(command.point);
                    result.AddLine(current.x, current.y, next.x, next.y);
                    current = next;
                }
                break;
            case render::DuiPathCommandType::Close:
                if (hasCurrent)
                {
                    result.CloseFigure();
                    hasCurrent = false;
                }
                break;
            }
        }
    }

    ::HDC context_{};
    core::DuiDpiScale scale_;
    DuiWin32TextRenderer textRenderer_;
    std::vector<int> clipStates_;
};

} // namespace

std::unique_ptr<render::Canvas> CreateWin32Canvas(std::uintptr_t deviceContext, int dpi)
{
    return std::make_unique<Win32Canvas>(reinterpret_cast<::HDC>(deviceContext), dpi);
}

} // namespace ysDui::platform::win32
