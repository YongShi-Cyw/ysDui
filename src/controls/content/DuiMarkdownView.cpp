/**
 * 文件名：DuiMarkdownView.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：DuiMarkdownView 控件实现（属性、节流解析、布局与交互入口）。
 */
#include "ysDui/controls/content/DuiMarkdownView.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "DuiMarkdownAst.hpp"
#include "DuiMarkdownHighlight.hpp"
#include "DuiMarkdownImagePipeline.hpp"
#include "DuiMarkdownViewState.hpp"
#include "ysDui/controls/layout/DuiScrollView.hpp"
#include "ysDui/controls/media/DuiAsyncImageLoader.hpp"
#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::content {
namespace {

constexpr int kA11yMaxHeadings = 32; // 无障碍大纲最多列出的标题数
constexpr int kA11yMaxLinks = 24;    // 无障碍大纲最多列出的链接数

void AppendInlinePlain(const detail::InlineNode& node, std::string& out)
{
    switch (node.kind)
    {
    case detail::InlineKind::Text:
    case detail::InlineKind::Code:
    case detail::InlineKind::Math:
        out += node.text;
        break;
    case detail::InlineKind::SoftBreak:
    case detail::InlineKind::HardBreak:
        out.push_back(' ');
        break;
    case detail::InlineKind::Image:
        out += node.text.empty() ? "[image]" : node.text;
        break;
    case detail::InlineKind::Link:
        if (node.children.empty())
            out += node.text.empty() ? node.href : node.text;
        else
            for (const detail::InlineNode& child : node.children)
                AppendInlinePlain(child, out);
        break;
    }
}

[[nodiscard]] std::vector<DuiMarkdownOutlineEntry> CollectOutlineEntries(
    const detail::MarkdownDocument& document, const detail::MarkdownLayout& layout)
{
    std::vector<DuiMarkdownOutlineEntry> entries;

    std::function<void(const detail::BlockNode&, int)> walk;
    walk = [&](const detail::BlockNode& block, int bandY)
    {
        if (block.kind == detail::BlockKind::Heading)
        {
            std::string text;
            for (const detail::InlineNode& inlineNode : block.inlines)
                AppendInlinePlain(inlineNode, text);
            if (!text.empty())
            {
                DuiMarkdownOutlineEntry entry;
                entry.level = (std::clamp)(block.headingLevel, 1, 6);
                entry.text = std::move(text);
                entry.y = bandY;
                entries.push_back(std::move(entry));
            }
        }
        for (const detail::BlockNode& child : block.children)
            walk(child, bandY);
    };

    for (std::size_t index = 0; index < document.blocks.size(); ++index)
    {
        const int bandY =
            index < layout.bands.size() ? layout.bands[index].y0 : layout.size.height;
        walk(document.blocks[index], bandY);
    }
    return entries;
}

struct A11yOutline final {
    std::string firstHeading;
    std::string description;
};

[[nodiscard]] A11yOutline BuildA11yOutline(const detail::MarkdownDocument& document,
                                           const detail::MarkdownLayout& layout)
{
    A11yOutline outline;
    const auto headings = CollectOutlineEntries(document, layout);
    std::string headingText;
    int headingCount = 0;
    for (const DuiMarkdownOutlineEntry& entry : headings)
    {
        if (headingCount >= kA11yMaxHeadings)
            break;
        if (outline.firstHeading.empty())
            outline.firstHeading = entry.text;
        headingText += 'H';
        headingText += static_cast<char>('0' + entry.level);
        headingText += ' ';
        headingText += entry.text;
        headingText.push_back('\n');
        ++headingCount;
    }

    std::string links;
    int linkCount = 0;
    std::function<void(const detail::InlineNode&)> walkInline;
    std::function<void(const detail::BlockNode&)> walkBlock;
    walkInline = [&](const detail::InlineNode& node)
    {
        if (node.kind == detail::InlineKind::Link && linkCount < kA11yMaxLinks)
        {
            std::string label;
            AppendInlinePlain(node, label);
            if (label.empty())
                label = node.href;
            if (!label.empty() || !node.href.empty())
            {
                links += "链接: ";
                links += label;
                if (!node.href.empty())
                {
                    links += " -> ";
                    links += node.href;
                }
                links.push_back('\n');
                ++linkCount;
            }
        }
        for (const detail::InlineNode& child : node.children)
            walkInline(child);
    };
    walkBlock = [&](const detail::BlockNode& block)
    {
        for (const detail::InlineNode& inlineNode : block.inlines)
            walkInline(inlineNode);
        for (const detail::BlockNode& child : block.children)
            walkBlock(child);
    };
    for (const detail::BlockNode& block : document.blocks)
        walkBlock(block);

    outline.description = "只读 Markdown 视图";
    if (!headingText.empty())
    {
        outline.description += "\n标题大纲:\n";
        outline.description += headingText;
    }
    if (!links.empty())
    {
        outline.description += "链接:\n";
        outline.description += links;
    }
    return outline;
}

/**
 * 无 Canvas 时的回退测量（仅估尺寸，禁止用于最终绘制）。
 * ASCII 约 0.55em，CJK/其它约 1em，避免把中文估得过窄。
 */
class FallbackMeasurer final : public render::DuiTextMeasurer
{
public:
    [[nodiscard]] render::DuiTextMetrics MeasureText(
        std::string_view text, const render::DuiTextStyle& style,
        const render::DuiTextMeasureOptions& options) override
    {
        const int em = (std::max)(8, style.pointSize);
        int width = 0;
        for (std::size_t index{}; index < text.size();)
        {
            const unsigned char first = static_cast<unsigned char>(text[index]);
            const std::size_t units = first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
            index += (std::min)(units, text.size() - index);
            width += units == 1 ? (std::max)(4, em * 55 / 100) : em;
        }
        int height = (std::max)(10, style.pointSize + 6);
        int lineCount = 1;
        if (options.wordWrap && options.maximumWidth > 0 && width > options.maximumWidth)
        {
            lineCount = (width + options.maximumWidth - 1) / options.maximumWidth;
            width = options.maximumWidth;
            height *= lineCount;
        }
        return {{width, height}, lineCount, height};
    }
};

/** 布局精度：回退测量不得覆盖已有的 Canvas 精确布局。 */
enum class LayoutPrecision : std::uint8_t {
    None,         // 尚未布局
    Approximate,  // FallbackMeasurer
    Precise,      // 真实 Canvas / DirectWrite
};

} // namespace

