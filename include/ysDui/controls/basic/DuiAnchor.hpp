/**
 * 文件名：DuiAnchor.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：锚点导航控件（对标 TDesign Anchor：轨道/游标/多级/滚动同步）。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::basic {

/** 锚点尺寸：影响首选宽度（small=120 / medium=200 / large=320）。 */
enum class DuiAnchorSize {
    Small,  // 紧凑宽度 120
    Medium, // 默认宽度 200
    Large,  // 宽松宽度 320
};

/**
 * 单条锚点项（对标 AnchorItem：href / title；level/contentY 为宿主滚动适配扩展）。
 */
struct DuiAnchorItem final {
    std::string href;   // 锚点 id（必填语义；用于活动项与 change 回调）
    std::string title;  // 显示文本
    int level{1};       // 多级缩进，1–6
    int contentY{};     // 目标内容 Y（DIP，供 ScrollView / Markdown 跳转）
    bool customScroll{}; // 保留：为 true 时仍触发 click/navigate，由宿主决定滚动方式
};

/**
 * 锚点导航：左侧灰轨 + 活动蓝游标；点击跳转；可按滚动位置同步高亮。
 *
 * Web 版 `container` / `affixProps` 在此由宿主 `SetNavigateHandler` +
 * `SyncActiveFromScroll` 替代；`AnchorTarget` 对应项上的 `href`/`contentY`。
 */
class DuiAnchor final : public core::Control, public render::DuiRenderable
{
public:
    DuiAnchor();
    ~DuiAnchor() override;
    DuiAnchor(const DuiAnchor&) = delete;
    DuiAnchor& operator=(const DuiAnchor&) = delete;
    DuiAnchor(DuiAnchor&&) noexcept;
    DuiAnchor& operator=(DuiAnchor&&) noexcept;

    void SetItems(std::vector<DuiAnchorItem> items);
    void AddItem(DuiAnchorItem item);
    void ClearItems();
    [[nodiscard]] const std::vector<DuiAnchorItem>& Items() const;

    void SetActiveHref(std::string_view href);
    [[nodiscard]] const std::string& ActiveHref() const;
    void SetActiveIndex(int index);
    [[nodiscard]] int ActiveIndex() const;

    void SetSize(DuiAnchorSize size);
    [[nodiscard]] DuiAnchorSize Size() const;
    /** @return 按 size 枚举的首选宽度。 */
    [[nodiscard]] int PreferredWidth() const;

    /** 判定活动锚点的边界容差（对标 bounds，默认 5）。 */
    void SetBoundsTolerance(int pixels);
    [[nodiscard]] int BoundsTolerance() const;

    /** 跳转时附加的目标偏移（对标 targetOffset，默认 0）。 */
    void SetTargetOffset(int pixels);
    [[nodiscard]] int TargetOffset() const;

    /**
     * 按滚动位置同步活动项：取满足 `contentY <= scrollY + bounds` 的最后一项。
     * @param scrollY 滚动容器当前 scrollTop。
     */
    void SyncActiveFromScroll(int scrollY);

    /** 活动锚点改变时回调（currentHref, previousHref）。 */
    void SetChangeHandler(std::function<void(std::string_view, std::string_view)> handler);
    /** 点击锚点时回调（href, title, index）。 */
    void SetClickHandler(std::function<void(std::string_view, std::string_view, int)> handler);
    /**
     * 点击后请求宿主滚动到目标（contentY - targetOffset, href）。
     * 未设置时仅更新活动项并触发 click/change。
     */
    void SetNavigateHandler(std::function<void(int contentY, std::string_view href)> handler);

    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds) override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    class Impl;
    std::unique_ptr<Impl> anchor_;
};

} // namespace ysDui::controls::basic
