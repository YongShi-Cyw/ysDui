#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiHost.hpp"
#include "ysDui/core/DuiDpi.hpp"
#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/ui/DuiHostRef.hpp"
#include "ysDui/render/DuiCanvas.hpp"

namespace ysDui::platform::win32 {

struct WindowOptions final {
    std::string title;
    core::Size size{800, 600};
    core::Point position{};
    NativeWindowHandle owner;
    enum class Kind { TopLevel, Popup } kind{Kind::TopLevel};
    bool resizable{};
    /** false：创建后保持隐藏，供分层窗先 UpdateLayeredWindow 再显示，避免不透明闪屏 */
    bool initiallyVisible{true};
    /**
     * false：指针移动不触发整窗重绘。
     * 用途：分层窗（每帧整窗合成较贵）由业务在内容变更时显式 SetBounds/RequestRepaint，
     *       避免鼠标每次移动都整窗重绘导致 UI 线程饱和。
     */
    bool repaintOnPointerMove{true};
};

class Window final {
public:
    explicit Window(std::shared_ptr<core::Host> host);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    bool Create(const WindowOptions& options);
    int Run();
    void Close();
    void RequestClose();
    [[nodiscard]] NativeWindowHandle NativeHandle() const;
    [[nodiscard]] ui::HostRef Reference() const;
    [[nodiscard]] core::DuiDpiScale DpiScale() const;
    void SetPaintHandler(std::function<void(render::Canvas&, core::Rect)> handler);
    void SetResizeHandler(std::function<void(core::Size)> handler);
    void SetFocusLostHandler(std::function<void(NativeWindowHandle)> handler);
    /** 设置原生窗口销毁后的通知回调。 */
    void SetClosedHandler(std::function<void()> handler);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
