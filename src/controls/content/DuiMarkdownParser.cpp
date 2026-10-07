/**
 * 文件名：DuiMarkdownParser.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：基于 MD4C 将 Markdown 解析为私有 AST，并处理消毒与流式围栏预补全。
 */
#include "DuiMarkdownAst.hpp"

#include <cctype>
#include <cstring>
#include <stack>

#include "md4c.h"

namespace ysDui::controls::content::detail {
namespace {

struct ParseContext final {
    ParseOptions options;
    MarkdownDocument document;
    std::stack<BlockNode*> blockStack;
    std::stack<InlineNode*> inlineStack;
    InlineFlags activeFlags{InlineFlags::None};
    std::string* errorMessage{};
    bool abort{};

    BlockNode* CurrentBlock()
    {
        return blockStack.empty() ? nullptr : blockStack.top();
    }

    InlineNode* CurrentInline()
    {
        return inlineStack.empty() ? nullptr : inlineStack.top();
    }

    InlineNode* AppendInline(InlineNode node)
    {
        if (InlineNode* parent = CurrentInline(); parent != nullptr)
        {
            parent->children.push_back(std::move(node));
            return &parent->children.back();
        }
        if (BlockNode* block = CurrentBlock(); block != nullptr)
        {
            block->inlines.push_back(std::move(node));
            return &block->inlines.back();
        }
        return nullptr;
    }
};

[[nodiscard]] std::string AttributeText(const MD_ATTRIBUTE& attribute)
{
    if (attribute.text == nullptr || attribute.size == 0)
        return {};
    return std::string(attribute.text, attribute.text + attribute.size);
}

[[nodiscard]] bool IsRelativeOrSafe(std::string_view href, bool sanitize)
{
    if (!sanitize)
        return true;
    if (href.empty())
        return false;
    std::string lower;
    lower.reserve(href.size());
    for (const char ch : href)
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    if (lower.rfind("javascript:", 0) == 0 || lower.rfind("data:", 0) == 0
        || lower.rfind("vbscript:", 0) == 0)
        return false;
    const auto schemeEnd = lower.find(':');
    if (schemeEnd == std::string::npos)
        return true; // 相对路径 / 锚点
    const std::string scheme = lower.substr(0, schemeEnd);
    return scheme == "http" || scheme == "https" || scheme == "mailto";
}

int EnterBlock(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    auto* ctx = static_cast<ParseContext*>(userdata);
    if (ctx->abort)
        return -1;

    BlockNode node;
    switch (type)
    {
    case MD_BLOCK_DOC:
        return 0;
    case MD_BLOCK_P:
        node.kind = BlockKind::Paragraph;
        break;
    case MD_BLOCK_H:
        node.kind = BlockKind::Heading;
        if (detail != nullptr)
            node.headingLevel = static_cast<int>(static_cast<MD_BLOCK_H_DETAIL*>(detail)->level);
        break;
    case MD_BLOCK_UL:
        node.kind = BlockKind::BulletList;
        if (detail != nullptr)
            node.tight = static_cast<MD_BLOCK_UL_DETAIL*>(detail)->is_tight != 0;
        break;
    case MD_BLOCK_OL:
        node.kind = BlockKind::OrderedList;
        node.ordered = true;
        if (detail != nullptr)
        {
            const auto* ol = static_cast<MD_BLOCK_OL_DETAIL*>(detail);
            node.listStart = static_cast<int>(ol->start);
            node.tight = ol->is_tight != 0;
        }
        break;
    case MD_BLOCK_LI:
        node.kind = BlockKind::ListItem;
        if (detail != nullptr)
        {
            const auto* li = static_cast<MD_BLOCK_LI_DETAIL*>(detail);
            node.taskItem = li->is_task != 0;
            node.taskChecked = li->is_task != 0
                && (li->task_mark == 'x' || li->task_mark == 'X');
        }
        break;
    case MD_BLOCK_CODE:
        node.kind = BlockKind::CodeFence;
        if (detail != nullptr)
            node.language = AttributeText(static_cast<MD_BLOCK_CODE_DETAIL*>(detail)->lang);
        break;
    case MD_BLOCK_QUOTE:
        node.kind = BlockKind::BlockQuote;
        break;
    case MD_BLOCK_HR:
        node.kind = BlockKind::ThematicBreak;
        break;
    case MD_BLOCK_TABLE:
        if (!ctx->options.enableTable)
            return 0;
        node.kind = BlockKind::Table;
        break;
    case MD_BLOCK_THEAD:
    case MD_BLOCK_TBODY:
        return 0;
    case MD_BLOCK_TR:
        if (!ctx->options.enableTable)
            return 0;
        node.kind = BlockKind::TableRow;
        break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        if (!ctx->options.enableTable)
            return 0;
        node.kind = BlockKind::TableCell;
        node.headerCell = type == MD_BLOCK_TH;
        if (detail != nullptr)
        {
            switch (static_cast<const MD_BLOCK_TD_DETAIL*>(detail)->align)
            {
            case MD_ALIGN_CENTER:
                node.cellAlign = render::DuiTextAlignment::Center;
                break;
            case MD_ALIGN_RIGHT:
                node.cellAlign = render::DuiTextAlignment::End;
                break;
            case MD_ALIGN_LEFT:
            case MD_ALIGN_DEFAULT:
            default:
                node.cellAlign = render::DuiTextAlignment::Start;
                break;
            }
        }
        break;
    case MD_BLOCK_HTML:
        // sanitize 时忽略 HTML 块
        if (ctx->options.sanitize)
            return 0;
        node.kind = BlockKind::Paragraph;
        break;
    default:
        return 0;
    }

    if (BlockNode* parent = ctx->CurrentBlock(); parent != nullptr)
    {
        parent->children.push_back(std::move(node));
        ctx->blockStack.push(&parent->children.back());
    }
    else
    {
        ctx->document.blocks.push_back(std::move(node));
        ctx->blockStack.push(&ctx->document.blocks.back());
    }
    return 0;
}

int LeaveBlock(MD_BLOCKTYPE type, void*, void* userdata)
{
    auto* ctx = static_cast<ParseContext*>(userdata);
    switch (type)
    {
    case MD_BLOCK_DOC:
    case MD_BLOCK_THEAD:
    case MD_BLOCK_TBODY:
        return 0;
    case MD_BLOCK_TABLE:
    case MD_BLOCK_TR:
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        if (!ctx->options.enableTable)
            return 0;
        break;
    case MD_BLOCK_HTML:
        if (ctx->options.sanitize)
            return 0;
        break;
    default:
        break;
    }
    if (!ctx->blockStack.empty())
        ctx->blockStack.pop();
    return 0;
}

int EnterSpan(MD_SPANTYPE type, void* detail, void* userdata)
{
    auto* ctx = static_cast<ParseContext*>(userdata);
    switch (type)
    {
    case MD_SPAN_EM:
        ctx->activeFlags |= InlineFlags::Emphasis;
        return 0;
    case MD_SPAN_STRONG:
        ctx->activeFlags |= InlineFlags::Strong;
        return 0;
    case MD_SPAN_DEL:
        ctx->activeFlags |= InlineFlags::Strike;
        return 0;
    case MD_SPAN_CODE:
    {
        InlineNode node;
        node.kind = InlineKind::Code;
        node.flags = ctx->activeFlags;
        if (InlineNode* created = ctx->AppendInline(std::move(node)); created != nullptr)
            ctx->inlineStack.push(created);
        return 0;
    }
    case MD_SPAN_A:
    {
        InlineNode node;
        node.kind = InlineKind::Link;
        node.flags = ctx->activeFlags;
        if (detail != nullptr)
            node.href = AttributeText(static_cast<MD_SPAN_A_DETAIL*>(detail)->href);
        if (!IsRelativeOrSafe(node.href, ctx->options.sanitize))
            node.href.clear();
        if (InlineNode* created = ctx->AppendInline(std::move(node)); created != nullptr)
            ctx->inlineStack.push(created);
        return 0;
    }
    case MD_SPAN_IMG:
    {
        InlineNode node;
        node.kind = InlineKind::Image;
        node.flags = ctx->activeFlags;
        if (detail != nullptr)
        {
            // src 存 href，alt 由后续 OnText 写入 text
            node.href = AttributeText(static_cast<MD_SPAN_IMG_DETAIL*>(detail)->src);
            if (!IsRelativeOrSafe(node.href, ctx->options.sanitize))
                node.href.clear();
        }
        if (InlineNode* created = ctx->AppendInline(std::move(node)); created != nullptr)
            ctx->inlineStack.push(created);
        return 0;
    }
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
    {
        InlineNode node;
        node.kind = InlineKind::Math;
        node.flags = ctx->activeFlags;
        node.mathDisplay = type == MD_SPAN_LATEXMATH_DISPLAY;
        if (InlineNode* created = ctx->AppendInline(std::move(node)); created != nullptr)
            ctx->inlineStack.push(created);
        return 0;
    }
    default:
        return 0;
    }
}

int LeaveSpan(MD_SPANTYPE type, void*, void* userdata)
{
    auto* ctx = static_cast<ParseContext*>(userdata);
    switch (type)
    {
    case MD_SPAN_EM:
        ctx->activeFlags = static_cast<InlineFlags>(static_cast<std::uint8_t>(ctx->activeFlags)
            & ~static_cast<std::uint8_t>(InlineFlags::Emphasis));
        return 0;
    case MD_SPAN_STRONG:
        ctx->activeFlags = static_cast<InlineFlags>(static_cast<std::uint8_t>(ctx->activeFlags)
            & ~static_cast<std::uint8_t>(InlineFlags::Strong));
        return 0;
    case MD_SPAN_DEL:
        ctx->activeFlags = static_cast<InlineFlags>(static_cast<std::uint8_t>(ctx->activeFlags)
            & ~static_cast<std::uint8_t>(InlineFlags::Strike));
        return 0;
    case MD_SPAN_CODE:
    case MD_SPAN_A:
    case MD_SPAN_IMG:
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
        if (!ctx->inlineStack.empty())
            ctx->inlineStack.pop();
        return 0;
    default:
        return 0;
    }
}

int OnText(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    auto* ctx = static_cast<ParseContext*>(userdata);
    if (ctx->abort)
        return -1;

    BlockNode* block = ctx->CurrentBlock();
    if (block != nullptr && block->kind == BlockKind::CodeFence)
    {
        if (type == MD_TEXT_CODE || type == MD_TEXT_NORMAL)
            block->code.append(text, text + size);
        return 0;
    }

    if (type == MD_TEXT_HTML && ctx->options.sanitize)
        return 0;

    InlineNode node;
    node.flags = ctx->activeFlags;
    switch (type)
    {
    case MD_TEXT_NULLCHAR:
        node.kind = InlineKind::Text;
        node.text = "\xEF\xBF\xBD";
        break;
    case MD_TEXT_BR:
        node.kind = InlineKind::HardBreak;
        break;
    case MD_TEXT_SOFTBR:
        node.kind = InlineKind::SoftBreak;
        node.text = " ";
        break;
    case MD_TEXT_CODE:
        node.kind = InlineKind::Code;
        node.text.assign(text, text + size);
        break;
    case MD_TEXT_ENTITY:
        // 简化：保留实体原文，避免引入完整实体表
        node.kind = InlineKind::Text;
        node.text.assign(text, text + size);
        break;
    case MD_TEXT_HTML:
        node.kind = InlineKind::Text;
        node.text.assign(text, text + size);
        break;
    default:
        node.kind = InlineKind::Text;
        node.text.assign(text, text + size);
        break;
    }

    if (type == MD_TEXT_LATEXMATH)
    {
        if (InlineNode* parent = ctx->CurrentInline(); parent != nullptr && parent->kind == InlineKind::Math)
        {
            parent->text.append(text, text + size);
            return 0;
        }
        InlineNode math;
        math.kind = InlineKind::Math;
        math.flags = ctx->activeFlags;
        math.text.assign(text, text + size);
        ctx->AppendInline(std::move(math));
        return 0;
    }

    if (InlineNode* parent = ctx->CurrentInline(); parent != nullptr)
    {
        if (parent->kind == InlineKind::Code || parent->kind == InlineKind::Image
            || parent->kind == InlineKind::Math)
        {
            parent->text += node.text;
            return 0;
        }
        parent->children.push_back(std::move(node));
        return 0;
    }
    if (block != nullptr)
        block->inlines.push_back(std::move(node));
    return 0;
}

void OnDebug(const char* msg, void* userdata)
{
    auto* ctx = static_cast<ParseContext*>(userdata);
    if (ctx->errorMessage != nullptr && msg != nullptr)
        *ctx->errorMessage = msg;
}

} // namespace

std::string PrepareStreamingSource(std::string_view source)
{
    std::string text(source);
    int fenceCount = 0;
    char fenceChar = '`';
    std::size_t index = 0;
    while (index < text.size())
    {
        const std::size_t lineStart = index;
        std::size_t lineEnd = text.find('\n', index);
        if (lineEnd == std::string::npos)
            lineEnd = text.size();
        std::string_view line(text.data() + lineStart, lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        std::size_t cursor = 0;
        while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t'))
            ++cursor;
        if (cursor < line.size() && (line[cursor] == '`' || line[cursor] == '~'))
        {
            const char mark = line[cursor];
            std::size_t run = cursor;
            while (run < line.size() && line[run] == mark)
                ++run;
            const std::size_t fenceLen = run - cursor;
            if (fenceLen >= 3)
            {
                if (fenceCount == 0)
                {
                    fenceChar = mark;
                    ++fenceCount;
                }
                else if (mark == fenceChar && fenceLen >= 3)
                {
                    --fenceCount;
                }
            }
        }
        index = lineEnd < text.size() ? lineEnd + 1 : text.size();
    }
    if (fenceCount > 0)
    {
        if (!text.empty() && text.back() != '\n')
            text.push_back('\n');
        text.append(3, fenceChar);
        text.push_back('\n');
    }
    return text;
}

MarkdownDocument ParseMarkdown(std::string_view source, const ParseOptions& options,
                               std::string* errorMessage)
{
    ParseContext ctx;
    ctx.options = options;
    ctx.errorMessage = errorMessage;

    const std::string prepared = options.streaming ? PrepareStreamingSource(source) : std::string(source);

    MD_PARSER parser{};
    parser.abi_version = 0;
    unsigned flags = MD_DIALECT_GITHUB | MD_FLAG_NOHTML | MD_FLAG_LATEXMATHSPANS;
    if (!options.enableTable)
        flags &= ~static_cast<unsigned>(MD_FLAG_TABLES);
    if (options.sanitize)
        flags |= MD_FLAG_NOHTML;
    parser.flags = flags;
    parser.enter_block = &EnterBlock;
    parser.leave_block = &LeaveBlock;
    parser.enter_span = &EnterSpan;
    parser.leave_span = &LeaveSpan;
    parser.text = &OnText;
    parser.debug_log = &OnDebug;
    parser.syntax = nullptr;

    const int result = md_parse(prepared.data(), static_cast<MD_SIZE>(prepared.size()), &parser, &ctx);
    if (result != 0)
    {
        if (errorMessage != nullptr && errorMessage->empty())
            *errorMessage = "md_parse failed";
        return {};
    }
    RefreshBlockFingerprints(ctx.document);
    return std::move(ctx.document);
}

} // namespace ysDui::controls::content::detail