class DuiMarkdownView::Impl
{
public:
    std::string content;
    detail::MarkdownDocument document;
    mutable detail::MarkdownLayout layout;
    bool streaming{};
    bool streamingCursor{true};
    int streamThrottleMs{50};
    bool sanitize{true};
    DuiMarkdownAppearance appearance{DuiMarkdownAppearance::Auto};
    bool codeHighlight{true};
    bool codeCopyable{true};
    bool enableTable{true};
    bool selectable{true};
    DuiMarkdownLinkTarget linkTarget{DuiMarkdownLinkTarget::External};
    ui::DuiClipboard* clipboard{};
    MarkdownImagePipeline images;
    core::AnimationClock* clock{};
    core::AnimationClock::TaskId throttleTask{};
    bool throttleTaskActive{};
    bool parsePending{};
    std::chrono::steady_clock::time_point lastParseTime{};
    std::function<void(std::string_view, std::string_view)> codeCopyHandler;
    std::function<void(std::string_view)> linkHandler;
    std::function<void()> renderedHandler;
    std::function<void(std::string_view)> errorHandler;
    std::function<void(int)> scrollRequestHandler; // 自定义纵向滚动；空则找祖先 ScrollView
    mutable int lastLayoutWidth{};
    mutable bool documentDirty{true};
    mutable LayoutPrecision layoutPrecision{LayoutPrecision::None};
    mutable std::vector<std::uint64_t> previousBlockFingerprints; // 上一帧布局时的块指纹
    std::string hoveredLink;
    SelectionState selection;
    HScrollState hScroll;
    core::Point pressLocal{};
    bool pressOnCopy{};
    std::string pressLink;
    DuiMarkdownView* owner{};

    void CancelThrottle()
    {
        if (clock != nullptr && throttleTaskActive)
        {
            clock->Cancel(throttleTask);
            throttleTaskActive = false;
        }
    }

    void ClearAsyncImageState() { images.Clear(); }

    bool PollAsyncImages() { return images.Poll(); }

    void SyncImageClock()
    {
        images.clock = clock;
        if (!images.asyncPending.empty())
            images.SchedulePoll();
    }

    void ParseNow()
    {
        CancelThrottle();
        parsePending = false;
        documentDirty = true;
        lastParseTime = std::chrono::steady_clock::now();

        detail::ParseOptions options;
        options.sanitize = sanitize;
        options.enableTable = enableTable;
        options.streaming = streaming;

        std::string error;
        document = detail::ParseMarkdown(content, options, &error);
        if (!error.empty() && errorHandler)
            errorHandler(error);
    }

    void ScheduleOrParse()
    {
        if (!streaming || streamThrottleMs <= 0)
        {
            ParseNow();
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastParseTime).count();
        if (elapsed >= streamThrottleMs || lastParseTime.time_since_epoch().count() == 0)
        {
            ParseNow();
            return;
        }

        parsePending = true;
        if (clock == nullptr || throttleTaskActive)
            return;

        const int delay = streamThrottleMs - static_cast<int>(elapsed);
        throttleTask = clock->Schedule((std::max)(1, delay), [this](double)
        {
            throttleTaskActive = false;
            if (parsePending)
                ParseNow();
        });
        throttleTaskActive = true;
    }

