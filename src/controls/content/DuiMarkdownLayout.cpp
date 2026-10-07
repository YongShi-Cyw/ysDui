/**
 * 文件名：DuiMarkdownLayout.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：将 Markdown AST 测量为可绘制布局（对标参考 HTML 的 .md-view 视觉）。
 */
#include "DuiMarkdownAst.hpp"
#include "DuiMarkdownLayoutInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "DuiMarkdownHighlight.hpp"

namespace ysDui::controls::content::detail {

/** 将单元格行内节点压成单行纯文本，供列宽预估。 */
void AppendPlainInline(const InlineNode& node, std::string& out)
{
    switch (node.kind)
    {
    case InlineKind::Text:
    case InlineKind::Code:
        out += node.text;
        break;
    case InlineKind::SoftBreak:
        out.push_back(' ');
        break;
    case InlineKind::HardBreak:
        out.push_back(' ');
        break;
    case InlineKind::Image:
        out += node.text.empty() ? "[image]" : node.text;
        break;
    case InlineKind::Link:
        if (node.children.empty())
            out += node.text.empty() ? node.href : node.text;
        else
            for (const InlineNode& child : node.children)
                AppendPlainInline(child, out);
        break;
    case InlineKind::Math:
        // 列宽预估：公式用源码文本近似，避免 GCC -Wswitch 因未覆盖枚举失败。
        out += node.text.empty() ? "[math]" : node.text;
        break;
    }
}

/** 是否为可优先断行的空白（对齐 imgui_markdown CalcWordWrap）。 */
[[nodiscard]] bool IsWrapBreakChar(char ch)
{
    return ch == ' ' || ch == '\t';
}

void LayoutInlines(LayoutContext& ctx, const std::vector<InlineNode>& nodes, int left, int right,
                   int& y, const std::string& inheritLink);

/** 将公式片段并入当前布局（相对坐标 → 绝对坐标）。 */
void MergeMathFragment(LayoutContext& ctx, MathFragment fragment, int x, int y)
{
    for (LaidDecor& decor : fragment.decors)
    {
        decor.bounds.left += x;
        decor.bounds.right += x;
        decor.bounds.top += y;
        decor.bounds.bottom += y;
        ctx.layout.decors.push_back(std::move(decor));
        ctx.NoteContentRight(decor.bounds.right);
    }
    for (LaidRun& run : fragment.runs)
    {
        run.bounds.left += x;
        run.bounds.right += x;
        run.bounds.top += y;
        run.bounds.bottom += y;
        if (run.selectable && !run.text.empty())
        {
            if (!ctx.layout.plainText.empty() && ctx.lastPlainTop >= 0 && y > ctx.lastPlainTop + 2)
                ctx.layout.plainText.push_back('\n');
            run.plainBegin = ctx.layout.plainText.size();
            ctx.layout.plainText += run.text;
            run.plainEnd = ctx.layout.plainText.size();
            ctx.lastPlainTop = y;
        }
        ctx.layout.runs.push_back(std::move(run));
        ctx.NoteContentRight(ctx.layout.runs.back().bounds.right);
    }
}

/** 水平流式行内布局：文本、行内代码、链接同排换行。 */
void LayoutInlines(LayoutContext& ctx, const std::vector<InlineNode>& nodes, int left, int right,
                   int& y, const std::string& inheritLink)
{
    int x = left;
    int lineTop = y;
    int lineHeight = ctx.LineHeight(ctx.baseStyle);
    std::size_t lineRunBegin = ctx.layout.runs.size();
    std::size_t lineDecorBegin = ctx.layout.decors.size();
    const int maxWidth = (std::max)(1, right - left);

    const auto centerLine = [&]()
    {
        // 同一行内各片段按 lineHeight 垂直居中（行内代码/中英文混排）
        for (std::size_t index = lineRunBegin; index < ctx.layout.runs.size(); ++index)
        {
            LaidRun& run = ctx.layout.runs[index];
            if (run.bounds.top < lineTop || run.bounds.top >= lineTop + lineHeight)
                continue;
            const int h = run.bounds.Height();
            const int centered = lineTop + (std::max)(0, lineHeight - h) / 2;
            const int delta = centered - run.bounds.top;
            if (delta == 0)
                continue;
            run.bounds.top += delta;
            run.bounds.bottom += delta;
        }
        for (std::size_t index = lineDecorBegin; index < ctx.layout.decors.size(); ++index)
        {
            LaidDecor& decor = ctx.layout.decors[index];
            if (decor.bounds.top < lineTop || decor.bounds.top >= lineTop + lineHeight)
                continue;
            const int h = decor.bounds.Height();
            const int centered = lineTop + (std::max)(0, lineHeight - h) / 2;
            const int delta = centered - decor.bounds.top;
            if (delta == 0)
                continue;
            decor.bounds.top += delta;
            decor.bounds.bottom += delta;
        }
    };

    const auto newLine = [&]()
    {
        centerLine();
        y = lineTop + lineHeight;
        lineTop = y;
        x = left;
        lineHeight = ctx.LineHeight(ctx.baseStyle);
        lineRunBegin = ctx.layout.runs.size();
        lineDecorBegin = ctx.layout.decors.size();
    };

    const auto placeChunk = [&](std::string_view text, const render::DuiTextStyle& style,
                                const std::string& link, bool pill, bool strike)
    {
        if (text.empty())
            return;
        std::size_t offset = 0;
        while (offset < text.size())
        {
            // 换行后跳过行首空白（imgui_markdown TextRegion::RenderTextWrapped）
            if (x == left)
            {
                while (offset < text.size() && IsWrapBreakChar(text[offset]))
                    ++offset;
                if (offset >= text.size())
                    break;
            }

            if (right - x < 8 && x > left)
            {
                newLine();
                continue;
            }

            std::size_t end = offset;
            std::size_t lastFit = offset;
            std::size_t lastBreak = offset; // 最近空白断点（断在空白之前）
            const int limit = (std::max)(1, right - x - (pill ? kInlineCodePadX * 2 : 0));
            while (end < text.size())
            {
                const std::size_t next =
                    end + Utf8Units(static_cast<unsigned char>(text[end]), text.size() - end);
                const auto metrics =
                    ctx.measurer->MeasureText(text.substr(offset, next - offset), style, {});
                if (metrics.size.width > limit && lastFit > offset)
                    break;
                if (IsWrapBreakChar(text[end]))
                    lastBreak = end;
                end = next;
                lastFit = end;
                if (metrics.size.width > limit)
                    break;
            }
            if (lastFit == offset)
            {
                if (x > left)
                {
                    newLine();
                    continue;
                }
                lastFit = end;
            }
            // 有后续内容时优先在空白处断行，避免把英文单词拦腰截断
            if (lastFit < text.size() && lastBreak > offset)
                lastFit = lastBreak;

            const bool wrapped = lastFit < text.size();
            const std::string_view piece = text.substr(offset, lastFit - offset);
            // 换行断点处的尾随空格不画到行尾；行内 SoftBreak 空格需保留
            std::string_view draw = piece;
            if (wrapped)
            {
                while (!draw.empty() && IsWrapBreakChar(draw.back()))
                    draw.remove_suffix(1);
            }

            if (!draw.empty())
            {
                const auto metrics = ctx.measurer->MeasureText(draw, style, {});
                const int textH = (std::max)(metrics.size.height, style.pointSize + 4);
                lineHeight = (std::max)(lineHeight, textH + (pill ? kInlineCodePadY * 2 : 0));
                const int textW = metrics.size.width;
                if (pill)
                {
                    const core::Rect pillRect{x, lineTop, x + textW + kInlineCodePadX * 2,
                                              lineTop + textH + kInlineCodePadY * 2};
                    ctx.AddDecor(pillRect, ctx.palette.inlineCodeBackground, 4);
                    ctx.AddRun({x + kInlineCodePadX, lineTop + kInlineCodePadY,
                                x + kInlineCodePadX + textW, lineTop + kInlineCodePadY + textH},
                               std::string(draw), style, link, strike);
                    x = pillRect.right + 2;
                }
                else
                {
                    ctx.AddRun({x, lineTop, x + textW, lineTop + textH}, std::string(draw), style,
                               link, strike);
                    x += textW;
                }
            }

            offset = lastFit;
            // 断在空白上时消费该空白，下一轮从下一词开始
            if (wrapped && offset < text.size() && IsWrapBreakChar(text[offset]))
                ++offset;
            if (wrapped && offset < text.size())
                newLine();
        }
    };

    std::function<void(const std::vector<InlineNode>&, const std::string&)> walk;
    walk = [&](const std::vector<InlineNode>& items, const std::string& link)
    {
        for (const InlineNode& node : items)
        {
            const bool strike = HasFlag(node.flags, InlineFlags::Strike);
            switch (node.kind)
            {
            case InlineKind::SoftBreak:
                placeChunk(" ", ctx.MakeStyle(node.flags, false, !link.empty()), link, false, strike);
                break;
            case InlineKind::HardBreak:
                newLine();
                break;
            case InlineKind::Text:
                placeChunk(node.text, ctx.MakeStyle(node.flags, false, !link.empty()), link, false,
                           strike);
                break;
            case InlineKind::Code:
                placeChunk(node.text, ctx.MakeStyle(node.flags, true, false), {}, true, false);
                break;
            case InlineKind::Link:
            {
                const std::string href = node.href.empty() ? link : node.href;
                if (node.children.empty())
                {
                    InlineNode fallback;
                    fallback.kind = InlineKind::Text;
                    fallback.flags = node.flags;
                    fallback.text = node.text.empty() ? node.href : node.text;
                    walk({fallback}, href);
                }
                else
                {
                    walk(node.children, href);
                }
                break;
            }
            case InlineKind::Math:
            {
                MathFragment fragment =
                    LayoutMathFragment(node.text, *ctx.measurer, ctx.baseStyle, node.mathDisplay);
                const int fragW = fragment.size.width;
                const int fragH = fragment.size.height;
                if (node.mathDisplay || fragW > right - x)
                {
                    if (x > left)
                        newLine();
                    const int ox = node.mathDisplay
                        ? left + (std::max)(0, (right - left - fragW) / 2)
                        : left;
                    const int oy = node.mathDisplay
                        ? lineTop
                        : lineTop + (std::max)(0, lineHeight - fragH) / 2;
                    MergeMathFragment(ctx, std::move(fragment), ox, oy);
                    lineHeight = (std::max)(lineHeight, fragH);
                    if (node.mathDisplay)
                    {
                        x = right;
                        newLine();
                    }
                    else
                        x = ox + fragW + 2;
                }
                else
                {
                    const int oy = lineTop + (std::max)(0, lineHeight - fragH) / 2;
                    MergeMathFragment(ctx, std::move(fragment), x, oy);
                    lineHeight = (std::max)(lineHeight, fragH);
                    x += fragW + 2;
                }
                break;
            }
            case InlineKind::Image:
            {
                if (x > left)
                    newLine();
                const std::string alt = node.text.empty() ? "image" : node.text;
                const int avail = (std::max)(1, right - left);
                std::shared_ptr<const render::DuiImage> decoded;
                if (ctx.imageResolver != nullptr && !node.href.empty())
                    decoded = (*ctx.imageResolver)(node.href);

                if (decoded != nullptr && !decoded->Empty() && !decoded->Size().Empty())
                {
                    const core::Size srcSize = decoded->Size();
                    int boxW = srcSize.width;
                    int boxH = srcSize.height;
                    if (boxW > avail && boxW > 0)
                    {
                        boxH = (std::max)(1, boxH * avail / boxW);
                        boxW = avail;
                    }
                    if (boxH > kImageMaxHeight && boxH > 0)
                    {
                        boxW = (std::max)(1, boxW * kImageMaxHeight / boxH);
                        boxH = kImageMaxHeight;
                    }
                    const core::Rect box{left, lineTop, left + boxW, lineTop + boxH};
                    LaidImage image;
                    image.bounds = box;
                    image.image = std::move(decoded);
                    image.src = node.href;
                    image.alt = alt;
                    ctx.layout.images.push_back(std::move(image));
                    // 可命中/可选的透明文本层（alt）
                    render::DuiTextStyle ghost = ctx.baseStyle;
                    ghost.color = {0, 0, 0, 0};
                    ctx.AddRun(box, alt, ghost, node.href, strike);
                    lineHeight = boxH;
                }
                else
                {
                    // 无图时画占位框（对齐 imgui_markdown）
                    const std::string label = "[图] " + alt;
                    render::DuiTextStyle labelStyle = ctx.baseStyle;
                    labelStyle.pointSize = (std::max)(11, ctx.baseStyle.pointSize - 1);
                    labelStyle.color = ctx.palette.imagePlaceholderText;
                    const auto metrics = ctx.measurer->MeasureText(label, labelStyle, {});
                    const int boxW = (std::min)(
                        avail, (std::max)(kImageMinWidth, metrics.size.width + kImagePad * 2));
                    const int boxH =
                        (std::max)(kImageDefaultHeight, metrics.size.height + kImagePad * 2);
                    const core::Rect box{left, lineTop, left + boxW, lineTop + boxH};
                    ctx.AddDecor(box, ctx.palette.imagePlaceholderFill, 6);
                    ctx.AddStroke(box, ctx.palette.imagePlaceholderStroke, 1.0f, 6);
                    const int textX = left + (boxW - metrics.size.width) / 2;
                    const int textY = lineTop + (boxH - metrics.size.height) / 2;
                    ctx.AddRun({textX, textY, textX + metrics.size.width, textY + metrics.size.height},
                               label, labelStyle, node.href, strike);
                    lineHeight = boxH;
                }
                x = right;
                newLine();
                break;
            }
            }
        }
    };

    walk(nodes, inheritLink);
    centerLine();
    y = lineTop + lineHeight;
    (void)maxWidth;
}

void LayoutBlock(LayoutContext& ctx, const BlockNode& block, int left, int right, int& y);

void LayoutBlock(LayoutContext& ctx, const BlockNode& block, int left, int right, int& y)
{
    switch (block.kind)
    {
    case BlockKind::Heading:
    {
        if (y > 0)
            y += kHeadingTopGap;
        render::DuiTextStyle style = ctx.baseStyle;
        style.bold = true;
        static constexpr int kSizes[] = {22, 18, 16, 15, 14, 14};
        const int level = (std::clamp)(block.headingLevel, 1, 6);
        style.pointSize = kSizes[level - 1];
        const render::DuiTextStyle old = ctx.baseStyle;
        ctx.baseStyle = style;
        LayoutInlines(ctx, block.inlines, left, right, y, {});
        ctx.baseStyle = old;
        // 对齐 imgui_markdown headingFormats.separator：H1/H2 标题下划分隔线
        if (level <= 2)
        {
            const int thickness = level == 1 ? 2 : 1;
            ctx.AddDecor({left, y + 4, right, y + 4 + thickness},
                         ctx.theme->Get(core::ThemeSlot::BorderLight));
            y += 4 + thickness + 2;
        }
        y += kHeadingBottomGap;
        break;
    }
    case BlockKind::Paragraph:
        LayoutInlines(ctx, block.inlines, left, right, y, {});
        y += kParagraphSpacing;
        break;
    case BlockKind::ThematicBreak:
        ctx.AddDecor({left, y + 8, right, y + 9}, ctx.theme->Get(core::ThemeSlot::BorderLight));
        y += 16 + kParagraphSpacing;
        break;
    case BlockKind::BlockQuote:
    {
        const int startY = y;
        y += kQuotePadY; // 内容上下留白，避免贴边
        const int contentLeft = left + kQuotePad + 3;
        const int contentRight = right - 4;
        render::DuiTextStyle old = ctx.baseStyle;
        ctx.baseStyle.color = ctx.theme->Get(core::ThemeSlot::TextSubtle);
        for (const BlockNode& child : block.children)
            LayoutBlock(ctx, child, contentLeft, contentRight, y);
        ctx.baseStyle = old;
        const int endY = (std::max)(startY + 20, y + kQuotePadY);
        LaidDecor bg;
        bg.bounds = {left, startY, right, endY};
        bg.fill = ctx.palette.quoteBackground;
        bg.radius = 6;
        bg.fillOnly = true;
        ctx.layout.decors.push_back(bg);
        LaidDecor bar;
        bar.bounds = {left, startY, left + 3, endY};
        bar.fill = ctx.palette.quoteAccent;
        bar.fillOnly = true;
        ctx.layout.decors.push_back(bar);
        y = endY + kParagraphSpacing;
        break;
    }
    case BlockKind::BulletList:
    case BlockKind::OrderedList:
    {
        int index = block.listStart;
        for (const BlockNode& item : block.children)
        {
            if (item.kind != BlockKind::ListItem)
                continue;
            const int itemTop = y;
            int markerWidth = kListIndent;
            if (item.taskItem)
            {
                const int boxTop = itemTop + 3;
                const core::Rect box{left, boxTop, left + kCheckboxSize, boxTop + kCheckboxSize};
                ctx.AddStroke(box, ctx.theme->Get(core::ThemeSlot::BorderHeavy), 1.2f, 3);
                if (item.taskChecked)
                {
                    // 简易勾选：填充浅底 + 对勾字符
                    ctx.AddDecor(box, {0, 122, 255, 40}, 3);
                    render::DuiTextStyle checkStyle = ctx.baseStyle;
                    checkStyle.pointSize = 10;
                    checkStyle.bold = true;
                    checkStyle.color = ctx.palette.link;
                    ctx.AddRun({left + 2, boxTop - 1, left + kCheckboxSize, boxTop + kCheckboxSize},
                               "✓", checkStyle);
                }
                markerWidth = kCheckboxSize + 8;
            }
            else if (block.kind == BlockKind::OrderedList)
            {
                const std::string marker = std::to_string(index++) + ".";
                render::DuiTextStyle markerStyle = ctx.baseStyle;
                markerStyle.color = ctx.theme->Get(core::ThemeSlot::TextSubtle);
                const auto metrics = ctx.measurer->MeasureText(marker, markerStyle, {});
                ctx.AddRun({left, itemTop, left + metrics.size.width, itemTop + metrics.size.height},
                           marker, markerStyle);
                markerWidth = (std::max)(kListIndent, metrics.size.width + 8);
            }
            else
            {
                render::DuiTextStyle markerStyle = ctx.baseStyle;
                markerStyle.color = ctx.theme->Get(core::ThemeSlot::TextSubtle);
                ctx.AddRun({left, itemTop, left + 10, itemTop + ctx.LineHeight(ctx.baseStyle)}, "•",
                           markerStyle);
            }

            const int itemLeft = left + markerWidth;
            int itemY = itemTop;
            if (!item.inlines.empty())
                LayoutInlines(ctx, item.inlines, itemLeft, right, itemY, {});
            for (const BlockNode& child : item.children)
                LayoutBlock(ctx, child, itemLeft, right, itemY);
            y = (std::max)(itemTop + ctx.LineHeight(ctx.baseStyle), itemY);
            y += block.tight ? 4 : kParagraphSpacing / 2;
        }
        y += kParagraphSpacing;
        break;
    }
    case BlockKind::CodeFence:
        LayoutCodeFenceBlock(ctx, block, left, right, y);
        break;
    case BlockKind::Table:
        LayoutTableBlock(ctx, block, left, right, y);
        break;
    case BlockKind::ListItem:
    case BlockKind::TableRow:
    case BlockKind::TableCell:
        for (const BlockNode& child : block.children)
            LayoutBlock(ctx, child, left, right, y);
        break;
    }
}


[[nodiscard]] std::uint64_t HashMix(std::uint64_t h, std::uint64_t v)
{
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

[[nodiscard]] std::uint64_t HashString(std::string_view text)
{
    std::uint64_t h = 14695981039346656037ULL;
    for (unsigned char ch : text)
    {
        h ^= ch;
        h *= 1099511628211ULL;
    }
    return h;
}

void HashInline(const InlineNode& node, std::uint64_t& h)
{
    h = HashMix(h, static_cast<std::uint64_t>(node.kind));
    h = HashMix(h, static_cast<std::uint64_t>(node.flags));
    h = HashMix(h, HashString(node.text));
    h = HashMix(h, HashString(node.href));
    h = HashMix(h, node.mathDisplay ? 1ULL : 0ULL);
    for (const InlineNode& child : node.children)
        HashInline(child, h);
}

void HashBlockNode(const BlockNode& block, std::uint64_t& h)
{
    h = HashMix(h, static_cast<std::uint64_t>(block.kind));
    h = HashMix(h, static_cast<std::uint64_t>(block.headingLevel));
    h = HashMix(h, static_cast<std::uint64_t>(block.listStart));
    h = HashMix(h, block.ordered ? 1ULL : 0ULL);
    h = HashMix(h, block.tight ? 1ULL : 0ULL);
    h = HashMix(h, block.headerCell ? 1ULL : 0ULL);
    h = HashMix(h, block.taskItem ? 1ULL : 0ULL);
    h = HashMix(h, block.taskChecked ? 1ULL : 0ULL);
    h = HashMix(h, static_cast<std::uint64_t>(block.cellAlign));
    h = HashMix(h, HashString(block.language));
    h = HashMix(h, HashString(block.code));
    for (const InlineNode& inlineNode : block.inlines)
        HashInline(inlineNode, h);
    for (const BlockNode& child : block.children)
        HashBlockNode(child, h);
}

std::uint64_t FingerprintBlock(const BlockNode& block)
{
    std::uint64_t h = 0xcbf29ce484222325ULL;
    HashBlockNode(block, h);
    return h;
}

void RefreshBlockFingerprints(MarkdownDocument& document)
{
    document.blockFingerprints.clear();
    document.blockFingerprints.reserve(document.blocks.size());
    for (const BlockNode& block : document.blocks)
        document.blockFingerprints.push_back(FingerprintBlock(block));
}

std::size_t CountReusablePrefixBlocks(const MarkdownDocument& document,
                                      const std::vector<std::uint64_t>& previousFingerprints)
{
    const std::vector<std::uint64_t>* fps = &document.blockFingerprints;
    std::vector<std::uint64_t> computed;
    if (fps->size() != document.blocks.size())
    {
        computed.reserve(document.blocks.size());
        for (const BlockNode& block : document.blocks)
            computed.push_back(FingerprintBlock(block));
        fps = &computed;
    }

    const std::size_t limit = (std::min)(fps->size(), previousFingerprints.size());
    std::size_t index = 0;
    for (; index < limit; ++index)
    {
        if ((*fps)[index] != previousFingerprints[index])
            break;
    }
    return index;
}

MarkdownLayout LayoutMarkdown(const MarkdownDocument& document, render::DuiTextMeasurer& measurer,
                              int width, const render::DuiTextStyle& baseStyle, bool codeHighlight,
                              bool codeCopyable, const core::DuiTheme& theme, bool streamingCursor,
                              const MarkdownPalette& palette,
                              const MarkdownImageResolver& imageResolver,
                              const LayoutReusePrefix* reusePrefix)
{
    LayoutContext ctx;
    ctx.measurer = &measurer;
    ctx.width = (std::max)(40, width);
    ctx.baseStyle = baseStyle;
    ctx.baseStyle.pointSize = baseStyle.pointSize > 0 ? baseStyle.pointSize : 14;
    ctx.codeHighlight = codeHighlight;
    ctx.codeCopyable = codeCopyable;
    ctx.theme = &theme;
    ctx.streamingCursor = streamingCursor;
    ctx.palette = palette;
    ctx.imageResolver = imageResolver ? &imageResolver : nullptr;
    ctx.layout.contentWidth = ctx.width;
    ctx.layout.layoutWidth = ctx.width;

    int y = 0;
    const int left = 0;
    const int right = ctx.width;
    std::size_t startBlock = 0;

    if (reusePrefix != nullptr && reusePrefix->source != nullptr && reusePrefix->blockCount > 0)
    {
        MarkdownLayout& src = *reusePrefix->source;
        const std::size_t bandCount =
            (std::min)(reusePrefix->blockCount, src.bands.size());
        if (bandCount > 0)
        {
            const LaidBand last = src.bands[bandCount - 1];
            // 原地截断到稳定前缀，再 move 进新上下文（避免整表深拷贝）
            if (last.runEnd < src.runs.size())
                src.runs.resize(last.runEnd);
            if (last.decorEnd < src.decors.size())
                src.decors.resize(last.decorEnd);
            if (last.imageEnd < src.images.size())
                src.images.resize(last.imageEnd);
            if (bandCount < src.bands.size())
                src.bands.resize(bandCount);
            if (last.plainEnd < src.plainText.size())
                src.plainText.resize(last.plainEnd);

            const int reusedContentWidth = src.contentWidth;
            ctx.layout = std::move(src);
            ctx.layout.contentWidth = (std::max)(ctx.width, reusedContentWidth);
            ctx.layout.layoutWidth = ctx.width;
            ctx.lastPlainTop = last.y1 > 0 ? last.y1 - 1 : -1;
            y = last.y1;
            startBlock = bandCount;
        }
    }

    for (std::size_t index = startBlock; index < document.blocks.size(); ++index)
    {
        LaidBand band;
        band.y0 = y;
        band.runBegin = ctx.layout.runs.size();
        band.decorBegin = ctx.layout.decors.size();
        band.imageBegin = ctx.layout.images.size();
        band.plainBegin = ctx.layout.plainText.size();

        LayoutBlock(ctx, document.blocks[index], left, right, y);

        band.y1 = y;
        band.runEnd = ctx.layout.runs.size();
        band.decorEnd = ctx.layout.decors.size();
        band.imageEnd = ctx.layout.images.size();
        band.plainEnd = ctx.layout.plainText.size();
        ctx.layout.bands.push_back(band);
    }

    if (streamingCursor)
    {
        ctx.AddDecor({left, y, left + 2, y + ctx.LineHeight(ctx.baseStyle)}, ctx.palette.quoteAccent);
        y += ctx.LineHeight(ctx.baseStyle);
    }

    ctx.layout.size = {(std::max)(ctx.width, ctx.layout.contentWidth), y};
    ctx.layout.linkHoverColor = ctx.palette.linkHover;
    return ctx.layout;
}

} // namespace ysDui::controls::content::detail
