/**
 * 文件名：DuiMarkdownAst.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：DuiMarkdownView 私有 AST 与布局/命中数据结构（不进入公开头）。
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "DuiMarkdownHighlight.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"

namespace ysDui::controls::content::detail {

/** 行内节点类型。 */
enum class InlineKind : std::uint8_t {
    Text,     // 普通文本
    SoftBreak, // 软换行（空格）
    HardBreak, // 硬换行
    Code,      // 行内代码
    Link,      // 超链接（子节点为可见文本）
    Image,     // 图片占位（仅 alt）
    Math,      // LaTeX 公式（$...$ / $$...$$，text 为源码）
};

/** 行内格式标志。 */
enum class InlineFlags : std::uint8_t {
    None = 0,
    Strong = 1 << 0,
    Emphasis = 1 << 1,
    Strike = 1 << 2,
};

inline InlineFlags operator|(InlineFlags left, InlineFlags right)
{
    return static_cast<InlineFlags>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

inline InlineFlags& operator|=(InlineFlags& left, InlineFlags right)
{
    left = left | right;
    return left;
}

inline bool HasFlag(InlineFlags flags, InlineFlags bit)
{
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(bit)) != 0;
}

/** 行内节点。 */
struct InlineNode final {
    InlineKind kind{InlineKind::Text};
    InlineFlags flags{InlineFlags::None};
    std::string text;
    std::string href;
    bool mathDisplay{}; // Math：是否为 $$ 块级公式
    std::vector<InlineNode> children;
};

/** 块类型。 */
enum class BlockKind : std::uint8_t {
    Paragraph,
    Heading,
    BulletList,
    OrderedList,
    ListItem,
    CodeFence,
    BlockQuote,
    ThematicBreak,
    Table,
    TableRow,
    TableCell,
};

/** 块节点。 */
struct BlockNode final {
    BlockKind kind{BlockKind::Paragraph};
    int headingLevel{1};
    int listStart{1};
    bool ordered{};
    bool tight{};
    bool headerCell{};
    bool taskItem{};   // GFM 任务列表项
    bool taskChecked{}; // 任务项是否勾选
    /** 表格单元格水平对齐（来自 `| :--- | :---: | ---: |`）。 */
    render::DuiTextAlignment cellAlign{render::DuiTextAlignment::Start};
    std::string language;
    std::string code;
    std::vector<InlineNode> inlines;
    std::vector<BlockNode> children;
};

/** 解析后的文档。 */
struct MarkdownDocument final {
    std::vector<BlockNode> blocks;
    /** 与 blocks 一一对应的顶层块指纹（Parse 后填充，供流式前缀复用）。 */
    std::vector<std::uint64_t> blockFingerprints;
};

/** 已布局的文本片段（可绘制、可命中）。 */
struct LaidRun final {
    core::Rect bounds;
    std::string text;
    render::DuiTextStyle style;
    std::string linkHref;
    bool strike{};       // 删除线（~~text~~）：Paint 时画中线
    bool isCodeChrome{};
    bool isCopyButton{};
    bool selectable{true}; // 是否计入划选纯文本
    render::DuiTextAlignment alignment{render::DuiTextAlignment::Start}; // 绘制对齐（复制按钮居中）
    std::size_t plainBegin{}; // 在 plainText 中的起止字节偏移
    std::size_t plainEnd{};
    std::string copyCode;
    std::string copyLanguage;
};

/** 已布局的图片（有解码图时绘制；否则仍用占位 run）。 */
struct LaidImage final {
    core::Rect bounds;
    std::shared_ptr<const render::DuiImage> image;
    std::string src;
    std::string alt;
};

/** 已布局的装饰矩形（代码背景、表格线、光标等）。 */
struct LaidDecor final {
    core::Rect bounds;
    core::Color fill{};
    core::Color stroke{};
    float strokeWidth{};
    int radius{};       // 圆角半径；0 表示直角
    bool fillOnly{true};
    bool strokeOnly{};  // 仅描边（无填充）
};

/**
 * 顶层块在布局结果中的切片（Y 区间 + runs/decors/images/plain 下标）。
 * 供绘制/命中跳过视口外块，以及流式追加时复用稳定前缀。
 */
struct LaidBand final {
    int y0{};
    int y1{};
    std::size_t runBegin{};
    std::size_t runEnd{};
    std::size_t decorBegin{};
    std::size_t decorEnd{};
    std::size_t imageBegin{};
    std::size_t imageEnd{};
    std::size_t plainBegin{};
    std::size_t plainEnd{};
};

/** 布局结果。 */
struct MarkdownLayout final {
    core::Size size{};
    std::vector<LaidDecor> decors;
    std::vector<LaidRun> runs;
    std::vector<LaidImage> images;
    std::vector<LaidBand> bands; // 与 document.blocks 顶层一一对应
    std::string plainText; // 可见纯文本（供划选复制）
    int layoutWidth{};     // 布局时传入的视口宽度
    int contentWidth{};    // 实际内容宽度（宽表可大于 layoutWidth）
    core::Color linkHoverColor{0, 90, 200, 255}; // 与 MarkdownPalette::linkHover 同步
};

