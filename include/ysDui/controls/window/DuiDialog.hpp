#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/ui/DuiHostRef.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"

namespace ysDui::ui {
class IUiHostFactory;
}

namespace ysDui::controls::window {

struct DuiDialogButton final
{
    std::string text; // 按钮显示文本。
    int result{};     // 非 IdNone 时作为对话框关闭结果。
    bool primary{};   // Enter 键触发的默认按钮。
};

class DuiDialog final {
public:
    static constexpr int IdNone = 0;
    static constexpr int IdOk = 1;
    static constexpr int IdCancel = 2;
    static constexpr int IdYes = 3;
    static constexpr int IdNo = 4;
    static constexpr int IdCustom = 100;

    DuiDialog();
    ~DuiDialog();
    DuiDialog(const DuiDialog&) = delete;
    DuiDialog& operator=(const DuiDialog&) = delete;
    DuiDialog(DuiDialog&&) noexcept;
    DuiDialog& operator=(DuiDialog&&) noexcept;

    /** 设置 portable 标题栏文字。 */
    void SetTitle(std::string title);
    /** 设置是否绘制标题栏（含设置/关闭等标题按钮）。
     * @param visible false 时内容区铺满窗口；空白处可拖动移动窗口
     */
    void SetTitleBarVisible(bool visible);
    /** @return 当前是否显示标题栏 */
    [[nodiscard]] bool TitleBarVisible() const;
    /**
     * 同步外框逻辑尺寸到对话框记忆（normalOptions），供后续 SetOptions/显隐标题栏使用。
     * @param size 逻辑像素外框尺寸（与 Show 时 options.size 同坐标系）
     */
    void SetOuterSize(core::Size size);
    /**
     * 更新弹出锚点（弹出位置相对 owner 的计算基准）。
     * 用途：贴边/右对齐的弹出窗口在尺寸变化后需重算锚点，否则宽度改变会挤掉边距。
     * @param anchor 新的锚点矩形（与 Show 时 options.anchor 同坐标系）
     */
    void SetAnchor(core::Rect anchor);
    /** 设置底部按钮；结果为 IdNone 的按钮保持对话框打开。 */
    void SetButtons(std::vector<DuiDialogButton> buttons);
    /**
     * 设置无底部按钮时内容区相对窗口的边距（标题栏下方与左/右/底）。
     * @param padding 边距像素；负数表示使用默认值（左右 16、上下 12）
     */
    void SetContentPadding(int padding);
    /**
     * 设置有底部按钮时，「面板」相对窗口的内缩与内容相对面板的内边距。
     * 用途：默认值（内缩 12、内边距 10）是对话框统一风格；内容需要更贴近窗口时调小。
     * @param margin 面板相对窗口左右与标题栏下方的内缩像素
     * @param padding 业务控件相对面板的内边距像素
     */
    void SetPanelInsets(int margin, int padding);
    /**
     * 设置面板是否绘制自身底色。
     * 用途：默认（true）面板以 DialogPanelBackground 呈现为一层内嵌窗口；
     *       置 false 时面板不绘底色，与对话框背景同色，仅保留面板边框。
     * @param visible 是否绘制面板底色
     */
    void SetPanelBackgroundVisible(bool visible);
    /**
     * 设置是否绘制窗口轮廓线（最外圈边框 + 内嵌面板边框）。
     * 用途：默认（true）以 DialogBorder / DialogPanelBorder 描出轮廓；
     *       置 false 时两条轮廓都不绘制，窗口与面板只见底色不见线。
     * @param visible 是否绘制轮廓线
     */
    void SetBorderVisible(bool visible);
    /**
     * 设置「鼠标中键按下」是否等同触发默认（主）按钮。
     * 用途：取色/确认类面板可用中键一键确定，省去移动光标到按钮的动作。
     * @param accepts 是否响应中键；默认 false（不影响其它对话框）
     */
    void SetMiddleClickAccepts(bool accepts);
    /** 启用无最大化按钮时的标题栏双击折叠。 */
    void SetCollapsible(bool collapsible);
    /** 启用标题栏最大化按钮和双击最大化。 */
    void SetMaximizable(bool maximizable);
    /** 设置 Dialog 是否在 owner 窗口移动时保持相对位置。 */
    void SetFollowOwnerWindow(bool follow);
    /** 返回 Dialog 是否跟随 owner 窗口移动。 */
    [[nodiscard]] bool FollowOwnerWindow() const;
    /** 设置标题栏左侧设置按钮回调。 */
    void SetSettingsHandler(std::function<void()> handler);
    /** 设置无最大化按钮时标题栏刷新按钮的回调。 */
    void SetRefreshHandler(std::function<void()> handler);
    /** 设置无最大化按钮时标题栏帮助按钮的回调。 */
    void SetHelpHandler(std::function<void()> handler);
    /** 设置无最大化按钮时是否显示标题栏帮助按钮（默认显示）。 */
    void SetShowHelpButton(bool show);
    /**
     * 返回对话框弹出宿主弱引用（用于再弹出菜单等子宿主）。
     * @return 可见时有效；隐藏后失效
     */
    [[nodiscard]] ui::HostRef HostReference() const;
    /**
     * 返回标题栏设置按钮在对话框客户区内的矩形（用于菜单锚点）。
     * @return 不可用时返回空矩形
     */
    [[nodiscard]] ysDui::core::Rect SettingsButtonRect() const;
    /**
     * 请求开始拖动移动对话框窗口（对齐系统标题栏拖拽）。
     * 用途：内容区（如工具条按钮）按住拖动时转发。
     */
    void RequestMove();
    bool Show(ui::IUiHostFactory& factory, ui::HostRef owner,
              ui::DuiPopupOptions options, std::unique_ptr<core::Control> content);
    void Close(int result);
    void SetCloseResult(int result);
    [[nodiscard]] int CloseResult() const;
    [[nodiscard]] int Result() const;
    [[nodiscard]] bool Visible() const;
    /** 返回当前是否只显示标题栏。 */
    [[nodiscard]] bool Collapsed() const;
    /** 返回当前是否使用宿主工作区尺寸。 */
    [[nodiscard]] bool Maximized() const;
    /**
     * 设置折叠状态（仅当允许折叠且未启用最大化时生效）
     * @param collapsed true=只显示标题栏
     */
    void SetCollapsed(bool collapsed);
    /** 在允许折叠时切换折叠状态。 */
    void ToggleCollapsed();
    /** 在允许最大化时切换最大化状态。 */
    void ToggleMaximized();

