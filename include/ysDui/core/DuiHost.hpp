#pragma once

#include <cstddef>
#include <functional>
#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiDpi.hpp"
#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/core/DuiUiDispatcher.hpp"

namespace ysDui::core {

class Host final {
public:
    using PointerHoverHandlerId = std::size_t;

    Host();
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    Host(Host&&) noexcept;
    Host& operator=(Host&&) noexcept;

    void SetRoot(std::unique_ptr<Control> root);
    [[nodiscard]] Control* Root() const;
    [[nodiscard]] Control* HitTest(Point point) const;
    [[nodiscard]] Control* FocusedControl() const;
    /** @return 当前指针捕获控件；无捕获时为空。 */
    [[nodiscard]] Control* CapturedControl() const;
    void SetFocusedControl(Control* control);
    [[nodiscard]] PointerHoverHandlerId SetPointerHoverHandler(std::function<void(Control*, Point)> handler);
    void RemovePointerHoverHandler(PointerHoverHandlerId handler);
    /**
     * 摘出当前根控件树并交出所有权，宿主变为空树。
     * 用途：内容需要在宿主之间搬迁时（如把悬浮窗里的窗格收回停靠区），先摘出再挂到新宿主。
     * 摘出时会清理焦点/悬停/捕获状态并解除继承主题，避免内容继续引用旧宿主的主题。
     * @return 原根控件树；宿主为空时返回空指针
     */
    [[nodiscard]] std::unique_ptr<Control> ReleaseRoot();
    /**
     * 应用根控件树上挂起的布局。
     * 绘制前由窗口调用；不要在指针拖拽的 `Dispatch` 里套用，以免把正在拖的分割条等几何写回。
     *
     * 本方法先执行派发器上待处理的跨线程任务（`Dispatcher().Drain()`），再做布局。
     * 顺序很关键：「应用后台增量」与「按增量排版」必须发生在同一帧内，否则会先画一帧旧布局。
     */
    void PrepareFrame();
    bool Dispatch(const Event& event);
    [[nodiscard]] AnimationClock& Clock();
    /**
     * 取此宿主的 UI 线程派发器。
     * 后台线程通过它把调用转交到 UI 线程；具体契约见 `THREADING.md`。
     */
    [[nodiscard]] DuiUiDispatcher& Dispatcher();
    /** 更新宿主的 DPI 上下文；控件树公开几何始终保持 DIP。 */
    void SetDpiScale(DuiDpiScale scale);
    /** @return 当前宿主对应平台表面的 DPI 缩放。 */
    [[nodiscard]] DuiDpiScale DpiScale() const;
    /** @return 此宿主持有的主题上下文。 */
    [[nodiscard]] DuiTheme& Theme();
    /** @return 此宿主持有的只读主题上下文。 */
    [[nodiscard]] const DuiTheme& Theme() const;
    /** 设置主题变化后的布局与重绘请求回调。 */
    void SetThemeChangedHandler(std::function<void()> handler);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::core
