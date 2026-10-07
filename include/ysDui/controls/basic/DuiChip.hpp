/**
 * 文件名：DuiChip.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：声明标签控件（Chip）：单行短文本，可选前置图标与关闭按钮。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::render {
class DuiImage;
}

namespace ysDui::controls::basic {

/** 标签外观。 */
enum class DuiChipVariant
{
    Filled,   // 实心底色 + 描边。
    Outlined, // 透明底 + 彩色描边与文字。
};

/**
 * 标签：用于附件名、建议提问、模型名、已选条件等短信息。
 * 只承载单行文本；超宽时按可用宽度裁剪而不换行。
 */
class DuiChip final : public core::Control, public render::DuiRenderable {
public:
    DuiChip();
    ~DuiChip() override;
    DuiChip(const DuiChip&) = delete;
    DuiChip& operator=(const DuiChip&) = delete;
    DuiChip(DuiChip&&) noexcept;
    DuiChip& operator=(DuiChip&&) noexcept;

    /**
     * 设置标签文本。
     * @param text UTF-8 文本
     */
    void SetText(std::string text);
    /** @return 当前文本 */
    [[nodiscard]] const std::string& Text() const;
    /**
     * 设置外观。
     * @param variant 外观
     */
    void SetVariant(DuiChipVariant variant);
    /** @return 当前外观 */
    [[nodiscard]] DuiChipVariant Variant() const;
    /** 设置底色；不设置时取主题 ChipFill。 */
    void SetFillColor(core::Color color);
    /** @return 底色（显式设置或主题值） */
    [[nodiscard]] core::Color FillColor() const;
    /** 设置文字颜色；不设置时取主题 ChipText。 */
    void SetTextColor(core::Color color);
    /** @return 文字颜色（显式设置或主题值） */
    [[nodiscard]] core::Color TextColor() const;
    /** 设置描边颜色；不设置时取主题 ChipBorder。 */
    void SetBorderColor(core::Color color);
    /** @return 描边颜色（显式设置或主题值） */
    [[nodiscard]] core::Color BorderColor() const;
    /**
     * 设置文字样式；族与字号生效，颜色仍由 TextColor() 决定。
     * @param style 文本样式
     */
    void SetTextStyle(render::DuiTextStyle style);
    /** @return 当前文本样式 */
    [[nodiscard]] const render::DuiTextStyle& TextStyle() const;
    /**
     * 设置圆角。
     * @param pixels 圆角像素；负值表示胶囊（取高度一半）
     */
    void SetCornerRadius(int pixels);
    /** @return 当前圆角设置（-1 表示胶囊） */
    [[nodiscard]] int CornerRadius() const;
    /**
     * 设置内边距。
     * @param horizontal 左右内边距
     * @param vertical 上下内边距
     */
    void SetPadding(int horizontal, int vertical);
    /** @return 左右内边距 */
    [[nodiscard]] int HorizontalPadding() const;
    /** @return 上下内边距 */
    [[nodiscard]] int VerticalPadding() const;
    /** 设置前置图标；空指针表示无图标。 */
    void SetLeadingIcon(std::shared_ptr<const render::DuiImage> icon);
    /** @return 前置图标 */
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& LeadingIcon() const;
    /** 设置前置图标边长。 */
    void SetIconSize(int pixels);
    /** @return 前置图标边长 */
    [[nodiscard]] int IconSize() const;
    /** 设置是否显示关闭按钮。 */
    void SetClosable(bool closable);
    /** @return 是否显示关闭按钮 */
    [[nodiscard]] bool Closable() const;
    /** 设置标签体被点击的回调（不含关闭按钮）。 */
    void SetClickHandler(std::function<void()> handler);
    /** 设置关闭按钮被点击的回调。 */
    void SetCloseHandler(std::function<void()> handler);
    /** @return 关闭按钮矩形；不可关闭或未布局时为空 */
    [[nodiscard]] core::Rect CloseRect() const;
    /** @return 图标与文字所占的内容矩形 */
    [[nodiscard]] core::Rect ContentRect() const;

    /**
     * 计算首选尺寸。
     * 说明：文本宽度按字号估算（无平台测量器依赖），调用方需要精确宽度时请显式给出 Bounds。
     * @return 首选尺寸
     */
    [[nodiscard]] core::Size DesiredSize() const override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;
    bool PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                    std::string_view value) override;

private:
    class Impl;
    std::unique_ptr<Impl> chip_;
};

} // namespace ysDui::controls::basic
