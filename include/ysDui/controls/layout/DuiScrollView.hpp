#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::layout {

class DuiScrollView final : public core::Control, public render::DuiRenderable {
public:
    DuiScrollView();
    ~DuiScrollView() override;
    DuiScrollView(const DuiScrollView&) = delete;
    DuiScrollView& operator=(const DuiScrollView&) = delete;
    DuiScrollView(DuiScrollView&&) noexcept;
    DuiScrollView& operator=(DuiScrollView&&) noexcept;

    void SetContent(std::unique_ptr<core::Control> content);
    [[nodiscard]] core::Control* Content() const;
    void SetContentSize(core::Size size);
    [[nodiscard]] core::Size ContentSize() const;
    void SetScrollBarWidth(int pixels);
    [[nodiscard]] int ScrollPosition() const;
    void SetScrollPosition(int position);

    /**
     * 设置是否贴底跟随。
     * 用途：流式输出（如 AI 回答逐字追加）时让视图始终停在末尾。
     * 行为：
     * - 开启即立刻滚到末尾，并在内容尺寸变化时保持贴底；
     * - 视图被移离末尾时**自动关闭**（拖拽滚动条、滚轮上滚、显式定位到非末尾），
     *   避免抢占用户正在查看的位置；
     * - 位置回到末尾时自动重新开启。
     * @param follow true 贴底
     */
    void SetFollowBottom(bool follow);
    /** @return 是否处于贴底跟随状态 */
    [[nodiscard]] bool FollowBottom() const;
    /** 立即滚到末尾，并把贴底跟随置为开启（等价于用户自己滚到了末尾）。 */
    void ScrollToBottom();
    /** @return 当前位置是否已在末尾（无可滚动余量时视为已在末尾） */
    [[nodiscard]] bool AtBottom() const;

    void Layout(core::Rect bounds);
    [[nodiscard]] core::Control* HitTest(core::Point point) override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> scrollView_;
};

} // namespace ysDui::controls::layout
