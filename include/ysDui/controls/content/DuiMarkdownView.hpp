/**
 * 文件名：DuiMarkdownView.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：声明 AI 对话场景的只读 Markdown 视图控件（GFM 子集、流式节流）。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/ui/DuiClipboard.hpp"

namespace ysDui::controls::media {
class DuiAsyncImageLoader;
}

namespace ysDui::controls::content {

/**
 * Markdown 视图外观：跟随宿主或强制浅色/深色配色倾向。
 */
enum class DuiMarkdownAppearance {
    Auto,  // 跟随 Host 主题槽位
    Light, // 倾向浅色表面与正文色
    Dark,  // 倾向深色表面与正文色
};

/**
 * 链接激活语义：是否将链接视为外部打开目标。
 */
enum class DuiMarkdownLinkTarget {
    External, // 外部链接语义（由业务决定如何打开）
    None,     // 仅回调，不附带外部打开语义
};

/**
 * 文档大纲条目（供 TOC / 侧栏 / 无障碍）。
 * @note `y` 为内容坐标（与 ContentSize / 布局一致）；嵌套标题取所属顶层块的 y0。
 */
struct DuiMarkdownOutlineEntry final {
    int level{1};      // 标题级别 1–6
    std::string text;  // 纯文本标题
    int y{};           // 内容区垂直位置（DIP）
};

/**
 * 只读 Markdown 视图：解析 GFM 子集并以 Canvas 自绘。
 *
 * 面向 AI 流式输出：支持节流重解析、未闭合代码围栏预补全与尾部光标。
 * 支持正文划选复制、双击选词、同步/异步图片加载。
 * 宽表横向滚动：Shift+滚轮、中键拖动、左右方向键、底栏滑块。
 * 纵向滚动由外层 `DuiScrollView` 负责；本控件通过 `ContentSize()` 报告内容尺寸。
 * 绘制零拷贝：布局保持内容坐标，Paint 仅按 origin 平移并裁剪 dirty。
 */
class DuiMarkdownView final : public core::Control, public render::DuiRenderable
{
public:
    /**
     * 图片解析回调：根据 Markdown 图片 src 返回已解码图像。
     * 返回空指针时绘制占位框。调用方负责缓存；控件内仅短期缓存上次结果。
     */
    using ImageProvider = std::function<std::shared_ptr<const render::DuiImage>(std::string_view src)>;

    DuiMarkdownView();
    ~DuiMarkdownView() override;
    DuiMarkdownView(const DuiMarkdownView&) = delete;
    DuiMarkdownView& operator=(const DuiMarkdownView&) = delete;
    DuiMarkdownView(DuiMarkdownView&&) noexcept;
    DuiMarkdownView& operator=(DuiMarkdownView&&) noexcept;

    /** 设置 Markdown 源文本（UTF-8）。流式中会按节流策略延迟重解析。 */
    void SetContent(std::string content);
    /** @return 当前 Markdown 源文本。 */
    [[nodiscard]] const std::string& Content() const;

    /** 标记是否处于流式输出中（影响未闭合围栏容错与光标）。 */
    void SetStreaming(bool streaming);
    [[nodiscard]] bool Streaming() const;

    /** 流式时是否绘制尾部光标。 */
    void SetStreamingCursor(bool enabled);
    [[nodiscard]] bool StreamingCursor() const;

    /**
     * 流式重解析节流间隔（毫秒）。
     * @param milliseconds 小于等于 0 表示每次 `SetContent` 立即解析。
     */
    void SetStreamThrottleMs(int milliseconds);
    [[nodiscard]] int StreamThrottleMs() const;

    /** 是否消毒：剥离 raw HTML、拦截危险链接协议。默认开启。 */
    void SetSanitize(bool sanitize);
    [[nodiscard]] bool Sanitize() const;

    /** 外观倾向。 */
    void SetAppearance(DuiMarkdownAppearance appearance);
    [[nodiscard]] DuiMarkdownAppearance Appearance() const;

    /** 是否绘制代码块 chrome（语言标签、行号、轻量语法着色与背景）。 */
    void SetCodeHighlight(bool enabled);
    [[nodiscard]] bool CodeHighlight() const;