    void EnsureParsed() const
    {
        if (parsePending)
            const_cast<Impl*>(this)->ParseNow();
    }

    /**
     * @param precise true=Canvas 精确测量（绘制用）；false=回退估宽（Layout/命中占位）。
     * 已有同宽精确布局时，回退路径不得降级覆盖，否则绘制字宽与 run 矩形不一致会裁切文字。
     */
    void EnsureLayout(render::DuiTextMeasurer& measurer, int width, const core::DuiTheme& theme,
                      bool precise) const
    {
        EnsureParsed();
        width = (std::max)(40, width);
        if (!documentDirty && lastLayoutWidth == width)
        {
            if (layoutPrecision == LayoutPrecision::Precise)
                return; // 精确布局已就绪：回退与再次精确均可直接复用
            if (!precise && layoutPrecision == LayoutPrecision::Approximate)
                return;
        }

        render::DuiTextStyle base;
        base.pointSize = 14;
        base.family = "Segoe UI, Microsoft YaHei UI, Arial";
        bool darkSurfaces = true;
        switch (appearance)
        {
        case DuiMarkdownAppearance::Light:
            base.color = {30, 30, 36, 255};
            darkSurfaces = false;
            break;
        case DuiMarkdownAppearance::Dark:
            base.color = {230, 230, 235, 255};
            darkSurfaces = true;
            break;
        case DuiMarkdownAppearance::Auto:
        default:
            base.color = theme.Get(core::ThemeSlot::TextDefault);
            // 按表面背景亮度判断代码块/引用用浅色还是深色色板
            {
                const core::Color bg = theme.Get(core::ThemeSlot::SurfaceBackground);
                const int luminance =
                    (bg.red * 299 + bg.green * 587 + bg.blue * 114) / 1000;
                darkSurfaces = luminance < 140;
            }
            break;
        }

        const detail::MarkdownPalette palette = detail::MarkdownPalette::Make(darkSurfaces);
        detail::MarkdownImageResolver resolver =
            [this](std::string_view src) -> std::shared_ptr<const render::DuiImage>
        {
            return images.Resolve(src);
        };

        detail::LayoutReusePrefix reuse{};
        detail::MarkdownLayout previousLayout;
        const detail::LayoutReusePrefix* reusePtr = nullptr;
        // 流式追加且同宽：复用指纹未变的顶层块前缀，只重排后续块（move，避免深拷贝峰值）
        if (streaming && lastLayoutWidth == width && layoutPrecision == LayoutPrecision::Precise
            && !layout.bands.empty() && !previousBlockFingerprints.empty())
        {
            const std::size_t reusable =
                detail::CountReusablePrefixBlocks(document, previousBlockFingerprints);
            if (reusable > 0)
            {
                previousLayout = std::move(layout);
                reuse.source = &previousLayout;
                reuse.blockCount = reusable;
                reusePtr = &reuse;
            }
        }

        layout = detail::LayoutMarkdown(document, measurer, width, base, codeHighlight, codeCopyable,
                                        theme, streaming && streamingCursor, palette, resolver,
                                        reusePtr);
        // 解析阶段已写入 document.blockFingerprints，此处只缓存供下一帧比较
        previousBlockFingerprints = document.blockFingerprints;

        lastLayoutWidth = width;
        documentDirty = false;
        layoutPrecision = precise ? LayoutPrecision::Precise : LayoutPrecision::Approximate;
        selection.ClampTo(layout.plainText.size());
        hScroll.Clamp(layout.size.width, width);
        if (renderedHandler)
            renderedHandler();
    }

    [[nodiscard]] bool HasSelection() const { return selection.Has(); }

    [[nodiscard]] std::string SelectedPlain() const { return selection.Plain(layout.plainText); }

    void ClampScroll(int viewportWidth) { hScroll.Clamp(layout.size.width, viewportWidth); }

    [[nodiscard]] int MaxScrollX(int viewportWidth) const
    {
        return hScroll.MaxScroll(layout.size.width, viewportWidth);
    }

    [[nodiscard]] bool NeedsHScroll(int viewportWidth) const
    {
        return hScroll.Needed(layout.size.width, viewportWidth);
    }

    [[nodiscard]] core::Rect HScrollTrackLocal(int viewportWidth, int viewportHeight) const
    {
        return hScroll.TrackLocal(layout.size.width, viewportWidth, viewportHeight);
    }

    [[nodiscard]] core::Rect HScrollThumbLocal(int viewportWidth, int viewportHeight) const
    {
        return hScroll.ThumbLocal(layout.size.width, viewportWidth, viewportHeight);
    }

