/**
 * 文件名：DuiMarkdownLayoutCodeFence.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：Markdown 代码围栏与 math 块级公式布局。
 */
#include "DuiMarkdownLayoutInternals.hpp"

#include "DuiMarkdownHighlight.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace ysDui::controls::content::detail {

void LayoutCodeFenceBlock(LayoutContext& ctx, const BlockNode& block, int left, int right, int& y)
{
    // ```math 围栏按块级公式渲染
    std::string langLower = block.language;
    for (char& ch : langLower)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (langLower == "math" || langLower == "latex")
    {
        if (y > 0)
            y += 4;
        MathFragment fragment =
            LayoutMathFragment(block.code, *ctx.measurer, ctx.baseStyle, true);
        const int fragH = fragment.size.height;
        const int ox = left + (std::max)(0, (right - left - fragment.size.width) / 2);
        MergeMathFragment(ctx, std::move(fragment), ox, y);
        y += fragH + kParagraphSpacing;
        return;
    }

    if (y > 0)
        y += 4;
    const int blockTop = y;
    const bool chrome = ctx.codeHighlight;
    const std::size_t decorBegin = ctx.layout.decors.size();
    const int contentTop = chrome ? blockTop + kCodeHeaderHeight : blockTop;

    render::DuiTextStyle codeStyle = ctx.baseStyle;
    codeStyle.family = "Consolas, Cascadia Mono, Courier New";
    codeStyle.pointSize = 13;
    codeStyle.color = ctx.palette.codePlain;

    const int gutter = chrome ? kLineNumberWidth : 0;
    const int codeLeft = left + kCodePaddingX + gutter;
    const int codeRight = right - kCodePaddingX;
    int codeY = contentTop + kCodePaddingY;
    int lineNo = 1;
    std::string_view remaining = block.code;
    while (!remaining.empty() && remaining.back() == '\n')
        remaining.remove_suffix(1);
    if (remaining.empty())
        codeY += ctx.LineHeight(codeStyle);

    /** 代码行软换行：超宽时按码点折行，避免被 Clip 裁切（对齐 imgui 自动 wrap）。 */
    const auto emitWrappedCode = [&](std::string_view text, const render::DuiTextStyle& style,
                                     int startX, int& y)
    {
        const int lineH = (std::max)(ctx.measurer->MeasureText("Ag", style, {}).size.height,
                                     static_cast<int>(std::lround(style.pointSize * 1.55)));
        std::size_t offset = 0;
        int x = startX;
        while (offset < text.size())
        {
            const int limit = (std::max)(1, codeRight - x);
            std::size_t end = offset;
            std::size_t lastFit = offset;
            while (end < text.size())
            {
                const std::size_t next =
                    end + Utf8Units(static_cast<unsigned char>(text[end]), text.size() - end);
                const auto metrics =
                    ctx.measurer->MeasureText(text.substr(offset, next - offset), style, {});
                if (metrics.size.width > limit && lastFit > offset)
                    break;
                end = next;
                lastFit = end;
                if (metrics.size.width > limit)
                    break;
            }
            if (lastFit == offset)
            {
                if (x > startX)
                {
                    y += lineH;
                    x = startX;
                    continue;
                }
                lastFit = end > offset ? end : offset + Utf8Units(static_cast<unsigned char>(text[offset]),
                                                                  text.size() - offset);
            }
            const std::string_view piece = text.substr(offset, lastFit - offset);
            const auto metrics = ctx.measurer->MeasureText(piece, style, {});
            ctx.AddRun({x, y, x + metrics.size.width, y + lineH}, std::string(piece), style);
            offset = lastFit;
            if (offset < text.size())
            {
                y += lineH;
                x = startX;
            }
            else
            {
                x += metrics.size.width;
            }
        }
        return x;
    };

    while (!remaining.empty())
    {
        const auto nl = remaining.find('\n');
        const std::string_view line =
            nl == std::string_view::npos ? remaining : remaining.substr(0, nl);
        const int lineH = (std::max)(ctx.measurer->MeasureText("Ag", codeStyle, {}).size.height,
                                     static_cast<int>(std::lround(codeStyle.pointSize * 1.55)));
        const int rowTop = codeY;
        if (chrome)
        {
            render::DuiTextStyle numStyle = codeStyle;
            numStyle.color = ctx.palette.codeChromeForeground;
            ctx.AddRun({left + 8, rowTop, left + kLineNumberWidth - 4, rowTop + lineH},
                       std::to_string(lineNo++), numStyle);
            int tokenX = codeLeft;
            int tokenY = rowTop;
            for (const CodeToken& token : HighlightCodeLine(line, block.language))
            {
                render::DuiTextStyle tokStyle = codeStyle;
                tokStyle.color = ctx.palette.TokenColor(token.kind);
                if (token.kind == CodeTokenKind::Comment)
                    tokStyle.italic = true;
                // 同一逻辑行内 token 连续排；超宽时整段折到下一视觉行
                std::size_t tokOff = 0;
                while (tokOff < token.text.size())
                {
                    const int limit = (std::max)(1, codeRight - tokenX);
                    std::size_t end = tokOff;
                    std::size_t lastFit = tokOff;
                    while (end < token.text.size())
                    {
                        const std::size_t next =
                            end + Utf8Units(static_cast<unsigned char>(token.text[end]),
                                            token.text.size() - end);
                        const auto metrics = ctx.measurer->MeasureText(
                            std::string_view(token.text).substr(tokOff, next - tokOff), tokStyle,
                            {});
                        if (metrics.size.width > limit && lastFit > tokOff)
                            break;
                        end = next;
                        lastFit = end;
                        if (metrics.size.width > limit)
                            break;
                    }
                    if (lastFit == tokOff)
                    {
                        if (tokenX > codeLeft)
                        {
                            tokenY += lineH;
                            tokenX = codeLeft;
                            continue;
                        }
                        lastFit = end > tokOff
                            ? end
                            : tokOff + Utf8Units(static_cast<unsigned char>(token.text[tokOff]),
                                                 token.text.size() - tokOff);
                    }
                    const std::string_view piece =
                        std::string_view(token.text).substr(tokOff, lastFit - tokOff);
                    const auto metrics = ctx.measurer->MeasureText(piece, tokStyle, {});
                    ctx.AddRun({tokenX, tokenY, tokenX + metrics.size.width, tokenY + lineH},
                               std::string(piece), tokStyle);
                    tokOff = lastFit;
                    tokenX += metrics.size.width;
                    if (tokOff < token.text.size())
                    {
                        tokenY += lineH;
                        tokenX = codeLeft;
                    }
                }
            }
            codeY = tokenY + lineH;
        }
        else
        {
            (void)emitWrappedCode(line, codeStyle, codeLeft, codeY);
            codeY += lineH;
        }
        if (nl == std::string_view::npos)
            break;
        remaining.remove_prefix(nl + 1);
    }
    codeY += kCodePaddingY;

    LaidDecor bodyBg;
    bodyBg.bounds = {left, blockTop, right, codeY};
    bodyBg.fill = ctx.palette.codeBlockBackground;
    bodyBg.radius = kCodeRadius;
    bodyBg.fillOnly = true;
    ctx.layout.decors.insert(
        ctx.layout.decors.begin() + static_cast<std::ptrdiff_t>(decorBegin), bodyBg);
    if (chrome)
    {
        LaidDecor headBg;
        headBg.bounds = {left, blockTop, right, blockTop + kCodeHeaderHeight};
        headBg.fill = ctx.palette.codeHeaderBackground;
        headBg.fillOnly = true;
        ctx.layout.decors.insert(
            ctx.layout.decors.begin() + static_cast<std::ptrdiff_t>(decorBegin + 1), headBg);

        render::DuiTextStyle langStyle = ctx.baseStyle;
        langStyle.family = "Consolas, Cascadia Mono, Courier New";
        langStyle.pointSize = 12;
        langStyle.color = ctx.palette.codeChromeForeground;
        // 整栏高度作 bounds，DrawText Center + wordWrap=false → 垂直居中
        ctx.AddRun({left + 12, blockTop, right - kCopyButtonWidth - 12,
                    blockTop + kCodeHeaderHeight},
                   block.language.empty() ? "code" : block.language, langStyle, {}, false, false,
                   {}, {}, false, true, render::DuiTextAlignment::Start);
        if (ctx.codeCopyable)
        {
            const core::Rect btn{right - kCopyButtonWidth - 8, blockTop + 4, right - 8,
                                 blockTop + kCodeHeaderHeight - 4};
            ctx.AddStroke(btn, ctx.palette.codeButtonStroke, 1.0f, 4);
            render::DuiTextStyle copyStyle = ctx.baseStyle;
            copyStyle.pointSize = 12;
            copyStyle.color = ctx.palette.codeButtonForeground;
            ctx.AddRun(btn, "复制", copyStyle, {}, false, true, block.code, block.language, false,
                       false, render::DuiTextAlignment::Center);
        }
    }
    y = codeY + 12;
}

} // namespace ysDui::controls::content::detail
