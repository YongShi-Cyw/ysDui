/**
 * 文件名：DuiNavigationView.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：左侧导航壳：条目列表 + 内容区，支持展开与紧凑宽度。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::window {

enum class DuiNavigationPaneDisplayMode {
    Left, // 展开侧栏，显示完整标题
    LeftCompact, // 窄侧栏，仅显示首字
};

/**
 * 导航视图：左侧条目、右侧内容。不实现 WinUI 的自适应阈值切换。
 */
class DuiNavigationView final : public core::Control, public render::DuiRenderable {
public:
    DuiNavigationView();
    ~DuiNavigationView() override;
    DuiNavigationView(const DuiNavigationView&) = delete;
    DuiNavigationView& operator=(const DuiNavigationView&) = delete;
    DuiNavigationView(DuiNavigationView&&) noexcept;
    DuiNavigationView& operator=(DuiNavigationView&&) noexcept;

    /**
     * 追加导航项。
     * @param text UTF-8 标题
     * @param id 业务标识，默认 0
     * @return 新项索引
     */
    int AddItem(std::string text, int id = 0);
    /** @return 导航项数量 */
    [[nodiscard]] int ItemCount() const;
    /** @return 指定项标题；索引无效时为空串 */
    [[nodiscard]] std::string ItemText(int index) const;
    /**
     * 选中指定项并通知回调。
     * @param index 项索引
     * @param notify 是否触发 SelectionChanged
     */
    void SetSelectedIndex(int index, bool notify = true);
    /** @return 当前选中索引；无选中为 -1 */
    [[nodiscard]] int SelectedIndex() const;
    void SetPaneDisplayMode(DuiNavigationPaneDisplayMode mode);
    [[nodiscard]] DuiNavigationPaneDisplayMode PaneDisplayMode() const;
    /**
     * 设置内容控件（替换既有内容）。
     * @param content 内容所有权
     */
    void SetContent(std::unique_ptr<core::Control> content);
    /** @return 内容控件；未设置时为空 */
    [[nodiscard]] core::Control* Content() const;
    void SetSelectionChangedHandler(std::function<void(int)> handler);
    void Layout(core::Rect bounds);
    [[nodiscard]] core::Rect PaneRect() const;
    [[nodiscard]] core::Rect ContentRect() const;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> nav_;
};

} // namespace ysDui::controls::window