    void SetScrollFromThumbCenter(int localX, int viewportWidth, int viewportHeight)
    {
        hScroll.SetFromThumb(localX, layout.size.width, viewportWidth, viewportHeight);
    }
};

DuiMarkdownView::DuiMarkdownView() : markdown_(std::make_unique<Impl>())
{
    markdown_->owner = this;
    markdown_->images.onAsyncReady = [this] { PollAsyncImages(); };
}
DuiMarkdownView::~DuiMarkdownView()
{
    if (markdown_ != nullptr)
    {
        markdown_->CancelThrottle();
        markdown_->ClearAsyncImageState();
        markdown_->images.onAsyncReady = {};
        markdown_->owner = nullptr;
    }
}
DuiMarkdownView::DuiMarkdownView(DuiMarkdownView&& other) noexcept
    : Control(std::move(other)), markdown_(std::move(other.markdown_))
{
    if (markdown_ != nullptr)
    {
        markdown_->owner = this;
        markdown_->images.onAsyncReady = [this] { PollAsyncImages(); };
    }
}

DuiMarkdownView& DuiMarkdownView::operator=(DuiMarkdownView&& other) noexcept
{
    if (this == &other)
        return *this;
    if (markdown_ != nullptr)
    {
        markdown_->CancelThrottle();
        markdown_->ClearAsyncImageState();
        markdown_->images.onAsyncReady = {};
        markdown_->owner = nullptr;
    }
    Control::operator=(std::move(other));
    markdown_ = std::move(other.markdown_);
    if (markdown_ != nullptr)
    {
        markdown_->owner = this;
        markdown_->images.onAsyncReady = [this] { PollAsyncImages(); };
    }
    return *this;
}

namespace {

/** 规范化 Markdown 源：去 BOM、拒 UTF-16、剔除 NUL（避免后续测量/绘制异常）。 */
void NormalizeMarkdownSource(std::string& text)
{
    if (text.size() >= 2)
    {
        const auto b0 = static_cast<unsigned char>(text[0]);
        const auto b1 = static_cast<unsigned char>(text[1]);
        // UTF-16 LE/BE：本控件只接受 UTF-8
        if ((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF))
        {
            text.clear();
            return;
        }
    }
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == 0xEF
        && static_cast<unsigned char>(text[1]) == 0xBB
        && static_cast<unsigned char>(text[2]) == 0xBF)
    {
        text.erase(0, 3);
    }
    for (char& ch : text)
    {
        if (ch == '\0')
            ch = ' ';
    }
}

} // namespace

void DuiMarkdownView::SetContent(std::string content)
{
    NormalizeMarkdownSource(content);
    markdown_->content = std::move(content);
    markdown_->selection.Clear();
    markdown_->hScroll.offsetX = 0;
    markdown_->ClearAsyncImageState();
    markdown_->ScheduleOrParse();
    InvalidateLayout();
}

const std::string& DuiMarkdownView::Content() const { return markdown_->content; }

void DuiMarkdownView::SetStreaming(bool streaming)
{
    markdown_->streaming = streaming;
    if (!streaming)
        markdown_->ParseNow();
    else
        markdown_->ScheduleOrParse();
    InvalidateLayout();
}

bool DuiMarkdownView::Streaming() const { return markdown_->streaming; }

void DuiMarkdownView::SetStreamingCursor(bool enabled)
{
    markdown_->streamingCursor = enabled;
    markdown_->documentDirty = true;
    InvalidateLayout();
}

bool DuiMarkdownView::StreamingCursor() const { return markdown_->streamingCursor; }

void DuiMarkdownView::SetStreamThrottleMs(int milliseconds)
{
    markdown_->streamThrottleMs = milliseconds;
}

int DuiMarkdownView::StreamThrottleMs() const { return markdown_->streamThrottleMs; }

void DuiMarkdownView::SetSanitize(bool sanitize)
{
    markdown_->sanitize = sanitize;
    markdown_->ParseNow();
    InvalidateLayout();
}

bool DuiMarkdownView::Sanitize() const { return markdown_->sanitize; }

void DuiMarkdownView::SetAppearance(DuiMarkdownAppearance appearance)
{
    markdown_->appearance = appearance;
    markdown_->documentDirty = true;
    InvalidateLayout();
}

DuiMarkdownAppearance DuiMarkdownView::Appearance() const { return markdown_->appearance; }

void DuiMarkdownView::SetCodeHighlight(bool enabled)
{
    markdown_->codeHighlight = enabled;
    markdown_->documentDirty = true;
    InvalidateLayout();
}

bool DuiMarkdownView::CodeHighlight() const { return markdown_->codeHighlight; }

