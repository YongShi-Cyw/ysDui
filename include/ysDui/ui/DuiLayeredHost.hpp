#pragma once

#include "ysDui/ui/DuiPopupHost.hpp"

namespace ysDui::ui {

struct DuiLayeredOptions final {
    core::Rect bounds;
    unsigned char opacity{255};
    bool dismissOnFocusLost{true};
    bool followOwner{};      // owner 移动时保持分层窗口与 owner 的相对屏幕位置
    bool mouseTransparent{}; // 鼠标穿透：命中测试透明，输入交给下方窗口（提示条须为 true，避免遮挡 owner 悬停）
    /**
     * false：指针移动不自动重绘整窗。
     * 用途：圆盘/提示条等悬停高亮频繁的场景；整窗合成较贵，由业务显式
     *       SetBounds / RequestRepaint 触发，避免鼠标每次移动都重绘导致 UI 卡顿。
     */
    bool repaintOnPointerMove{true};
};

class ILayeredHost {
public:
    virtual ~ILayeredHost() = default;
    virtual bool Show(const DuiLayeredOptions& options, std::unique_ptr<core::Control> content,
                      IPopupHost::LayoutHandler layout, IPopupHost::PaintHandler paint) = 0;
    virtual void Hide() = 0;
    virtual void RequestHide() = 0;
    [[nodiscard]] virtual bool Visible() const = 0;
    /** @return 当前分层宿主的弱引用；宿主隐藏后引用失效。 */
    [[nodiscard]] virtual HostRef Reference() const = 0;
    /**
     * 更新分层窗口相对 owner 客户区的位置/尺寸，并重新合成位图。
     * @param bounds 相对 owner 客户区的矩形（设计坐标）
     */
    virtual void SetBounds(core::Rect bounds) = 0;

    /**
     * 仅重新合成分层位图（不改位置/尺寸）
     * 用途：悬停高亮等频繁刷新，避免每次 SetWindowPos 造成顿挫
     */
    virtual void RequestRepaint() = 0;

    virtual void SetDismissedHandler(std::function<void()> handler) = 0;
    /**
     * 将键盘焦点交给分层窗口。
     * 用途：与 owner（如 NX 绘图区）之间按 Tab 往返切换焦点。
     */
    virtual void Activate() = 0;
    /**
     * 注册 owner 窗口几何变化回调。
     * @return true 表示业务已重算并 SetBounds，宿主跳过 followOwner 平移。
     * 用途：贴角等需随绘图区移动/缩放重算；followOwner 仅做平移，不处理缩放与贴角。
     */
    virtual void SetOwnerGeometryHandler(std::function<bool()> handler) = 0;
};

} // namespace ysDui::ui
