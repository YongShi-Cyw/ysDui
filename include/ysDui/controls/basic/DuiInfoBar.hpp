/**
 * 文件名：DuiInfoBar.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明内联信息横幅——在页面内常驻显示状态/校验/告警，与临时浮层 DuiToast 职责不同。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::basic {

/** 信息横幅的严重级别，决定强调色与图标。 */
enum class DuiInfoBarSeverity {
    Information, // 中性说明：授权状态、操作提示
    Success,     // 成功：保存完成、校验通过
    Warning,     // 警告：不影响继续但需注意
    Error,       // 错误：操作失败或数据非法
};

/**
 * 内联信息横幅。
 * 与 DuiToast 的区别：本控件在布局内占位、常驻显示、随父容器排布；
 * DuiToast 是覆盖层、自动消失、不参与布局。
 */
class DuiInfoBar final : public core::Control, public render::DuiRenderable {
public:
    DuiInfoBar();
    ~DuiInfoBar() override;
    DuiInfoBar(const DuiInfoBar&) = delete;
    DuiInfoBar& operator=(const DuiInfoBar&) = delete;
    DuiInfoBar(DuiInfoBar&&) noexcept;
    DuiInfoBar& operator=(DuiInfoBar&&) noexcept;

    void SetSeverity(DuiInfoBarSeverity severity);
    [[nodiscard]] DuiInfoBarSeverity Severity() const;
    /** 设置标题（加粗行）；空字符串表示不显示标题行。 */
    void SetTitle(std::string title);
    [[nodiscard]] const std::string& Title() const;
    /** 设置正文；支持 '\n' 换行，长文本按控件宽度自动换行。 */
    void SetMessage(std::string message);
    [[nodiscard]] const std::string& Message() const;
    /** 设置是否显示右上角关闭按钮。 */
    void SetClosable(bool closable);
    [[nodiscard]] bool Closable() const;
    /** 注册关闭按钮回调；点击后横幅自行隐藏。 */
    void SetClosedHandler(std::function<void()> handler);
    void SetTextStyle(render::DuiTextStyle style);
    [[nodiscard]] const render::DuiTextStyle& TextStyle() const;

    /** @return 内容区（图标右侧、关闭按钮左侧的文本区域）；由当前 Bounds() 推导。 */
    [[nodiscard]] core::Rect ContentRect() const;
    /** @return 关闭按钮命中矩形；不可关闭时为空矩形。 */
    [[nodiscard]] core::Rect CloseRect() const;
    /**
     * @return 按显式 '\n' 分行的总行数（标题行 + 正文行，至少 1）。
     * 不含按宽度自动换行产生的行；高度计算只依据显式换行，保证不依赖文本测量器。
     */
    [[nodiscard]] int LineCount() const;

    /** @return 最小可用尺寸：宽度取 0（由容器决定），高度按显式换行行数计算。 */
    [[nodiscard]] core::Size DesiredSize() const override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    /** @return 正文与标题需要的总行数（用于高度计算）。 */
    [[nodiscard]] int TextLineCount() const;

    class Impl;
    std::unique_ptr<Impl> bar_;
};

} // namespace ysDui::controls::basic