void DuiMarkdownView::SetCodeCopyable(bool enabled)
{
    markdown_->codeCopyable = enabled;
    markdown_->documentDirty = true;
    InvalidateLayout();
}

bool DuiMarkdownView::CodeCopyable() const { return markdown_->codeCopyable; }

void DuiMarkdownView::SetEnableTable(bool enabled)
{
    markdown_->enableTable = enabled;
    markdown_->ParseNow();
    InvalidateLayout();
}

bool DuiMarkdownView::EnableTable() const { return markdown_->enableTable; }

void DuiMarkdownView::SetSelectable(bool selectable) { markdown_->selectable = selectable; }
bool DuiMarkdownView::Selectable() const { return markdown_->selectable; }

void DuiMarkdownView::SetLinkTarget(DuiMarkdownLinkTarget target) { markdown_->linkTarget = target; }
DuiMarkdownLinkTarget DuiMarkdownView::LinkTarget() const { return markdown_->linkTarget; }

void DuiMarkdownView::SetClipboard(ui::DuiClipboard* clipboard) { markdown_->clipboard = clipboard; }

void DuiMarkdownView::SetImageProvider(ImageProvider provider)
{
    markdown_->images.provider = std::move(provider);
    markdown_->images.syncCache.clear();
    markdown_->documentDirty = true;
    InvalidateLayout();
}

void DuiMarkdownView::SetAsyncImageLoader(media::DuiAsyncImageLoader* loader)
{
    markdown_->ClearAsyncImageState();
    markdown_->images.asyncLoader = loader;
    markdown_->documentDirty = true;
    InvalidateLayout();
}

void DuiMarkdownView::SetAnimationClock(core::AnimationClock* clock)
{
    markdown_->CancelThrottle();
    markdown_->images.CancelPoll();
    markdown_->clock = clock;
    markdown_->SyncImageClock();
    if (markdown_->parsePending)
        markdown_->ScheduleOrParse();
}

bool DuiMarkdownView::PollAsyncImages()
{
    if (!markdown_->PollAsyncImages())
        return false;
    markdown_->documentDirty = true;
    markdown_->images.syncCache.clear();
    InvalidateLayout();
    return true;
}

void DuiMarkdownView::SetCodeCopyHandler(
    std::function<void(std::string_view code, std::string_view language)> handler)
{
    markdown_->codeCopyHandler = std::move(handler);
}

void DuiMarkdownView::SetLinkActivatedHandler(std::function<void(std::string_view href)> handler)
{
    markdown_->linkHandler = std::move(handler);
}

void DuiMarkdownView::SetRenderedHandler(std::function<void()> handler)
{
    markdown_->renderedHandler = std::move(handler);
}

void DuiMarkdownView::SetErrorHandler(std::function<void(std::string_view message)> handler)
{
    markdown_->errorHandler = std::move(handler);
}

std::string DuiMarkdownView::SelectedText() const
{
    FallbackMeasurer measurer;
    const int width = Bounds().Width() > 0 ? Bounds().Width() : 400;
    markdown_->EnsureLayout(measurer, width, Theme(), false);
    return markdown_->SelectedPlain();
}

void DuiMarkdownView::SelectAll()
{
    FallbackMeasurer measurer;
    const int width = Bounds().Width() > 0 ? Bounds().Width() : 400;
    markdown_->EnsureLayout(measurer, width, Theme(), false);
    markdown_->selection.anchor = 0;
    markdown_->selection.caret = markdown_->layout.plainText.size();
}

void DuiMarkdownView::ClearSelection()
{
    markdown_->selection.Clear();
}

core::Size DuiMarkdownView::ContentSize() const
{
    const int width = Bounds().Width() > 0
        ? Bounds().Width()
        : (markdown_->lastLayoutWidth > 0 ? markdown_->lastLayoutWidth : 400);
    FallbackMeasurer measurer;
    markdown_->EnsureLayout(measurer, width, Theme(), false);
    return markdown_->layout.size;
}

int DuiMarkdownView::ScrollOffsetX() const
{
    return markdown_->hScroll.offsetX;
}

core::Size DuiMarkdownView::DesiredSize() const
{
    return ContentSize();
}

void DuiMarkdownView::Layout(core::Rect bounds)
{
    core::Control::Layout(bounds);
    PollAsyncImages();
    FallbackMeasurer measurer;
    markdown_->EnsureLayout(measurer, bounds.Width(), Theme(), false);
    markdown_->ClampScroll(bounds.Width());
}

std::vector<DuiMarkdownOutlineEntry> DuiMarkdownView::GetOutline() const
{
    FallbackMeasurer measurer;
    const int width = Bounds().Width() > 0 ? Bounds().Width() : 400;
    markdown_->EnsureLayout(measurer, width, Theme(), false);
    return CollectOutlineEntries(markdown_->document, markdown_->layout);
}

