#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/ui/DuiHostRef.hpp"

namespace ysDui::ui {

enum class DuiPopupPlacement
{
    Below,  // 优先放在锚点下方，空间不足时翻转到上方。
    Above,  // 优先放在锚点上方，空间不足时翻转到下方。
    Right,  // 优先放在锚点右侧，空间不足时翻转到左侧。
    CenterOwner,  // 在 owner 窗口内居中，无 owner 时在工作区居中。
};

enum class DuiPopupModality
{
    Modeless,   // owner 保持可交互。
    OwnerModal, // 宿主可见期间禁用 owner。
};

enum class DuiPopupSizeMode
{
    Specified, // 使用 size 指定的尺寸。
    WorkArea,  // 使用 owner 所在显示器的工作区尺寸。
};

/**
 * 由客户区自行绘制的弹出窗口标题栏。
 * 后端仅使用这些平台无关尺寸完成移动、缩放和标题栏命中测试。
 */
struct DuiPopupChrome final
{
    int titleBarHeight{24};       // 标题栏高度。
    int resizeBorder{8};          // 可缩放窗口的边缘命中宽度。
    int leadingActionWidth{};     // 标题栏左侧不可拖动的按钮区宽度。
    int trailingActionWidth{};    // 标题栏右侧不可拖动的按钮区宽度。
    bool preferSquareCorners{true}; // 是否优先使用直角窗口轮廓。
};

struct DuiPopupOptions final {
    core::Rect anchor;
    core::Size size{200, 200};
    DuiPopupPlacement placement{DuiPopupPlacement::Below};
    bool dismissOnFocusLost{true};
    DuiPopupModality modality{DuiPopupModality::Modeless};
    DuiPopupSizeMode sizeMode{DuiPopupSizeMode::Specified};
    bool resizable{};
    bool followOwner{}; // owner 移动时保持弹出窗口与 owner 的相对位置。
    std::optional<DuiPopupChrome> chrome; // 设置后由内容绘制窗口边框和标题栏。
    /**
     * 启用分层窗口（创建期选项，仅 Show 时生效）。
     * 用途：窗口移动时由 DWM 合成，不再逐帧同步重绘下层窗口；当下层为重绘代价高的窗口
     *       （如 NX 绘图区）时，可显著改善拖动与贴边滑动动画的流畅度。
     * 说明：窗口内容仍为普通 GDI 绘制，不改变外观与不透明度。
     *       新增字段一律追加在末尾，避免破坏已有位置初始化（如 DuiMenu 的聚合初始化）。
     */
    bool layeredWindow{};
};

/**
 * 贴边自动隐藏参数
 * 行为：窗口被拖到参考区域（默认所在显示器工作区）的左/右/上边缘后自动贴边收起，
 *       仅保留一条细边可见；鼠标移入细边滑出，移出后延时再收起。
 */
struct DuiEdgeAutoHideOptions final
{
    bool enabled{};                   // 是否启用
    std::uintptr_t referenceWindow{}; // 边缘判定参考窗口（取其客户区）；0=使用所在显示器工作区
    int slideOutMs{200};              // 滑出动画时长（毫秒）
    int hideMs{250};                  // 收起动画时长（毫秒）
    int leaveDelayMs{250};            // 光标移出窗口后再收起的延时（毫秒）
    int edgeThreshold{10};            // 判定贴边的距离阈值（像素）
    int stripWidth{3};                // 收起后可见的细边宽度（像素）
};

class IPopupHost {
public:
    using LayoutHandler = std::function<void(core::Rect)>;
    using PaintHandler = std::function<void(render::Canvas&, core::Rect)>;
    virtual ~IPopupHost() = default;
    virtual bool Show(const DuiPopupOptions& options, std::unique_ptr<core::Control> content,
                      LayoutHandler layout, PaintHandler paint) = 0;
    /** 动态更新弹出宿主的位置、尺寸模式和模态策略。 */
    virtual void SetOptions(const DuiPopupOptions& options) = 0;
    virtual void Hide() = 0;
    virtual void RequestHide() = 0;
    /** 请求平台最小化当前弹出宿主；不支持最小化的后端可以忽略。 */
    virtual void RequestMinimize() = 0;
    /**
     * 请求开始拖动移动窗口（对齐系统标题栏拖拽）。
     * 用途：自定义标题栏在客户区命中时由内容发起，避免 HTCAPTION 抢走 Move/Leave。
     */
    virtual void RequestMove() = 0;
    [[nodiscard]] virtual bool Visible() const = 0;
    /** @return 当前弹出宿主的弱引用；宿主隐藏后引用失效。 */
    [[nodiscard]] virtual HostRef Reference() const = 0;

    /**
     * @return 弹出窗口的原生句柄；不支持或无窗口时返回 0
     * 用途：调用方需要把窗口实际位置（用户拖动后）换算成自己的坐标并记忆时使用
     */
    [[nodiscard]] virtual std::uintptr_t NativeHandle() const { return 0; }
    virtual void SetDismissedHandler(std::function<void()> handler) = 0;

    /**
     * 启用/停用「光标移出宿主后自动折叠」
     * 说明：定时器与光标探测属平台细节，由宿主实现；折叠语义仍由调用方决定，
     *       触发时通过 toggle 回调通知（在 UI 线程调用）。
     * @param enabled 是否启用
     * @param expandDelayMs 光标移入后展开的驻留延时（毫秒）
     * @param collapseDelayMs 光标移出后折叠的驻留延时（毫秒）
     * @param collapseNow 当前是否已处于折叠态（避免首次触发多余回调）
     * @param toggle 触发回调：true=请求折叠，false=请求展开；为空时停用
     */
    virtual void SetAutoCollapse(bool enabled, int expandDelayMs, int collapseDelayMs,
                                 bool collapseNow, std::function<void(bool)> toggle)
    {
        (void)enabled;
        (void)expandDelayMs;
        (void)collapseDelayMs;
        (void)collapseNow;
        (void)toggle;
    }

    /**
     * 启用/停用「贴边自动隐藏」
     * 说明：拖动结束判定、显示器工作区与动画均属平台细节；
     *       停用时恢复到完整可见位置。
     * @param options 参数；options.enabled 为 false 时停用
     */
    virtual void SetEdgeAutoHide(const DuiEdgeAutoHideOptions& options) { (void)options; }

    /**
     * 立即按当前边缘判定尝试收起
     * 用途：用户勾选「贴边隐藏」且窗口已满足贴边条件时，无需等下一次拖动即可收起。
     * 说明：未启用、不可见或未命中边缘时不做任何事。
     */
    virtual void TryDockEdgeNow() {}

    /**
     * @return 是否处于贴边状态（已收起为细边，或已落位待收起）
     * 用途：贴边状态下面板位置由该边缘决定，不跟随主窗口移动；
     *       折叠态由贴边行为接管，双击标题栏不再切换折叠。
     */
    [[nodiscard]] virtual bool EdgeDocked() const { return false; }

    /**
     * 设置「即将贴边收起」的准备回调
     * 用途：窗口处于折叠态时先回调（供上层展开），再按展开后的窗口落位收起，
     *       否则收起后只剩标题栏尺寸。
     */
    virtual void SetEdgeHidePrepareHandler(std::function<void()> handler) { (void)handler; }

    /**
     * 设置「已贴边收起为细边」的通知
     * 用途：窗口收起时上层可关闭已打开的菜单等依附界面。
     * @param handler 回调；为空则取消通知
     */
    virtual void SetEdgeHiddenHandler(std::function<void()> handler) { (void)handler; }
};

} // namespace ysDui::ui
