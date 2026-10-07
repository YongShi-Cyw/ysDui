#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::basic {

struct DuiGroupBoxPadding final {
    int left{12};
    int top{12};
    int right{12};
    int bottom{12};
};

class DuiGroupBox final : public core::Control, public render::DuiRenderable {
public:
    DuiGroupBox();
    ~DuiGroupBox() override;
    DuiGroupBox(const DuiGroupBox&) = delete;
    DuiGroupBox& operator=(const DuiGroupBox&) = delete;
    DuiGroupBox(DuiGroupBox&&) noexcept;
    DuiGroupBox& operator=(DuiGroupBox&&) noexcept;

    void SetTitle(std::string title);
    [[nodiscard]] const std::string& Title() const;
    void SetBorderColor(core::Color color);
    [[nodiscard]] core::Color BorderColor() const;
    void SetCornerRadius(int pixels);
    [[nodiscard]] int CornerRadius() const;
    void SetTitleStyle(render::DuiTextStyle style);
    [[nodiscard]] const render::DuiTextStyle& TitleStyle() const;
    void SetTitleBackgroundColor(core::Color color);
    [[nodiscard]] core::Color TitleBackgroundColor() const;
    void SetPadding(DuiGroupBoxPadding padding);
    [[nodiscard]] DuiGroupBoxPadding Padding() const;
    void SetTitleStripHeight(int pixels);
    [[nodiscard]] int TitleStripHeight() const;
    void SetContent(std::unique_ptr<core::Control> content);
    [[nodiscard]] core::Control* Content() const;
    [[nodiscard]] static core::Rect ComputeContentRect(core::Rect outer, int titleStripHeight, DuiGroupBoxPadding padding);

    /**
     * 设置标题前是否显示复选框。
     * 勾选状态用于整体启用/停用内容：未勾选时内容根被置为禁用，内部控件随之变灰并停止响应输入。
     * 说明：开启后本控件接管**内容根自身**的 enabled 标志（`Control::Enabled()` 沿父链求值，
     * 因此置内容根即可覆盖整棵子树）；请勿再自行在该内容根上设置 enabled。
     * @param checkable true 显示复选框
     */
    void SetCheckable(bool checkable);
    /** @return 是否显示复选框 */
    [[nodiscard]] bool Checkable() const;
    /**
     * 设置勾选状态；未勾选时内容整棵子树变灰且不可交互。
     * @param checked true 表示启用内容
     */
    void SetChecked(bool checked);
    /** @return 是否勾选（内容处于启用状态） */
    [[nodiscard]] bool Checked() const;
    /**
     * 设置勾选状态变化后的回调（状态已更新）。
     * @param handler 回调；可为空
     */
    void SetCheckChangedHandler(std::function<void(bool)> handler);
    /** @return 复选框矩形；不可勾选时为空矩形 */
    [[nodiscard]] core::Rect CheckBoxRect() const;

    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;
    bool PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                    std::string_view value = {}) override;

private:
    /** 按勾选状态同步内容根的启用标志。 */
    void ApplyContentEnabled();
    void Toggle();

    class Impl;
    std::unique_ptr<Impl> groupBox_;
};

} // namespace ysDui::controls::basic