void DuiMarkdownView::SetScrollRequestHandler(std::function<void(int contentY)> handler)
{
    markdown_->scrollRequestHandler = std::move(handler);
}

bool DuiMarkdownView::ScrollToY(int contentY)
{
    contentY = (std::max)(0, contentY);
    if (markdown_->scrollRequestHandler)
    {
        markdown_->scrollRequestHandler(contentY);
        return true;
    }

    for (core::Control* ancestor = Parent(); ancestor != nullptr; ancestor = ancestor->Parent())
    {
        if (auto* scroll = dynamic_cast<layout::DuiScrollView*>(ancestor))
        {
            scroll->SetScrollPosition(contentY);
            return true;
        }
    }
    return false;
}

bool DuiMarkdownView::ScrollToOutline(std::size_t index)
{
    const auto outline = GetOutline();
    if (index >= outline.size())
        return false;
    return ScrollToY(outline[index].y);
}

core::DuiAccessibilityData DuiMarkdownView::CreateAccessibilityData() const
{
    FallbackMeasurer measurer;
    const int width = Bounds().Width() > 0 ? Bounds().Width() : 400;
    markdown_->EnsureLayout(measurer, width, Theme(), false);
    std::string plain = markdown_->layout.plainText;
    if (plain.empty())
        plain = markdown_->content;
    const A11yOutline outline = BuildA11yOutline(markdown_->document, markdown_->layout);
    core::DuiAccessibilityData data;
    data.role = core::DuiAccessibilityRole::Text;
    data.name = outline.firstHeading.empty() ? "Markdown" : outline.firstHeading;
    data.value = std::move(plain);
    data.description = outline.description;
    data.keyboardFocusable = true;
    data.readOnly = true;
    data.patterns = core::DuiAccessibilityPattern::Value;
    if (markdown_->selectable)
        data.patterns = data.patterns | core::DuiAccessibilityPattern::Selection;
    return data;
}

