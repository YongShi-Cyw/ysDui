/**
 * 文件名：DuiMarkdownHighlight.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：轻量代码着色与 Markdown 浅/深色表面色板。
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::controls::content::detail {

/** 代码着色 token 类别。 */
enum class CodeTokenKind : std::uint8_t {
    Plain,    // 普通文本
    Keyword,  // 关键字
    String,   // 字符串字面量
    Comment,  // 注释
    Number,   // 数字
    Function, // 函数名（标识符后紧跟 '('）
};

/** 单行内的着色片段。 */
struct CodeToken final {
    CodeTokenKind kind{CodeTokenKind::Plain};
    std::string text;
};

/**
 * 对一行代码做轻量分词着色。
 * @param line 不含换行的一行源码。
 * @param language 语言标签（如 js/cpp/python），大小写不敏感；未知时仍做通用着色。
 */
[[nodiscard]] std::vector<CodeToken> HighlightCodeLine(std::string_view line, std::string_view language);

/**
 * Markdown 表面色板：代码块 / 行内码 / 引用 / 链接随浅色或深色外观切换。
 * Dark 对齐 VS Code Dark+；Light 对齐 GitHub 浅色预览。
 */
struct MarkdownPalette final {
    bool dark{true};

    core::Color codePlain{};
    core::Color codeBlockBackground{};
    core::Color codeHeaderBackground{};
    core::Color codeChromeForeground{}; // 语言标签、行号
    core::Color codeButtonStroke{};
    core::Color codeButtonForeground{};
    core::Color inlineCodeForeground{};
    core::Color inlineCodeBackground{};
    core::Color quoteAccent{};
    core::Color quoteBackground{};
    core::Color link{};
    core::Color linkHover{};
    core::Color imagePlaceholderFill{};
    core::Color imagePlaceholderStroke{};
    core::Color imagePlaceholderText{};
    core::Color tableBorder{};      // 表格网格线
    core::Color tableHeaderFill{};  // 表头行背景
    core::Color tableBodyFill{};    // 表体行背景（通常透明/白）

    /** @param dark true=深色代码表面；false=浅色。 */
    [[nodiscard]] static MarkdownPalette Make(bool dark);

    /** 按 token 类别取着色（随 dark/light 变化）。 */
    [[nodiscard]] core::Color TokenColor(CodeTokenKind kind) const;
};

/** 兼容旧调用：默认深色色板单项（测试与过渡期）。 */
[[nodiscard]] core::Color CodeTokenColor(CodeTokenKind kind);
[[nodiscard]] core::Color CodePlainColor();
[[nodiscard]] core::Color CodeBlockBackground();
[[nodiscard]] core::Color CodeHeaderBackground();
[[nodiscard]] core::Color InlineCodeForeground();
[[nodiscard]] core::Color InlineCodeBackground();
[[nodiscard]] core::Color QuoteAccent();
[[nodiscard]] core::Color QuoteBackground();
[[nodiscard]] core::Color LinkColor();
[[nodiscard]] core::Color LinkHoverColor();

} // namespace ysDui::controls::content::detail
