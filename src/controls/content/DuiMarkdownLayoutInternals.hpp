/**
 * 文件名：DuiMarkdownLayoutInternals.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：Markdown 布局内部共享上下文与块级布局入口（Table/CodeFence 拆分用）。
 */
#pragma once

#include "DuiMarkdownAst.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace ysDui::controls::content::detail {

constexpr int kParagraphSpacing = 8;  // 段落后间距
constexpr int kHeadingTopGap = 16;    // 标题上间距
constexpr int kHeadingBottomGap = 8;  // 标题下间距
constexpr int kCodePaddingX = 14;     // 代码块水平内边距
constexpr int kCodePaddingY = 12;     // 代码块垂直内边距
constexpr int kCodeHeaderHeight = 34; // 代码块标题栏高度（含复制按钮垂直居中余量）
constexpr int kCodeRadius = 8;        // 代码块圆角
constexpr int kListIndent = 22;       // 列表缩进
constexpr int kQuotePad = 14;         // 引用水平内边距
constexpr int kQuotePadY = 10;        // 引用垂直内边距
constexpr int kTableCellPadX = 14;    // 表格水平内边距（对齐常见预览）
constexpr int kTableCellPadY = 10;    // 表格垂直内边距
constexpr int kTableGridLine = 1;     // 网格线厚度
constexpr int kCopyButtonWidth = 56;  // 复制按钮宽度
constexpr int kLineNumberWidth = 36;  // 行号列宽
constexpr int kInlineCodePadX = 6;    // 行内代码水平内边距
constexpr int kInlineCodePadY = 2;    // 行内代码垂直内边距
constexpr int kCheckboxSize = 14;     // 任务复选框边长
constexpr int kImageMinWidth = 160;     // 图片占位最小宽度
constexpr int kImageDefaultHeight = 72; // 图片占位默认高度
constexpr int kImagePad = 10;           // 图片占位内边距
constexpr int kImageMaxHeight = 240;    // 真实图片最大高度
constexpr int kTableMinColWidth = 48;   // 表格列最小宽度

/** 单次布局过程的可变上下文。 */
struct LayoutContext final {
    render::DuiTextMeasurer* measurer{};
    int width{};
    render::DuiTextStyle baseStyle;
    bool codeHighlight{};
    bool codeCopyable{};
    const core::DuiTheme* theme{};
    bool streamingCursor{};
    MarkdownPalette palette{MarkdownPalette::Make(true)};
    const MarkdownImageResolver* imageResolver{};
    MarkdownLayout layout;
    int lastPlainTop{-1}; // 上一贡献纯文本的行顶，用于插入换行

    void NoteContentRight(int right)
    {
        layout.contentWidth = (std::max)(layout.contentWidth, right);
    }

    void AddDecor(core::Rect bounds, core::Color fill, int radius = 0, bool fillOnly = true)
    {
        LaidDecor decor;
        decor.bounds = bounds;
        decor.fill = fill;
        decor.radius = radius;
        decor.fillOnly = fillOnly;
        layout.decors.push_back(decor);
        NoteContentRight(bounds.right);
    }

    void AddStroke(core::Rect bounds, core::Color stroke, float strokeWidth, int radius = 0)
    {
        LaidDecor decor;
        decor.bounds = bounds;
        decor.stroke = stroke;
        decor.strokeWidth = strokeWidth;
        decor.radius = radius;
        decor.fillOnly = false;
        decor.strokeOnly = true;
        layout.decors.push_back(decor);
        NoteContentRight(bounds.right);
    }

    void AddRun(core::Rect bounds, std::string text, render::DuiTextStyle style,
                std::string linkHref = {}, bool strike = false, bool isCopy = false,
                std::string copyCode = {}, std::string copyLang = {}, bool selectable = true,
                bool isCodeChrome = false,
                render::DuiTextAlignment alignment = render::DuiTextAlignment::Start)
    {
        LaidRun run;
        run.bounds = bounds;
        run.text = std::move(text);
        run.style = std::move(style);
        run.linkHref = std::move(linkHref);
        run.strike = strike;
        run.isCopyButton = isCopy;
        run.isCodeChrome = isCodeChrome;
        run.alignment = alignment;
        run.selectable = selectable && !isCopy && !isCodeChrome;
        run.copyCode = std::move(copyCode);
        run.copyLanguage = std::move(copyLang);
        if (run.selectable && !run.text.empty())
        {
            if (!layout.plainText.empty() && lastPlainTop >= 0 && bounds.top > lastPlainTop + 2)
                layout.plainText.push_back('\n');
            run.plainBegin = layout.plainText.size();
            layout.plainText += run.text;
            run.plainEnd = layout.plainText.size();
            lastPlainTop = bounds.top;
        }
        else
        {
            run.plainBegin = run.plainEnd = layout.plainText.size();
            run.selectable = false;
        }
        layout.runs.push_back(std::move(run));
        NoteContentRight(bounds.right);
    }

    [[nodiscard]] int LineHeight(const render::DuiTextStyle& style) const
    {
        return (std::max)(style.pointSize + 6, static_cast<int>(std::lround(style.pointSize * 1.7)));
    }

    [[nodiscard]] render::DuiTextStyle MakeStyle(InlineFlags flags, bool inlineCode, bool link) const
    {
        render::DuiTextStyle style = baseStyle;
        style.bold = HasFlag(flags, InlineFlags::Strong);
        style.italic = HasFlag(flags, InlineFlags::Emphasis);
        style.underline = link;
        if (inlineCode)
        {
            style.family = "Consolas, Cascadia Mono, Courier New";
            style.pointSize = (std::max)(8, static_cast<int>(std::lround(baseStyle.pointSize * 0.88)));
            style.color = palette.inlineCodeForeground;
            style.bold = false;
            style.italic = false;
            style.underline = false;
        }
        else if (link)
        {
            style.color = palette.link;
            style.underline = true;
        }
        return style;
    }
};

/** UTF-8 下一码点字节数。 */
[[nodiscard]] inline std::size_t Utf8Units(unsigned char first, std::size_t remain)
{
    const std::size_t width = first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
    return (std::min)(width, remain);
}

void AppendPlainInline(const InlineNode& node, std::string& out);
void LayoutInlines(LayoutContext& ctx, const std::vector<InlineNode>& nodes, int left, int right,
                   int& y, const std::string& inheritLink);
void MergeMathFragment(LayoutContext& ctx, MathFragment fragment, int x, int y);
void LayoutTableBlock(LayoutContext& ctx, const BlockNode& block, int left, int right, int& y);
void LayoutCodeFenceBlock(LayoutContext& ctx, const BlockNode& block, int left, int right, int& y);

} // namespace ysDui::controls::content::detail
