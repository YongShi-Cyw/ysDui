/**
 * 文件名：DuiTeachingTip.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：锚定引导气泡：标题、说明与可选操作按钮，基于 DuiFlyout。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/ui/DuiHostRef.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"

namespace ysDui::ui {
class IUiHostFactory;
}

namespace ysDui::controls::window {

/**
 * 引导气泡。用于首次功能说明：锚在控件旁，可带操作按钮。
 */
class DuiTeachingTip final {
public:
    DuiTeachingTip();
    ~DuiTeachingTip();
    DuiTeachingTip(const DuiTeachingTip&) = delete;
    DuiTeachingTip& operator=(const DuiTeachingTip&) = delete;
    DuiTeachingTip(DuiTeachingTip&&) noexcept;
    DuiTeachingTip& operator=(DuiTeachingTip&&) noexcept;

    /**
     * 设置标题。
     * @param title UTF-8 标题
     */
    void SetTitle(std::string title);
    /** @return 当前标题 */
    [[nodiscard]] const std::string& Title() const;
    /**
     * 设置说明正文。
     * @param message UTF-8 正文
     */
    void SetMessage(std::string message);
    /** @return 当前正文 */
    [[nodiscard]] const std::string& Message() const;
    /**
     * 设置操作按钮文案；空串则不显示按钮。
     * @param text UTF-8 按钮文案
     */
    void SetActionText(std::string text);
    /** @return 当前操作文案 */
    [[nodiscard]] const std::string& ActionText() const;
    /**
     * 设置操作按钮回调（点击后关闭气泡）。
     * @param handler 回调；可为空
     */
    void SetActionHandler(std::function<void()> handler);
    void SetDismissedHandler(std::function<void()> handler);
    /**
     * 在锚点旁显示引导。
     * @return 成功显示返回 true
     */
    bool Show(ui::IUiHostFactory& factory, ui::HostRef owner, core::Rect anchor,
              ui::DuiPopupPlacement placement = ui::DuiPopupPlacement::Right);
    void Hide();
    [[nodiscard]] bool Visible() const;

private:
    class Impl;
    std::unique_ptr<Impl> tip_;
};

} // namespace ysDui::controls::window