    /**
     * 启用/停用「光标移出对话框后自动折叠」
     * 行为：光标移入需连续停留 expandDelayMs 才展开；移出需连续离开 collapseDelayMs 才折叠。
     * 说明：仅在启用折叠且未启用最大化时生效；定时器与光标探测由宿主平台层完成。
     *       与 SetEdgeAutoHide 互斥：启用本项会关闭贴边隐藏。
     * @param enabled 是否启用
     * @param expandDelayMs 展开驻留延时（毫秒）
     * @param collapseDelayMs 折叠驻留延时（毫秒）
     */
    void SetAutoCollapse(bool enabled, int expandDelayMs = 200, int collapseDelayMs = 250);

    /** @return 是否已启用自动折叠。 */
    [[nodiscard]] bool AutoCollapse() const;

    /**
     * 启用/停用「贴边自动隐藏」
     * 行为：对话框被拖到参考区域（默认所在显示器工作区）左/右/上边缘后自动贴边收起，
     *       仅保留一条细边；鼠标移入细边滑出，移出 leaveDelayMs 后再收起。
     * 说明：仅在允许折叠且未启用最大化时生效；与 SetAutoCollapse 互斥（启用本项会关闭自动折叠）。
     *       拖动结束判定、工作区约束与滑动动画由宿主平台层完成。
     * @param enabled 是否启用
     * @param referenceWindow 边缘判定参考窗口句柄；0=所在显示器工作区
     * @param slideOutMs 滑出动画时长（毫秒）
     * @param hideMs 收起动画时长（毫秒）
     * @param leaveDelayMs 光标移出后再收起的延时（毫秒）
     */
    void SetEdgeAutoHide(bool enabled, std::uintptr_t referenceWindow = 0, int slideOutMs = 200,
                         int hideMs = 250, int leaveDelayMs = 250);

    /** @return 是否已启用贴边自动隐藏。 */
    [[nodiscard]] bool EdgeAutoHide() const;

    /**
     * @return 弹出窗口的原生句柄；不可用时返回 0
     * 用途：调用方需要读取窗口被用户拖动后的实际位置以便记忆
     */
    [[nodiscard]] std::uintptr_t NativeHandle() const;

    /**
     * @return 当前是否处于贴边状态（已收起或已落位待收起）
     * 说明：贴边期间窗口位置由边缘决定，调用方不应再下发位置/尺寸请求，
     *       否则会把窗口拽离贴边位置（表现为抖动、拖不动）。
     */
    [[nodiscard]] bool EdgeDocked() const;

    /**
     * 设置「已贴边收起为细边」的通知
     * 用途：窗口收起时调用方关闭已打开的菜单等依附界面。
     * @param handler 回调；为空则取消通知
     */
    void SetEdgeHiddenHandler(std::function<void()> handler);

    /**
     * 立即按当前边缘判定尝试收起（仅贴边隐藏已启用时生效）
     * 用途：勾选「贴边隐藏」时窗口若已在边缘，无需等下一次拖动即可收起。
     */
    void TryDockEdgeNow();

    void SetClosedHandler(std::function<void(int)> handler);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::controls::window