bool DuiMarkdownView::OnEvent(const core::Event& event)
{
    if (!Enabled() || !EffectivelyVisible())
        return false;

    PollAsyncImages();
    if (markdown_->layoutPrecision != LayoutPrecision::Precise
        || markdown_->lastLayoutWidth != Bounds().Width() || markdown_->documentDirty)
    {
        FallbackMeasurer measurer;
        markdown_->EnsureLayout(measurer, Bounds().Width(), Theme(), false);
    }

    const int viewW = Bounds().Width();
    const int viewH = Bounds().Height();
    const auto toLocal = [this](core::Point screen) -> core::Point
    {
        return {screen.x - Bounds().left, screen.y - Bounds().top};
    };
    const auto toContent = [this](core::Point screen) -> core::Point
    {
        return {screen.x - Bounds().left + markdown_->hScroll.offsetX, screen.y - Bounds().top};
    };

    FallbackMeasurer measurer;

    if (event.type == core::EventType::PointerWheel && Bounds().Contains(event.position))
    {
        // Shift+滚轮：宽表横向滚动（纵向仍交给外层 ScrollView）
        if ((event.modifiers & core::modifier::Shift) != 0 && markdown_->NeedsHScroll(viewW))
        {
            markdown_->hScroll.offsetX -= event.wheelDelta;
            markdown_->ClampScroll(viewW);
            return true;
        }
        return false;
    }

    if (event.type == core::EventType::PointerMove)
    {
        const bool contains = Bounds().Contains(event.position);
        SetHovered(contains);
        const core::Point localUi = toLocal(event.position);
        if (markdown_->hScroll.panning)
        {
            markdown_->hScroll.offsetX =
                markdown_->hScroll.panStartScrollX - (event.position.x - markdown_->hScroll.panStartScreenX);
            markdown_->ClampScroll(viewW);
            SetPointerCursor(core::DuiPointerCursor::Move);
            return true;
        }
        if (markdown_->hScroll.thumbDragging)
        {
            markdown_->SetScrollFromThumbCenter(localUi.x, viewW, viewH);
            SetPointerCursor(core::DuiPointerCursor::Arrow);
            return true;
        }
        const core::Point local = toContent(event.position);
        if (markdown_->selection.dragging && markdown_->selectable)
        {
            markdown_->selection.caret = detail::PlainIndexAt(markdown_->layout, local, &measurer);
            SetPointerCursor(core::DuiPointerCursor::Arrow);
            return true;
        }
        const core::Rect thumb = markdown_->HScrollThumbLocal(viewW, viewH);
        if (!thumb.Empty() && thumb.Contains(localUi))
        {
            SetPointerCursor(core::DuiPointerCursor::Arrow);
            return false;
        }
        const detail::HitResult hit = detail::HitTestMarkdown(markdown_->layout, local);
        const bool overLink = hit.hit && !hit.linkHref.empty() && !hit.copyButton;
        SetPointerCursor((hit.hit && (hit.copyButton || overLink))
                             ? core::DuiPointerCursor::Hand
                             : core::DuiPointerCursor::Arrow);
        const std::string nextHover = overLink ? hit.linkHref : std::string{};
        if (markdown_->hoveredLink != nextHover)
            markdown_->hoveredLink = nextHover;
        return false;
    }

    if (event.type == core::EventType::PointerDoubleClick && Bounds().Contains(event.position)
        && event.button == core::PointerButton::Primary && markdown_->selectable)
    {
        SetFocused(true);
        const core::Point localUi = toLocal(event.position);
        const core::Rect track = markdown_->HScrollTrackLocal(viewW, viewH);
        if (!track.Empty() && track.Contains(localUi))
            return true;
        const core::Point local = toContent(event.position);
        const std::size_t index = detail::PlainIndexAt(markdown_->layout, local, &measurer);
        const auto range = detail::WordRangeAt(markdown_->layout, index);
        markdown_->selection.anchor = range.first;
        markdown_->selection.caret = range.second;
        markdown_->selection.dragging = false;
        return true;
    }

    if (event.type == core::EventType::PointerDown && Bounds().Contains(event.position))
    {
        SetFocused(true);
        SetCaptured(true);
        const core::Point localUi = toLocal(event.position);
        const core::Point local = toContent(event.position);
        markdown_->pressLocal = local;

        if (event.button == core::PointerButton::Middle && markdown_->NeedsHScroll(viewW))
        {
            markdown_->hScroll.panning = true;
            markdown_->hScroll.panStartScreenX = event.position.x;
            markdown_->hScroll.panStartScrollX = markdown_->hScroll.offsetX;
            markdown_->selection.dragging = false;
            markdown_->pressOnCopy = false;
            markdown_->pressLink.clear();
            SetPointerCursor(core::DuiPointerCursor::Move);
            return true;
        }

        const core::Rect track = markdown_->HScrollTrackLocal(viewW, viewH);
        if (event.button == core::PointerButton::Primary && !track.Empty() && track.Contains(localUi))
        {
            const core::Rect thumb = markdown_->HScrollThumbLocal(viewW, viewH);
            if (thumb.Contains(localUi))
            {
                markdown_->hScroll.thumbDragging = true;
                markdown_->hScroll.thumbGrabX = localUi.x - thumb.left;
            }
            else
            {
                // 点击轨道：以点击处为滑块中心翻页
                markdown_->hScroll.thumbGrabX = thumb.Width() / 2;
                markdown_->SetScrollFromThumbCenter(localUi.x, viewW, viewH);
                markdown_->hScroll.thumbDragging = true;
            }
            markdown_->selection.dragging = false;
            markdown_->pressOnCopy = false;
            markdown_->pressLink.clear();
            return true;
        }

        if (event.button != core::PointerButton::Primary)
        {
            SetCaptured(false);
            return false;
        }

        const detail::HitResult hit = detail::HitTestMarkdown(markdown_->layout, local);
        markdown_->pressOnCopy = hit.copyButton;
        markdown_->pressLink = hit.copyButton ? std::string{} : hit.linkHref;
        if (markdown_->selectable && !hit.copyButton)
        {
            markdown_->selection.dragging = true;
            markdown_->selection.anchor = markdown_->selection.caret =
                detail::PlainIndexAt(markdown_->layout, local, &measurer);
        }
        return true;
    }

    if (event.type == core::EventType::PointerCancel && Captured())
    {
        markdown_->selection.dragging = false;
        markdown_->hScroll.panning = false;
        markdown_->hScroll.thumbDragging = false;
        SetCaptured(false);
        return true;
    }

    if (event.type == core::EventType::PointerUp && Captured())
    {
        const bool contains = Bounds().Contains(event.position);
        const core::Point local = toContent(event.position);
        const bool wasPanning = markdown_->hScroll.panning;
        const bool wasScrollDrag = markdown_->hScroll.thumbDragging;
        if (markdown_->selection.dragging && markdown_->selectable)
            markdown_->selection.caret = detail::PlainIndexAt(markdown_->layout, local, &measurer);
        markdown_->selection.dragging = false;
        markdown_->hScroll.panning = false;
        markdown_->hScroll.thumbDragging = false;
        SetCaptured(false);
        SetHovered(contains);
        if (wasPanning || wasScrollDrag)
            return true;

        const bool dragged = std::abs(local.x - markdown_->pressLocal.x) > 3
            || std::abs(local.y - markdown_->pressLocal.y) > 3 || markdown_->HasSelection();

        if (contains && markdown_->pressOnCopy && !dragged)
        {
            const detail::HitResult hit = detail::HitTestMarkdown(markdown_->layout, local);
            if (hit.copyButton)
            {
                if (markdown_->clipboard != nullptr)
                    markdown_->clipboard->SetText(hit.copyCode);
                if (markdown_->codeCopyHandler)
                    markdown_->codeCopyHandler(hit.copyCode, hit.copyLanguage);
                return true;
            }
        }
        if (contains && !dragged && !markdown_->pressLink.empty() && !markdown_->HasSelection())
        {
            if (markdown_->linkHandler)
                markdown_->linkHandler(markdown_->pressLink);
            return true;
        }
        return true;
    }

    if (event.type == core::EventType::KeyDown && Focused())
    {
        if (markdown_->NeedsHScroll(viewW))
        {
            constexpr int kLineStep = 40; // 方向键单步横向滚动
            if (event.key == core::key::Left)
            {
                markdown_->hScroll.offsetX -= kLineStep;
                markdown_->ClampScroll(viewW);
                return true;
            }
            if (event.key == core::key::Right)
            {
                markdown_->hScroll.offsetX += kLineStep;
                markdown_->ClampScroll(viewW);
                return true;
            }
            if (event.key == core::key::Home && (event.modifiers & core::modifier::Control) == 0)
            {
                markdown_->hScroll.offsetX = 0;
                return true;
            }
            if (event.key == core::key::End && (event.modifiers & core::modifier::Control) == 0)
            {
                markdown_->hScroll.offsetX = markdown_->MaxScrollX(viewW);
                return true;
            }
        }

        if (markdown_->selectable)
        {
            if ((event.modifiers & core::modifier::Control) != 0)
            {
                if (event.key == 'A' || event.key == 'a')
                {
                    SelectAll();
                    return true;
                }
                if (event.key == 'C' || event.key == 'c')
                {
                    const std::string text = markdown_->HasSelection()
                        ? markdown_->SelectedPlain()
                        : markdown_->layout.plainText;
                    if (markdown_->clipboard != nullptr && !text.empty())
                        markdown_->clipboard->SetText(text);
                    return true;
                }
            }
            if (event.key == core::key::Escape)
            {
                ClearSelection();
                return true;
            }
        }
    }
    return false;
}

