/**
 * 文件名：DuiFlyout.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明锚定内容气泡——在指定锚点旁浮出任意控件内容，用于字段旁挂参数编辑等场景。
 */
#pragma once

#include <functional>
#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/ui/DuiHostRef.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"

namespace ysDui::ui {
class IUiHostFactory;
}

namespace ysDui::controls::window {

/**
 * 锚定内容气泡。
 *
 * 与既有弹窗控件的区别：
 * - `DuiMenu` 承载的是菜单项列表，本控件承载**任意**控件内容；
 * - `DuiToast` 是覆盖层、自动消失，本控件是锚定弹窗、点击外部关闭。
 *
 * 使用方式（可反复开合）：
 * @code
 * flyout.SetContentFactory([&] { return BuildParamEditor(); });
 * flyout.Show(factory, owner, fieldBounds, ui::DuiPopupPlacement::Right);
 * flyout.Hide();
 * @endcode
 *
 * 内容尺寸默认取 `Control::DesiredSize()`；需要固定尺寸时用 `SetFixedSize`。
 */
class DuiFlyout final {
public:
    DuiFlyout();
    ~DuiFlyout();
    DuiFlyout(const DuiFlyout&) = delete;
    DuiFlyout& operator=(const DuiFlyout&) = delete;
    DuiFlyout(DuiFlyout&&) noexcept;
    DuiFlyout& operator=(DuiFlyout&&) noexcept;

    /**
     * 设置内容工厂。
     * Show 每次调用该工厂创建一份新内容（内容所有权随 Show 交给宿主），
     * 因此同一气泡可反复开合，而不需要在外部保存控件指针。
     * @param factory 返回新内容的函数；为空时 Show 直接失败。
     */
    void SetContentFactory(std::function<std::unique_ptr<core::Control>()> factory);
    /** 设置固定弹出尺寸；宽或高非正时该方向改用内容的 DesiredSize。 */
    void SetFixedSize(core::Size size);
    [[nodiscard]] core::Size FixedSize() const;
    /**
     * 设置布局回调；默认为对内容调用 `SetBounds`。
     * 用途：内容若是自带布局算法的容器，可在此改为调用其 `Layout`。
     */
    void SetLayoutHandler(std::function<void(core::Control&, core::Rect)> handler);
    void SetDismissedHandler(std::function<void()> handler);
    /**
     * 在锚点旁浮出内容。
     * @param factory UI 宿主工厂
     * @param owner 归属宿主
     * @param anchor 锚点矩形（客户区坐标）
     * @param placement 相对锚点的优先放置方向，默认右侧
     * @return 成功显示返回 true；未设置内容工厂或宿主创建失败时返回 false
     */
    bool Show(ui::IUiHostFactory& factory, ui::HostRef owner, core::Rect anchor,
              ui::DuiPopupPlacement placement = ui::DuiPopupPlacement::Right);
    void Hide();
    [[nodiscard]] bool Visible() const;
    /** @return 弹出宿主原生句柄；无窗口时返回 0。 */
    [[nodiscard]] std::uintptr_t NativeHandle() const;

private:
    class Impl;
    std::unique_ptr<Impl> flyout_;
};

} // namespace ysDui::controls::window
