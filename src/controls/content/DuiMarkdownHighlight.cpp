/**
 * 文件名：DuiMarkdownHighlight.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：实现轻量代码着色与 Markdown 浅/深色表面色板。
 */
#include "DuiMarkdownHighlight.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace ysDui::controls::content::detail {
namespace {

std::string ToLower(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const char ch : text)
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    return out;
}

const std::unordered_set<std::string>& KeywordsFor(std::string_view language)
{
    static const std::unordered_set<std::string> kJs{
        "function", "return", "const", "let", "var", "if", "else", "for", "while", "do", "switch",
        "case", "break", "continue", "class", "new", "this", "typeof", "instanceof", "import",
        "export", "default", "from", "async", "await", "try", "catch", "finally", "throw", "null",
        "undefined", "true", "false", "of", "in", "void", "yield",
    };
    static const std::unordered_set<std::string> kCpp{
        "int", "return", "if", "else", "for", "while", "do", "switch", "case", "break", "continue",
        "class", "struct", "public", "private", "protected", "virtual", "override", "const",
        "constexpr", "static", "void", "bool", "true", "false", "nullptr", "auto", "using",
        "namespace", "template", "typename", "new", "delete", "this", "sizeof", "include",
    };
    static const std::unordered_set<std::string> kPy{
        "def", "return", "if", "elif", "else", "for", "while", "class", "import", "from", "as",
        "try", "except", "finally", "raise", "with", "yield", "lambda", "True", "False", "None",
        "and", "or", "not", "in", "is", "pass", "break", "continue", "global", "nonlocal", "async",
        "await",
    };
    static const std::unordered_set<std::string> kGeneric = [] {
        std::unordered_set<std::string> set = kJs;
        set.insert(kCpp.begin(), kCpp.end());
        set.insert(kPy.begin(), kPy.end());
        return set;
    }();

    const std::string lang = ToLower(language);
    if (lang == "js" || lang == "javascript" || lang == "ts" || lang == "typescript")
        return kJs;
    if (lang == "c" || lang == "cpp" || lang == "c++" || lang == "cxx" || lang == "h" || lang == "hpp")
        return kCpp;
    if (lang == "py" || lang == "python")
        return kPy;
    return kGeneric;
}

[[nodiscard]] bool IsIdentStart(char ch)
{
    return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '$';
}

[[nodiscard]] bool IsIdent(char ch)
{
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '$';
}

} // namespace

MarkdownPalette MarkdownPalette::Make(bool dark)
{
    MarkdownPalette palette;
    palette.dark = dark;
    if (dark)
    {
        palette.codePlain = {212, 212, 212, 255};
        palette.codeBlockBackground = {30, 30, 30, 255};
        palette.codeHeaderBackground = {45, 45, 45, 255};
        palette.codeChromeForeground = {170, 170, 170, 255};
        palette.codeButtonStroke = {85, 85, 85, 255};
        palette.codeButtonForeground = {204, 204, 204, 255};
        palette.inlineCodeForeground = {199, 37, 78, 255};
        palette.inlineCodeBackground = {128, 128, 128, 38};
        palette.quoteAccent = {52, 199, 89, 255};
        palette.quoteBackground = {52, 199, 89, 20};
        palette.link = {0, 122, 255, 255};
        palette.linkHover = {0, 90, 200, 255};
        palette.imagePlaceholderFill = {45, 45, 45, 255};
        palette.imagePlaceholderStroke = {85, 85, 85, 255};
        palette.imagePlaceholderText = {170, 170, 170, 255};
        palette.tableBorder = {70, 70, 75, 255};
        palette.tableHeaderFill = {50, 50, 55, 255};
        palette.tableBodyFill = {0, 0, 0, 0};
    }
    else
    {
        // GitHub 浅色预览倾向
        palette.codePlain = {36, 41, 47, 255};
        palette.codeBlockBackground = {246, 248, 250, 255};
        palette.codeHeaderBackground = {234, 238, 242, 255};
        palette.codeChromeForeground = {87, 96, 106, 255};
        palette.codeButtonStroke = {208, 215, 222, 255};
        palette.codeButtonForeground = {36, 41, 47, 255};
        palette.inlineCodeForeground = {207, 34, 46, 255};
        palette.inlineCodeBackground = {175, 184, 193, 40};
        palette.quoteAccent = {26, 127, 55, 255};
        palette.quoteBackground = {26, 127, 55, 18};
        palette.link = {9, 105, 218, 255};
        palette.linkHover = {5, 80, 180, 255};
        palette.imagePlaceholderFill = {246, 248, 250, 255};
        palette.imagePlaceholderStroke = {208, 215, 222, 255};
        palette.imagePlaceholderText = {87, 96, 106, 255};
        // 对齐常见 Markdown 预览表：浅灰网格 + 表头底
        palette.tableBorder = {224, 224, 224, 255};
        palette.tableHeaderFill = {247, 247, 247, 255};
        palette.tableBodyFill = {255, 255, 255, 255};
    }
    return palette;
}