    /** 是否在代码块右上角显示复制按钮。 */
    void SetCodeCopyable(bool enabled);
    [[nodiscard]] bool CodeCopyable() const;

    /** 是否渲染 GFM 表格。关闭时表格按普通段落文本回退。 */
    void SetEnableTable(bool enabled);
    [[nodiscard]] bool EnableTable() const;

    /** 是否允许划选正文并 Ctrl+C / Ctrl+A。默认开启。 */
    void SetSelectable(bool selectable);
    [[nodiscard]] bool Selectable() const;

    /** 链接目标语义。 */
    void SetLinkTarget(DuiMarkdownLinkTarget target);
    [[nodiscard]] DuiMarkdownLinkTarget LinkTarget() const;

    /** 复制代码/选区时使用的剪贴板；为空时仅触发回调。 */
    void SetClipboard(ui::DuiClipboard* clipboard);

    /**
     * 注入同步图片解析器；优先于异步加载器。
     * 返回空指针时绘制占位；若同时设置了异步加载器，则回退到异步路径。
     */
    void SetImageProvider(ImageProvider provider);

    /**
     * 注入异步本地图片加载器（`DuiAsyncImageLoader`）。
     * 对非 http(s)/data 的 src 提交解码；完成前显示占位，完成后自动重布局。
     * 需调用方预先 `SetDecoder`；可为空。
     */
    void SetAsyncImageLoader(media::DuiAsyncImageLoader* loader);

    /** 注入动画时钟以在节流窗口结束后刷新解析/轮询异步图片；可为空。 */
    void SetAnimationClock(core::AnimationClock* clock);

    /** 代码复制回调：参数为代码正文与语言标签。 */
    void SetCodeCopyHandler(std::function<void(std::string_view code, std::string_view language)> handler);
    /** 链接点击回调。 */
    void SetLinkActivatedHandler(std::function<void(std::string_view href)> handler);
    /** 一次成功渲染（解析+布局）完成后的回调。 */
    void SetRenderedHandler(std::function<void()> handler);
    /** 解析或布局错误回调。 */
    void SetErrorHandler(std::function<void(std::string_view message)> handler);

    /** @return 当前选中的纯文本；无选区时为空。 */
    [[nodiscard]] std::string SelectedText() const;
    /** 全选可见纯文本。 */
    void SelectAll();
    /** 清除选区。 */
    void ClearSelection();

    /** @return 最近一次布局后的内容尺寸（DIP，宽度可大于控件以容纳宽表）。 */
    [[nodiscard]] core::Size ContentSize() const;
    /** @return 当前横向滚动偏移（宽表超出视口时，单位 DIP）。 */
    [[nodiscard]] int ScrollOffsetX() const;

    /**
     * 标题大纲（含内容 Y，便于 TOC 滚动定位）。
     * @note 会确保已解析并完成当前宽度下的布局。
     */
    [[nodiscard]] std::vector<DuiMarkdownOutlineEntry> GetOutline() const;

    /**
     * 请求祖先 `DuiScrollView` 滚到内容坐标 Y（TOC / 锚点跳转）。
     * @param contentY 内容区垂直位置（与 `GetOutline().y` 一致）。
     * @return 找到并设置了滚动容器时为 true。
     */
    bool ScrollToY(int contentY);

    /**
     * 滚到大纲条目；等价于 `ScrollToY(GetOutline()[index].y)`。
     * @return 索引有效且滚动成功时为 true。
     */
    bool ScrollToOutline(std::size_t index);

    /**
     * 可选：自定义纵向滚动请求（优先于查找祖先 ScrollView）。
     * 参数为内容坐标 Y；设为空则恢复祖先查找。
     */
    void SetScrollRequestHandler(std::function<void(int contentY)> handler);

    [[nodiscard]] core::Size DesiredSize() const override;

    void Layout(core::Rect bounds) override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    /** 轮询异步图片结果；有更新时标记脏并请求重布局。 */
    bool PollAsyncImages();

    class Impl;
    std::unique_ptr<Impl> markdown_;
};

} // namespace ysDui::controls::content