void DuiMarkdownView::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;

    const_cast<DuiMarkdownView*>(this)->PollAsyncImages();
    const int previousHeight = markdown_->layout.size.height;
    const int previousWidth = markdown_->layout.size.width;
    const auto previousPrecision = markdown_->layoutPrecision;
    markdown_->EnsureLayout(canvas, Bounds().Width(), Theme(), true);
    if (previousPrecision != LayoutPrecision::Precise
        || markdown_->layout.size.height != previousHeight
        || markdown_->layout.size.width != previousWidth)
    {
        const_cast<DuiMarkdownView*>(this)->InvalidateLayout();
    }

    const core::Rect clipped = core::Rect::Intersect(Bounds(), dirty);
    if (clipped.Empty())
        return;

    canvas.PushClip(clipped);
    detail::MarkdownPaintOptions options;
    options.hoveredLink = markdown_->hoveredLink;
    options.selectionBegin = markdown_->selection.anchor;
    options.selectionEnd = markdown_->selection.caret;
    options.selectionColor = Theme().Get(core::ThemeSlot::TextSelectionBackground);
    if (options.selectionColor.alpha == 0)
        options.selectionColor = {217, 232, 252, 180};
    // 零拷贝：布局保持内容坐标，绘制时通过 origin 平移到屏幕
    options.origin = {Bounds().left - markdown_->hScroll.offsetX, Bounds().top};
    detail::PaintMarkdown(canvas, markdown_->layout, clipped, options);

    // 宽表底栏横向滚动指示（叠在内容之上，不参与布局高度）
    if (markdown_->NeedsHScroll(Bounds().Width()))
    {
        const core::Rect trackLocal =
            markdown_->HScrollTrackLocal(Bounds().Width(), Bounds().Height());
        const core::Rect thumbLocal =
            markdown_->HScrollThumbLocal(Bounds().Width(), Bounds().Height());
        if (!trackLocal.Empty() && !thumbLocal.Empty())
        {
            const core::Rect track{Bounds().left + trackLocal.left, Bounds().top + trackLocal.top,
                                   Bounds().left + trackLocal.right,
                                   Bounds().top + trackLocal.bottom};
            const core::Rect thumb{Bounds().left + thumbLocal.left, Bounds().top + thumbLocal.top,
                                   Bounds().left + thumbLocal.right,
                                   Bounds().top + thumbLocal.bottom};
            canvas.FillRect(track, {0, 0, 0, 40});
            canvas.FillRect(thumb, {120, 120, 130, 160});
        }
    }
    canvas.PopClip();
}

} // namespace ysDui::controls::content