/**
 * 流式布局：复用上一帧前 N 个顶层块的布局切片。
 * @note `source` 非空时会被截断并 move 进新布局（调用后内容失效），避免深拷贝峰值。
 */
struct LayoutReusePrefix final {
    MarkdownLayout* source{};
    std::size_t blockCount{};
};

/** 图片 src → 已解码图像；返回空则画占位。 */
using MarkdownImageResolver =
    std::function<std::shared_ptr<const render::DuiImage>(std::string_view src)>;

/** 解析选项。 */
struct ParseOptions final {
    bool sanitize{true};
    bool enableTable{true};
    bool streaming{};
};

/** 解析 Markdown；失败返回空文档并写入 errorMessage。 */
[[nodiscard]] MarkdownDocument ParseMarkdown(std::string_view source, const ParseOptions& options,
                                             std::string* errorMessage);

/** 流式场景：为未闭合围栏预补全闭合符。 */
[[nodiscard]] std::string PrepareStreamingSource(std::string_view source);

/**
 * 轻量 LaTeX 公式布局（常用子集：分数/根号/上下标/希腊字母/运算符）。
 * @param latex 不含 $ 定界符的公式源。
 * @param display true=块级居中；false=行内。
 * @return 相对 (0,0) 的 runs/decors 与包围盒尺寸。
 */
struct MathFragment final {
    core::Size size{};
    int baseline{}; // 相对 top 的基线（用于行内对齐）
    std::vector<LaidDecor> decors;
    std::vector<LaidRun> runs;
};

[[nodiscard]] MathFragment LayoutMathFragment(std::string_view latex,
                                              render::DuiTextMeasurer& measurer,
                                              const render::DuiTextStyle& baseStyle, bool display);

/** 顶层块指纹（流式前缀复用比较用）。 */
[[nodiscard]] std::uint64_t FingerprintBlock(const BlockNode& block);

/** 按当前 blocks 重算并写入 `document.blockFingerprints`。 */
void RefreshBlockFingerprints(MarkdownDocument& document);

/**
 * 与 previousFingerprints 比较，返回可复用的顶层块前缀长度。
 * 优先使用 `document.blockFingerprints`（缺省时现场计算）。
 */
[[nodiscard]] std::size_t CountReusablePrefixBlocks(
    const MarkdownDocument& document, const std::vector<std::uint64_t>& previousFingerprints);

/** 使用文本测量器布局文档。 */
[[nodiscard]] MarkdownLayout LayoutMarkdown(const MarkdownDocument& document,
                                            render::DuiTextMeasurer& measurer, int width,
                                            const render::DuiTextStyle& baseStyle, bool codeHighlight,
                                            bool codeCopyable, const core::DuiTheme& theme,
                                            bool streamingCursor, const MarkdownPalette& palette,
                                            const MarkdownImageResolver& imageResolver = {},
                                            const LayoutReusePrefix* reusePrefix = nullptr);

/** 在布局结果中命中测试。 */
struct HitResult final {
    bool hit{};
    std::string linkHref;
    bool copyButton{};
    std::string copyCode;
    std::string copyLanguage;
    bool onText{}; // 命中可选文本（用于划选）
};

[[nodiscard]] HitResult HitTestMarkdown(const MarkdownLayout& layout, core::Point point);

/**
 * 将局部坐标映射到 plainText 字节偏移（用于划选）。
 * @param measurer 用于 run 内按字形测宽；可为空则按宽度比例估算。
 */
[[nodiscard]] std::size_t PlainIndexAt(const MarkdownLayout& layout, core::Point point,
                                       render::DuiTextMeasurer* measurer);

/**
 * 以 plainText 字节偏移为中心取词边界（字母数字/下划线/多字节字符）。
 * @return [begin, end) 字节区间；无词时 begin==end。
 */
[[nodiscard]] std::pair<std::size_t, std::size_t> WordRangeAt(const MarkdownLayout& layout,
                                                             std::size_t plainIndex);

/** 绘制选项。 */
struct MarkdownPaintOptions final {
    std::string_view hoveredLink{};
    std::size_t selectionBegin{};
    std::size_t selectionEnd{};
    core::Color selectionColor{217, 232, 252, 180};
    /** 将布局坐标平移到屏幕：screen = layout + origin（避免整表拷贝布局）。 */
    core::Point origin{};
};

/**
 * 绘制已布局文档（零拷贝：按 origin 平移，仅绘制与 dirty 相交的片段）。
 * @param options 悬停链接、选区高亮与原点偏移。
 */
void PaintMarkdown(render::Canvas& canvas, const MarkdownLayout& layout, core::Rect dirty,
                   const MarkdownPaintOptions& options = {});

} // namespace ysDui::controls::content::detail
