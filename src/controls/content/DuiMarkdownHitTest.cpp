/**
 * 文件名：DuiMarkdownHitTest.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：Markdown 布局的链接、复制按钮与划选命中测试。
 */
#include "DuiMarkdownAst.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>

namespace ysDui::controls::content::detail {
namespace {

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

} // namespace

HitResult HitTestMarkdown(const MarkdownLayout& layout, core::Point point)
{
    HitResult result;

    const auto probeRun = [&](const LaidRun& run) -> bool
    {
        if (!run.bounds.Contains(point))
            return false;
        if (run.isCopyButton)
        {
            result.hit = true;
            result.copyButton = true;
            result.copyCode = run.copyCode;
            result.copyLanguage = run.copyLanguage;
            return true;
        }
        if (!run.linkHref.empty())
        {
            result.hit = true;
            result.linkHref = run.linkHref;
            result.onText = run.selectable;
            return true;
        }
        if (run.selectable)
        {
            result.hit = true;
            result.onText = true;
            return true;
        }
        return false;
    };

    if (!layout.bands.empty())
    {
        // 后绘制的块优先；块内同样自后向前
        for (auto bandIt = layout.bands.rbegin(); bandIt != layout.bands.rend(); ++bandIt)
        {
            if (point.y < bandIt->y0 || point.y >= bandIt->y1)
                continue;
            if (bandIt->runEnd <= bandIt->runBegin)
                continue;
            for (std::size_t index = bandIt->runEnd; index > bandIt->runBegin; --index)
            {
                if (probeRun(layout.runs[index - 1]))
                    return result;
            }
        }
        // 块切片之外的尾部 runs（极少）
        if (!layout.bands.empty())
        {
            const std::size_t tailBegin = layout.bands.back().runEnd;
            for (std::size_t index = layout.runs.size(); index > tailBegin; --index)
            {
                if (probeRun(layout.runs[index - 1]))
                    return result;
            }
        }
        return result;
    }

    for (auto it = layout.runs.rbegin(); it != layout.runs.rend(); ++it)
    {
        if (probeRun(*it))
            return result;
    }
    return result;
}

std::size_t PlainIndexAt(const MarkdownLayout& layout, core::Point point,
                         render::DuiTextMeasurer* measurer)
{
    if (layout.plainText.empty())
        return 0;

    const LaidRun* best = nullptr;
    int bestDist = (std::numeric_limits<int>::max)();
    bool foundExact = false;

    const auto considerRun = [&](const LaidRun& run)
    {
        if (foundExact || !run.selectable || run.plainBegin >= run.plainEnd)
            return;
        if (run.bounds.Contains(point))
        {
            best = &run;
            bestDist = 0;
            foundExact = true;
            return;
        }
        const int cx = (std::clamp)(point.x, run.bounds.left, run.bounds.right);
        const int cy = (std::clamp)(point.y, run.bounds.top, run.bounds.bottom);
        const int dx = cx - point.x;
        const int dy = cy - point.y;
        const int dist = dx * dx + dy * dy;
        if (dist < bestDist)
        {
            bestDist = dist;
            best = &run;
        }
    };

    const auto scanRunRange = [&](std::size_t begin, std::size_t end)
    {
        end = (std::min)(end, layout.runs.size());
        for (std::size_t index = begin; index < end; ++index)
            considerRun(layout.runs[index]);
    };

    if (!layout.bands.empty())
    {
        // 优先只扫 Y 命中的顶层块；未命中再落到最近块（页边空白点击）
        for (const LaidBand& band : layout.bands)
        {
            if (point.y < band.y0 || point.y >= band.y1)
                continue;
            scanRunRange(band.runBegin, band.runEnd);
            if (foundExact)
                break;
        }
        if (!foundExact && best == nullptr)
        {
            const LaidBand* nearest = nullptr;
            int nearestYDist = (std::numeric_limits<int>::max)();
            for (const LaidBand& band : layout.bands)
            {
                int dist = 0;
                if (point.y < band.y0)
                    dist = band.y0 - point.y;
                else if (point.y >= band.y1)
                    dist = point.y - band.y1 + 1;
                if (dist < nearestYDist)
                {
                    nearestYDist = dist;
                    nearest = &band;
                }
            }
            if (nearest != nullptr)
                scanRunRange(nearest->runBegin, nearest->runEnd);
            if (!layout.bands.empty())
                scanRunRange(layout.bands.back().runEnd, layout.runs.size());
        }
    }
    else
    {
        scanRunRange(0, layout.runs.size());
    }

    if (best == nullptr)
        return layout.plainText.size();

    if (point.y < best->bounds.top)
        return best->plainBegin;
    if (point.y > best->bounds.bottom && bestDist != 0)
    {
        // 偏下方：落到该 run 末尾（多行时由距离选取最近 run）
    }

    const int localX = point.x - best->bounds.left;
    if (localX <= 0)
        return best->plainBegin;
    if (localX >= best->bounds.Width())
        return best->plainEnd;

    const auto boundaries = Utf8Boundaries(best->text);
    if (boundaries.size() <= 1)
        return best->plainBegin;

    std::size_t chosen = boundaries.back();
    for (std::size_t i = 0; i + 1 < boundaries.size(); ++i)
    {
        const std::size_t byteEnd = boundaries[i + 1];
        int width = 0;
        if (measurer != nullptr)
        {
            width = measurer->MeasureText(best->text.substr(0, byteEnd), best->style, {}).size.width;
        }
        else
        {
            width = best->bounds.Width() * static_cast<int>(byteEnd)
                / static_cast<int>((std::max)(std::size_t{1}, best->text.size()));
        }
        if (localX < width)
        {
            chosen = boundaries[i];
            // 若更靠近下一边界中点则取下一
            int nextWidth = width;
            if (i + 2 < boundaries.size())
            {
                if (measurer != nullptr)
                    nextWidth = measurer
                                    ->MeasureText(best->text.substr(0, boundaries[i + 2]), best->style,
                                                  {})
                                    .size.width;
                else
                    nextWidth = best->bounds.Width() * static_cast<int>(boundaries[i + 2])
                        / static_cast<int>((std::max)(std::size_t{1}, best->text.size()));
            }
            if (localX > (width + nextWidth) / 2)
                chosen = boundaries[i + 1];
            break;
        }
        chosen = byteEnd;
    }
    return best->plainBegin + (std::min)(chosen, best->text.size());
}

std::pair<std::size_t, std::size_t> WordRangeAt(const MarkdownLayout& layout, std::size_t plainIndex)
{
    const std::string& text = layout.plainText;
    if (text.empty())
        return {0, 0};

    plainIndex = (std::min)(plainIndex, text.size());
    const auto isWordByte = [](unsigned char ch) -> bool
    {
        // ASCII 词字符，或 UTF-8 多字节（含 CJK）视为词的一部分
        return (std::isalnum(ch) != 0) || ch == '_' || ch >= 0x80U;
    };

    std::size_t pos = plainIndex;
    if (pos >= text.size() || !isWordByte(static_cast<unsigned char>(text[pos])))
    {
        if (pos > 0 && isWordByte(static_cast<unsigned char>(text[pos - 1])))
            --pos;
        else
            return {plainIndex, plainIndex};
    }

    std::size_t begin = pos;
    while (begin > 0 && isWordByte(static_cast<unsigned char>(text[begin - 1])))
        --begin;
    // 避免落在 UTF-8 续字节上
    while (begin < text.size() && (static_cast<unsigned char>(text[begin]) & 0xC0U) == 0x80U)
        ++begin;

    std::size_t end = pos + 1;
    while (end < text.size() && isWordByte(static_cast<unsigned char>(text[end])))
        ++end;
    while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U)
        ++end;

    return {begin, end};
}

} // namespace ysDui::controls::content::detail
