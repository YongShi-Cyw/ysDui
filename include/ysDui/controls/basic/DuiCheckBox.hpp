/**
 * 文件名：DuiCheckBox.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：独立复选框控件声明，从 DuiButton 的 CheckBox kind 剥离。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/controls/basic/DuiButton.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::basic {

/**
 * 复选框：点击或空格/回车在选中与未选中之间切换。
 * 外观变体复用 `DuiButtonVariant`（Default / Ghost / Outlined / Text），勾选标记由渲染抽象绘制。
 */
class DuiCheckBox final : public core::Control, public render::DuiRenderable {
public:
    DuiCheckBox();
    ~DuiCheckBox() override;
    DuiCheckBox(const DuiCheckBox&) = delete;
    DuiCheckBox& operator=(const DuiCheckBox&) = delete;
    DuiCheckBox(DuiCheckBox&&) noexcept;
    DuiCheckBox& operator=(DuiCheckBox&&) noexcept;

    /**
     * 设置标签文本。
     * @param text UTF-8 标签
     */
    void SetText(std::string text);
    /** @return 当前标签文本 */
    [[nodiscard]] const std::string& Text() const;
    /**
     * 设置选中状态。
     * @param checked true 表示选中
     */
    void SetChecked(bool checked);
    /** @return 是否选中 */
    [[nodiscard]] bool Checked() const;
    /**
     * 设置外观变体（透明底与描边行为与原 Button-CheckBox 一致）。
     * @param variant 外观
     */
    void SetVariant(DuiButtonVariant variant);
    /** @return 当前外观变体 */
    [[nodiscard]] DuiButtonVariant Variant() const;
    /**
     * 设置标签文字样式；颜色在绘制时被主题覆盖。
     * @param style 文字样式
     */
    void SetTextStyle(render::DuiTextStyle style);
    /** @return 当前文字样式 */
    [[nodiscard]] const render::DuiTextStyle& TextStyle() const;
    /**
     * 设置切换后的点击回调（状态已更新）。
     * @param handler 回调；可为空
     */
    void SetClickHandler(std::function<void()> handler);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    void Activate();
    class Impl;
    std::unique_ptr<Impl> checkBox_;
};

} // namespace ysDui::controls::basic
