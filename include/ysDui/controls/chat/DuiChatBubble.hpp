/**
 * 文件名：DuiChatBubble.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：声明对话消息气泡：按角色左右分侧，承载任意内容控件，附头像与元信息行。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::render {
class DuiImage;
}

namespace ysDui::controls::chat {

/** 消息角色；决定气泡左右分侧与配色。 */
enum class DuiChatRole
{
    User,      // 用户：靠右，取 ChatUserBubble* 配色。
    Assistant, // 助手：靠左，取 ChatAssistantBubble* 配色。
    System,    // 系统提示：靠左，取浅灰底，与白底助手气泡区分。
};

/** 消息状态；显示在元信息行上。 */
enum class DuiChatStatus
{
    Complete,  // 已完成：不额外显示状态文字。
    Sending,   // 发送中。
    Streaming, // 正在生成。
    Failed,    // 发送失败。
    Stopped,   // 已被用户停止。
};

/**
 * 对话消息气泡。
 *
 * 职责边界：
 * - 只负责**摆放与外观**：分侧、限宽、圆角、头像、元信息行；
 * - 消息正文由调用方通过 `SetContent` 提供（`DuiMarkdownView` / `DuiLabel` / 图片等）；
 * - **不改写内容控件的文字颜色**（内容自带主题）。因此浅色主题的默认配色是"浅底深字"，
 *   需要深底白字时请自行设置内容控件的样式或外观。
 * - 高度随内容自适应：`Layout` 会按 `DesiredSize()` 收缩自身高度（与 `DuiExpander` 一致）；
 *   正文若**不报告首选高度**（例如 `DuiLabel` 返回 0），则沿用调用方给定的高度而非收缩成空气泡——
 *   这类正文请显式给出 Bounds 高度，或改用能报告高度的正文控件（如 `DuiMarkdownView`）。
 */
class DuiChatBubble final : public core::Control, public render::DuiRenderable {
public:
    DuiChatBubble();
    ~DuiChatBubble() override;
    DuiChatBubble(const DuiChatBubble&) = delete;
    DuiChatBubble& operator=(const DuiChatBubble&) = delete;
    DuiChatBubble(DuiChatBubble&&) noexcept;
    DuiChatBubble& operator=(DuiChatBubble&&) noexcept;

    /**
     * 设置角色；同时切换气泡分侧与默认配色。
     * @param role 角色
     */
    void SetRole(DuiChatRole role);
    /** @return 当前角色 */
    [[nodiscard]] DuiChatRole Role() const;
    /**
     * 设置消息正文控件（替换既有内容）。
     * @param content 内容所有权
     */
    void SetContent(std::unique_ptr<core::Control> content);
    /** @return 正文控件；未设置时为空 */
    [[nodiscard]] core::Control* Content() const;
    /**
     * 设置发送者名；会显示在元信息行最前。
     * @param name UTF-8 名称
     */
    void SetName(std::string name);
    /** @return 发送者名 */
    [[nodiscard]] const std::string& Name() const;
    /**
     * 设置头像；空指针表示不占头像列。
     * @param avatar 头像图
     */
    void SetAvatar(std::shared_ptr<const render::DuiImage> avatar);
    /** @return 头像图 */
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& Avatar() const;
    /** 设置头像边长（同时决定头像列宽度）。 */
    void SetAvatarSize(int pixels);
    /** @return 头像边长 */
    [[nodiscard]] int AvatarSize() const;
    /**
     * 设置时间戳文本；会显示在元信息行。
     * @param text UTF-8 文本
     */
    void SetTimestamp(std::string text);
    /** @return 时间戳文本 */
    [[nodiscard]] const std::string& Timestamp() const;
    /**
     * 设置消息状态；`Complete` 之外会在元信息行显示状态文字。
     * @param status 状态
     */
    void SetStatus(DuiChatStatus status);
    /** @return 当前状态 */
    [[nodiscard]] DuiChatStatus Status() const;
    /**
     * 覆盖状态文字。
     * @param text UTF-8 文本；空串表示用状态对应的默认文字
     */
    void SetStatusText(std::string text);
    /** @return 生效的状态文字（覆盖值或默认文字） */
    [[nodiscard]] std::string StatusText() const;
    /**
     * 设置气泡最大宽度。
     * 说明：气泡宽度取 `min(可用宽度, 上限)`。正文控件（如 `DuiMarkdownView`）不报告"自然宽度"，
     *       因此**短消息也会占满该上限**；需要紧凑气泡时请调小上限，或改用能报告宽度的内容控件。
     * @param pixels 上限像素；<=0 表示不限制（占满可用宽度）
     */
    void SetMaxWidth(int pixels);
    /** @return 气泡最大宽度；0 表示不限制 */
    [[nodiscard]] int MaxWidth() const;
    /**
     * 设置正文内边距。
     * @param horizontal 左右内边距
     * @param vertical 上下内边距
     */
    void SetContentPadding(int horizontal, int vertical);
    /** @return 左右内边距 */
    [[nodiscard]] int HorizontalPadding() const;
    /** @return 上下内边距 */
    [[nodiscard]] int VerticalPadding() const;
    /**
     * 设置气泡圆角。
     * @param pixels 圆角像素；负值表示按高度自动（取较小值的一半）
     */
    void SetCornerRadius(int pixels);
    /** @return 当前圆角设置（-1 表示自动） */
    [[nodiscard]] int CornerRadius() const;
    /** 设置气泡底色；不设置时按角色取主题槽。 */
    void SetFillColor(core::Color color);
    /** @return 气泡底色（显式设置或角色对应主题值） */
    [[nodiscard]] core::Color FillColor() const;
    /** 设置气泡描边颜色；alpha 为 0 时不描边。 */
    void SetBorderColor(core::Color color);
    /** @return 气泡描边颜色 */
    [[nodiscard]] core::Color BorderColor() const;

    /** @return 气泡矩形；未布局时为空 */
    [[nodiscard]] core::Rect BubbleRect() const;
    /** @return 头像矩形；无头像时为空 */
    [[nodiscard]] core::Rect AvatarRect() const;
    /** @return 正文矩形；未布局时为空 */
    [[nodiscard]] core::Rect ContentRect() const;
    /** @return 元信息行矩形；无元信息时为空 */
    [[nodiscard]] core::Rect MetaRect() const;

    /**
     * 计算首选尺寸。
     * 说明：高度依赖正文在给定宽度下的换行结果，首次布局前按默认宽度估算。
     * @return 首选尺寸
     */
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    /** @return 元信息行是否需要显示。 */
    [[nodiscard]] bool HasMeta() const;

    class Impl;
    std::unique_ptr<Impl> bubble_;
};

} // namespace ysDui::controls::chat
