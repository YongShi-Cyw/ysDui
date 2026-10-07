/**
 * 文件名：DuiMarkdownPaint.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：绘制已布局的 Markdown 片段（圆角表面、选区、图片与文本）。
 */
#include "DuiMarkdownAst.hpp"

#include <algorithm>

namespace ysDui::controls::content::detail {
namespace {

[[nodiscard]] core::Rect OffsetRect(core::Rect bounds, core::Point origin)
{
    return {bounds.left + origin.x, bounds.top + origin.y, bounds.right + origin.x,
            bounds.bottom + origin.y};
}

[[nodiscard]] std::vector<std::size_t> Utf8Boundaries(std::string_view text)
{
    std::vector<std::size_t> boundaries{0};
    for (std::size_t index = 0; index < text.size();)
    {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        const std::size_t units = first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
        index += (std::min)(units, text.size() - index);
        boundaries.push_back(index);
    }
    return boundaries;
}

[[nodiscard]] bool BandIntersectsDirty(const LaidBand& band, core::Rect layoutDirty)
{
    if (band.y1 <= layoutDirty.top || band.y0 >= layoutDirty.bottom)
        return false;
    return true;
}

void PaintDecorRange(render::Canvas& canvas, const MarkdownLayout& layout, core::Rect dirty,
                     core::Point origin, std::size_t begin, std::size_t end)
{
    end = (std::min)(end, layout.decors.size());
    for (std::size_t index = begin; index < end; ++index)
    {
        const LaidDecor& decor = layout.decors[index];
        const core::Rect screen = OffsetRect(decor.bounds, origin);
        const core::Rect clipped = core::Rect::Intersect(screen, dirty);
        if (clipped.Empty())
            continue;

        if (!decor.strokeOnly && decor.fill.alpha != 0)
        {
            if (decor.radius > 0)
                canvas.FillRoundedRect(screen, decor.radius, decor.fill);
            else
                canvas.FillRect(screen, decor.fill);
        }
        if ((!decor.fillOnly || decor.strokeOnly) && decor.strokeWidth > 0.0f
            && decor.stroke.alpha != 0)
        {
            canvas.StrokeRoundedRect(screen, decor.radius, decor.stroke, decor.strokeWidth);
        }
    }
}

void PaintImageRange(render::Canvas& canvas, const MarkdownLayout& layout, core::Rect dirty,
                     core::Point origin, std::size_t begin, std::size_t end)
{
    end = (std::min)(end, layout.images.size());
    for (std::size_t index = begin; index < end; ++index)
    {
        const LaidImage& image = layout.images[index];
        const core::Rect screen = OffsetRect(image.bounds, origin);
        const core::Rect clipped = core::Rect::Intersect(screen, dirty);
        if (clipped.Empty() || image.image == nullptr)
            continue;
        canvas.DrawImageRounded(*image.image, screen, 6);
    }
}

void PaintSelectionRange(render::Canvas& canvas, const MarkdownLayout& layout, core::Rect dirty,
                         const MarkdownPaintOptions& options, std::size_t begin, std::size_t end)
{
    const std::size_t selMin = (std::min)(options.selectionBegin, options.selectionEnd);
    const std::size_t selMax = (std::max)(options.selectionBegin, options.selectionEnd);
    if (selMin >= selMax)
        return;

    end = (std::min)(end, layout.runs.size());
    for (std::size_t index = begin; index < end; ++index)
    {
        const LaidRun& run = layout.runs[index];
        if (!run.selectable || run.plainEnd <= selMin || run.plainBegin >= selMax)
            continue;
        const core::Rect screen = OffsetRect(run.bounds, options.origin);
        if (core::Rect::Intersect(screen, dirty).Empty())
            continue;
        const std::size_t localBegin = (std::max)(selMin, run.plainBegin) - run.plainBegin;
        const std::size_t localEnd = (std::min)(selMax, run.plainEnd) - run.plainBegin;
        if (localBegin >= localEnd || run.text.empty())
            continue;

        const auto boundaries = Utf8Boundaries(run.text);
        auto advanceAt = [&](std::size_t byteOffset) -> int
        {
            if (byteOffset == 0)
                return 0;
            if (byteOffset >= run.text.size())
                return screen.Width();
            const auto metrics =
                canvas.MeasureText(run.text.substr(0, byteOffset), run.style, {});
            return metrics.size.width;
        };
        const int x0 = screen.left + advanceAt(localBegin);
        const int x1 = screen.left + advanceAt(localEnd);
        const core::Rect highlight{x0, screen.top, (std::max)(x0 + 1, x1), screen.bottom};
        const core::Rect clipped = core::Rect::Intersect(highlight, dirty);
        if (!clipped.Empty())
            canvas.FillRect(clipped, options.selectionColor);
        (void)boundaries;
    }
}

void PaintRunRange(render::Canvas& canvas, const MarkdownLayout& layout, core::Rect dirty,
                   const MarkdownPaintOptions& options, std::size_t begin, std::size_t end)
{
    end = (std::min)(end, layout.runs.size());
    for (std::size_t index = begin; index < end; ++index)
    {
        const LaidRun& run = layout.runs[index];
        const core::Rect screen = OffsetRect(run.bounds, options.origin);
        const core::Rect clipped = core::Rect::Intersect(screen, dirty);
        if (clipped.Empty() || run.text.empty())
            continue;

        render::DuiTextStyle style = run.style;
        if (!run.linkHref.empty() && !options.hoveredLink.empty()
            && run.linkHref == options.hoveredLink)
            style.color = layout.linkHoverColor;

        if (style.color.alpha == 0)
            continue;

        canvas.DrawText(run.text, screen, style, run.alignment, false);

        if (run.strike)
        {
            const int midY = (screen.top + screen.bottom) / 2;
            canvas.FillRect({screen.left, midY, screen.right, midY + 1}, style.color);
        }
    }
}

} // namespace

void PaintMarkdown(render::Canvas& canvas, const MarkdownLayout& layout, core::Rect dirty,
                   const MarkdownPaintOptions& options)
{
    const core::Point origin = options.origin;
    const core::Rect layoutDirty{dirty.left - origin.x, dirty.top - origin.y, dirty.right - origin.x,
                                 dirty.bottom - origin.y};

    if (layout.bands.empty())
    {
        PaintDecorRange(canvas, layout, dirty, origin, 0, layout.decors.size());
        PaintImageRange(canvas, layout, dirty, origin, 0, layout.images.size());
        PaintSelectionRange(canvas, layout, dirty, options, 0, layout.runs.size());
        PaintRunRange(canvas, layout, dirty, options, 0, layout.runs.size());
        return;
    }

    std::size_t decorCursor = 0;
    std::size_t imageCursor = 0;
    std::size_t runCursor = 0;
    for (const LaidBand& band : layout.bands)
    {
        if (!BandIntersectsDirty(band, layoutDirty))
        {
            decorCursor = (std::max)(decorCursor, band.decorEnd);
            imageCursor = (std::max)(imageCursor, band.imageEnd);
            runCursor = (std::max)(runCursor, band.runEnd);
            continue;
        }
        PaintDecorRange(canvas, layout, dirty, origin, band.decorBegin, band.decorEnd);
        PaintImageRange(canvas, layout, dirty, origin, band.imageBegin, band.imageEnd);
        PaintSelectionRange(canvas, layout, dirty, options, band.runBegin, band.runEnd);
        PaintRunRange(canvas, layout, dirty, options, band.runBegin, band.runEnd);
        decorCursor = (std::max)(decorCursor, band.decorEnd);
        imageCursor = (std::max)(imageCursor, band.imageEnd);
        runCursor = (std::max)(runCursor, band.runEnd);
    }

    // 流式光标等落在最后一块之后的装饰/片段
    PaintDecorRange(canvas, layout, dirty, origin, decorCursor, layout.decors.size());
    PaintImageRange(canvas, layout, dirty, origin, imageCursor, layout.images.size());
    PaintSelectionRange(canvas, layout, dirty, options, runCursor, layout.runs.size());
    PaintRunRange(canvas, layout, dirty, options, runCursor, layout.runs.size());
}

} // namespace ysDui::controls::content::detail