core::Color MarkdownPalette::TokenColor(CodeTokenKind kind) const
{
    if (dark)
    {
        switch (kind)
        {
        case CodeTokenKind::Keyword:
            return {86, 156, 214, 255};
        case CodeTokenKind::String:
            return {206, 145, 120, 255};
        case CodeTokenKind::Comment:
            return {106, 153, 85, 255};
        case CodeTokenKind::Number:
            return {181, 206, 168, 255};
        case CodeTokenKind::Function:
            return {220, 220, 170, 255};
        case CodeTokenKind::Plain:
        default:
            return codePlain;
        }
    }

    switch (kind)
    {
    case CodeTokenKind::Keyword:
        return {207, 34, 46, 255}; // 浅红关键字
    case CodeTokenKind::String:
        return {11, 110, 76, 255};
    case CodeTokenKind::Comment:
        return {111, 115, 122, 255};
    case CodeTokenKind::Number:
        return {5, 80, 174, 255};
    case CodeTokenKind::Function:
        return {130, 80, 223, 255};
    case CodeTokenKind::Plain:
    default:
        return codePlain;
    }
}

core::Color CodeTokenColor(CodeTokenKind kind)
{
    return MarkdownPalette::Make(true).TokenColor(kind);
}
core::Color CodePlainColor() { return MarkdownPalette::Make(true).codePlain; }
core::Color CodeBlockBackground() { return MarkdownPalette::Make(true).codeBlockBackground; }
core::Color CodeHeaderBackground() { return MarkdownPalette::Make(true).codeHeaderBackground; }
core::Color InlineCodeForeground() { return MarkdownPalette::Make(true).inlineCodeForeground; }
core::Color InlineCodeBackground() { return MarkdownPalette::Make(true).inlineCodeBackground; }
core::Color QuoteAccent() { return MarkdownPalette::Make(true).quoteAccent; }
core::Color QuoteBackground() { return MarkdownPalette::Make(true).quoteBackground; }
core::Color LinkColor() { return MarkdownPalette::Make(true).link; }
core::Color LinkHoverColor() { return MarkdownPalette::Make(true).linkHover; }

std::vector<CodeToken> HighlightCodeLine(std::string_view line, std::string_view language)
{
    std::vector<CodeToken> tokens;
    const auto& keywords = KeywordsFor(language);
    std::size_t i = 0;
    while (i < line.size())
    {
        const char ch = line[i];
        if ((ch == '/' && i + 1 < line.size() && line[i + 1] == '/') || ch == '#')
        {
            tokens.push_back({CodeTokenKind::Comment, std::string(line.substr(i))});
            break;
        }
        if (ch == '/' && i + 1 < line.size() && line[i + 1] == '*')
        {
            const auto end = line.find("*/", i + 2);
            const auto last = end == std::string_view::npos ? line.size() : end + 2;
            tokens.push_back({CodeTokenKind::Comment, std::string(line.substr(i, last - i))});
            i = last;
            continue;
        }
        if (ch == '"' || ch == '\'' || ch == '`')
        {
            const char quote = ch;
            std::size_t j = i + 1;
            while (j < line.size())
            {
                if (line[j] == '\\' && j + 1 < line.size())
                {
                    j += 2;
                    continue;
                }
                if (line[j] == quote)
                {
                    ++j;
                    break;
                }
                ++j;
            }
            tokens.push_back({CodeTokenKind::String, std::string(line.substr(i, j - i))});
            i = j;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0)
        {
            std::size_t j = i + 1;
            while (j < line.size()
                   && (std::isdigit(static_cast<unsigned char>(line[j])) != 0 || line[j] == '.'
                       || line[j] == 'x' || line[j] == 'X' || line[j] == 'e' || line[j] == 'E'))
                ++j;
            tokens.push_back({CodeTokenKind::Number, std::string(line.substr(i, j - i))});
            i = j;
            continue;
        }
        if (IsIdentStart(ch))
        {
            std::size_t j = i + 1;
            while (j < line.size() && IsIdent(line[j]))
                ++j;
            const std::string word(line.substr(i, j - i));
            CodeTokenKind kind = CodeTokenKind::Plain;
            if (keywords.contains(word))
                kind = CodeTokenKind::Keyword;
            else
            {
                std::size_t k = j;
                while (k < line.size() && (line[k] == ' ' || line[k] == '\t'))
                    ++k;
                if (k < line.size() && line[k] == '(')
                    kind = CodeTokenKind::Function;
            }
            tokens.push_back({kind, word});
            i = j;
            continue;
        }
        std::size_t j = i + 1;
        while (j < line.size())
        {
            const char next = line[j];
            if (IsIdentStart(next) || std::isdigit(static_cast<unsigned char>(next)) != 0
                || next == '"' || next == '\'' || next == '`' || next == '#'
                || (next == '/' && j + 1 < line.size()
                    && (line[j + 1] == '/' || line[j + 1] == '*')))
                break;
            ++j;
        }
        tokens.push_back({CodeTokenKind::Plain, std::string(line.substr(i, j - i))});
        i = j;
    }
    return tokens;
}

} // namespace ysDui::controls::content::detail
