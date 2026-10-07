/**
 * 文件名：DuiWin32FrameHost.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现 Win32 无边框顶层窗口、标题栏和非客户区交互。
 */
#include "DuiWin32FrameHost.hpp"
#include "DuiWin32Utf8.hpp"
#include "DuiWin32HostRef.hpp"

#include <windows.h>
#include <commctrl.h>

#ifdef DrawText
#undef DrawText
#endif

#include <algorithm>
#include <map>
#include <utility>

#include "ysDui/platform/win32/DuiFrameChrome.hpp"
#include "ysDui/platform/win32/DuiWindow.hpp"
#include "ysDui/core/DuiDpi.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCaptionGlyph.hpp"

namespace ysDui::platform::win32 {
namespace {

constexpr int CaptionIconSize = 16;
/** 标题图标边长（逻辑像素）。 */
constexpr int TitleIconSize = 16;
/** 自定义消息：把标题栏按钮动作推迟到消息处理栈外执行，避免回调内关闭窗口导致的自毁。 */
constexpr ::UINT kInvokeCaptionMessage = WM_APP + 0x51;

[[nodiscard]] int ClampPositive(int value, int fallback)
{
    return value > 0 ? value : fallback;
}

[[nodiscard]] core::Rect CenteredCaptionIconBounds(core::Rect button)
{
    const int centerX = (button.left + button.right) / 2;
    const int centerY = (button.top + button.bottom) / 2;
    return {centerX - CaptionIconSize / 2, centerY - CaptionIconSize / 2,
            centerX + CaptionIconSize / 2, centerY + CaptionIconSize / 2};
}

} // namespace

class Win32FrameHost::Impl
{
public:
    std::shared_ptr<core::Host> host;
    std::unique_ptr<Window> window;
    ui::DuiFrameOptions options;
    LayoutHandler layout;
    PaintHandler paint;
    std::function<void(int)> captionButton;
    std::function<void()> closed;
    bool visible{};
    bool closeNotified{};
    bool maximized{};          // 是否处于自实现的"最大化"状态
    ::RECT restorePixels{};    // 最大化前的窗口矩形（物理像素），供还原
    int hoveredCaption{-1};  // 悬停的标题栏按钮下标（-1 为无）
    int pressedCaption{-1};  // 按下的标题栏按钮下标（-1 为无）
    /** 着色后的标题栏图标缓存：键为「图标种类 + 颜色」，避免每帧重新光栅化。 */
    mutable std::map<std::pair<int, int>, std::shared_ptr<const render::DuiImage>> glyphCache;

    [[nodiscard]] core::Rect ClientBounds() const
    {
        return {0, ClampPositive(options.titleBarHeight, 36), options.size.width, options.size.height};
    }

    void LayoutContent()
    {
        if (layout)
            layout(ClientBounds());
    }

    /** @return 客户区标题栏按钮矩形，顺序与命中判定一致（关闭/最大化/最小化 → 右侧自定义 → 左侧自定义）。 */
    [[nodiscard]] std::vector<core::Rect> CaptionRects() const
    {
        std::vector<core::Rect> result;
        int right = options.size.width;
        constexpr int ButtonWidth = 46;
        if (options.showClose) { right -= ButtonWidth; result.push_back({right, 0, right + ButtonWidth, ClampPositive(options.titleBarHeight, 36)}); }
        if (options.showMaximize) { right -= ButtonWidth; result.push_back({right, 0, right + ButtonWidth, ClampPositive(options.titleBarHeight, 36)}); }
        if (options.showMinimize) { right -= ButtonWidth; result.push_back({right, 0, right + ButtonWidth, ClampPositive(options.titleBarHeight, 36)}); }
        for (const auto& button : options.captionButtons) {
            if (!button.left) { right -= ButtonWidth; result.push_back({right, 0, right + ButtonWidth, ClampPositive(options.titleBarHeight, 36)}); }
        }
        int left{};
        for (const auto& button : options.captionButtons) {
            if (button.left) { result.push_back({left, 0, left + ButtonWidth, ClampPositive(options.titleBarHeight, 36)}); left += ButtonWidth; }
        }
        return result;
    }

    /** @return 该按钮下标是否为关闭按钮（用关闭态反馈色）。 */
    [[nodiscard]] bool IsCloseIndex(int index) const
    {
        return index >= 0 && options.showClose && index == 0;
    }

    /** 绘制标题栏按钮反馈色与矢量符号（观感对齐 DuiDialog）。 */
    void PaintCaptionButton(render::Canvas& canvas, core::Rect bounds, int index, bool hot, bool down) const
    {
        const core::DuiTheme* theme = options.theme;
        if (down || hot)
        {
            const bool closing = IsCloseIndex(index);
            core::Color feedback{};
            if (theme != nullptr)
            {
                feedback = closing
                    ? theme->Get(down ? core::ThemeSlot::DialogClosePressed
                                      : core::ThemeSlot::DialogCloseHover)
                    : theme->Get(down ? core::ThemeSlot::DialogCaptionPressed
                                      : core::ThemeSlot::DialogCaptionHover);
            }
            else
            {
                feedback = down ? core::Color{214, 218, 226, 255} : core::Color{232, 236, 243, 255};
            }
            canvas.FillRect(bounds, feedback);
        }
        // 悬停时用高亮色，与 DuiDialog 一致
        const core::Color iconColor = theme != nullptr
            ? theme->Get(hot ? core::ThemeSlot::DialogCaptionHoverText
                             : core::ThemeSlot::DialogTitleText)
            : options.captionGlyphColor;
        // 先确定语义：关闭/最大化/最小化 → 右侧自定义 → 左侧自定义
        int cursor{};
        if (options.showClose && cursor++ == index)
        {
            DrawCaptionGlyph(canvas, render::DuiCaptionGlyph::Close, bounds, iconColor);
            return;
        }
        if (options.showMaximize && cursor++ == index)
        {
            DrawCaptionGlyph(canvas,
                             maximized ? render::DuiCaptionGlyph::Restore
                                       : render::DuiCaptionGlyph::Maximize,
                             bounds, iconColor);
            return;
        }
        if (options.showMinimize && cursor++ == index)
        {
            DrawCaptionGlyph(canvas, render::DuiCaptionGlyph::Minimize, bounds, iconColor);
            return;
        }
        for (const auto& button : options.captionButtons)
        {
            if (button.left)
                continue;
            if (cursor++ != index)
                continue;
            if (button.image && !button.image->Empty())
                canvas.DrawImage(*button.image, CenteredCaptionIconBounds(bounds));
            else if (!button.glyph.empty())
                canvas.DrawText(button.glyph, bounds, {iconColor, {}, 9, false},
                                render::DuiTextAlignment::Center, false);
            return;
        }
        for (const auto& button : options.captionButtons)
        {
            if (!button.left)
                continue;
            if (cursor++ != index)
                continue;
            if (button.image && !button.image->Empty())
                canvas.DrawImage(*button.image, CenteredCaptionIconBounds(bounds));
            else if (!button.glyph.empty())
                canvas.DrawText(button.glyph, bounds, {iconColor, {}, 9, false},
                                render::DuiTextAlignment::Center, false);
            return;
        }
    }

    /**
     * 绘制标题栏按钮图标。
     * 用共享的矢量图标（与 DuiDialog 同源）并缓存着色结果：按主题 + 版本 + 明暗两个色调缓存，
     * 避免每帧重新光栅化 SVG。
     */
    void DrawCaptionGlyph(render::Canvas& canvas, render::DuiCaptionGlyph glyph, core::Rect bounds,
                          core::Color color) const
    {
        const std::shared_ptr<const render::DuiImage>& image = GlyphImage(glyph, color);
        if (!image)
            return;
        canvas.DrawImage(*image, CenteredCaptionIconBounds(bounds));
    }

    /** @return 着色后的图标位图（按主题版本与颜色缓存）。 */
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& GlyphImage(
        render::DuiCaptionGlyph glyph, core::Color color) const
    {
        const auto key = std::make_pair(static_cast<int>(glyph),
                                        (static_cast<int>(color.red) << 16) |
                                            (static_cast<int>(color.green) << 8) |
                                            static_cast<int>(color.blue));
        const auto found = glyphCache.find(key);
        if (found != glyphCache.end())
            return found->second;
        return glyphCache.emplace(key, render::CreateCaptionGlyphImage(glyph, color)).first->second;
    }

    void PaintFrame(render::Canvas& canvas, core::Rect dirty)
    {
        const core::Rect bounds{0, 0, options.size.width, options.size.height};
        const core::DuiTheme* theme = options.theme;
        const int titleHeight = ClampPositive(options.titleBarHeight, 36);
        if (options.backgroundImage && !options.backgroundImage->Empty())
            render::DrawNinePatch(canvas, *options.backgroundImage, bounds, options.backgroundSourceInsets,
                                  options.backgroundDestinationInsets);
        else
        {
            const core::Color background = theme != nullptr
                ? theme->Get(core::ThemeSlot::DialogBackground) : options.backgroundColor;
            if (background.alpha != 0)
                canvas.FillRect(core::Rect::Intersect(bounds, dirty), background);
        }
        const core::Color titleBar = theme != nullptr
            ? theme->Get(core::ThemeSlot::DialogTitleBackground) : options.titleBarColor;
        if (!options.titleBarTransparent && titleBar.alpha != 0)
            canvas.FillRect({0, 0, options.size.width, titleHeight}, titleBar);
        render::DuiTextStyle style;
        style.color = theme != nullptr ? theme->Get(core::ThemeSlot::DialogTitleText)
                                       : options.titleTextColor;
        style.pointSize = 10;
        const auto rectangles = CaptionRects();
        for (std::size_t index = 0; index < rectangles.size(); ++index)
            PaintCaptionButton(canvas, rectangles[index], static_cast<int>(index),
                               static_cast<int>(index) == hoveredCaption,
                               static_cast<int>(index) == pressedCaption);
        // 标题图标：为窗格提供「前面放图标」的落点，与标签页图标同一张图
        int titleLeft = 12;
        if (options.icon && !options.icon->Empty())
        {
            const int size = TitleIconSize;
            const int top = (titleHeight - size) / 2;
            canvas.DrawImage(*options.icon, {titleLeft, top, titleLeft + size, top + size});
            titleLeft += size + 6;
        }
        canvas.DrawText(options.title, {titleLeft, 0, options.size.width - 8, titleHeight}, style,
                        render::DuiTextAlignment::Start, false);        // 窗口轮廓线：DuiDialog 观感的关键一环，原先完全没有画
        const core::Color border = theme != nullptr ? theme->Get(core::ThemeSlot::DialogBorder)
                                                    : options.borderColor;
        if (border.alpha != 0)
            canvas.StrokeRoundedRect(bounds, 0, border, 1.0F);
        if (paint)
            paint(canvas, dirty);
    }

    void NotifyClosed()
    {
        if (closeNotified)
            return;
        closeNotified = true;
        visible = false;
        if (closed)
            closed();
    }

    /** 就地执行标题栏按钮动作；调用方须保证不在消息处理栈内（见 PostCaptionInvoke）。 */
    void InvokeCaptionByIndex(int index)
    {
        const auto handle = reinterpret_cast<::HWND>(window->NativeHandle().value);
        if (const ui::DuiFrameCaptionButton* custom = CaptionButtonAt(index); custom != nullptr)
        {
            // 自定义按钮也允许声明标准动作，行为与内建的关闭/最大化/最小化一致
            switch (custom->action)
            {
            case ui::DuiFrameCaptionAction::Minimize: ::ShowWindow(handle, SW_MINIMIZE); return;
            case ui::DuiFrameCaptionAction::ToggleMaximize:
                ::ShowWindow(handle, ::IsZoomed(handle) ? SW_RESTORE : SW_MAXIMIZE);
                return;
            case ui::DuiFrameCaptionAction::Close: window->RequestClose(); return;
            case ui::DuiFrameCaptionAction::Custom:
                if (captionButton)
                    captionButton(custom->id);
                return;
            }
            return;
        }
        if (options.showClose && index == 0)
        {
            window->RequestClose();
            return;
        }
        if (options.showMaximize && index == (options.showClose ? 1 : 0))
        {
            ToggleMaximize();
            return;
        }
        if (options.showMinimize && index == (options.showClose ? 1 : 0) + (options.showMaximize ? 1 : 0))
            ::ShowWindow(handle, SW_MINIMIZE);
    }

    /**
     * 切换最大化 / 还原。
     * 自己实现而不是用 `ShowWindow(SW_MAXIMIZE)`：带 WS_THICKFRAME 的窗口由系统最大化时，
     * 窗口矩形会被撑到工作区之外（多出边框厚度），而本窗口客户区即整个窗口，
     * 表现为最大化后盖住任务栏、标题栏内容错位；自实现可精确落在工作区并保留还原尺寸。
     */
    void ToggleMaximize()
    {
        if (window == nullptr)
            return;
        const auto handle = reinterpret_cast<::HWND>(window->NativeHandle().value);
        if (handle == nullptr)
            return;
        if (!maximized)
        {
            ::RECT current{};
            if (!::GetWindowRect(handle, &current))
                return;
            ::MONITORINFO monitor{};
            monitor.cbSize = sizeof(monitor);
            if (!::GetMonitorInfoW(::MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor))
                return;
            ::RECT target = monitor.rcWork;
            // 尊重调用方给定的最大尺寸
            const core::DuiDpiScale scale(static_cast<int>(::GetDpiForWindow(handle)));
            const core::Size maximum = scale.Scale(options.maximumSize);
            if (maximum.width > 0 && target.right - target.left > maximum.width)
                target.right = target.left + maximum.width;
            if (maximum.height > 0 && target.bottom - target.top > maximum.height)
                target.bottom = target.top + maximum.height;
            restorePixels = current;
            maximized = true;
            ::SetWindowPos(handle, nullptr, target.left, target.top,
                           target.right - target.left, target.bottom - target.top,
                           SWP_NOZORDER | SWP_NOACTIVATE);
            Invalidate();
            return;
        }
        maximized = false;
        ::SetWindowPos(handle, nullptr, restorePixels.left, restorePixels.top,
                       restorePixels.right - restorePixels.left, restorePixels.bottom - restorePixels.top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
        Invalidate();
    }

    /** @return 该下标对应的自定义按钮；属关闭/最大化/最小化时为空。 */
    [[nodiscard]] const ui::DuiFrameCaptionButton* CaptionButtonAt(int index) const
    {
        int cursor = (options.showClose ? 1 : 0) + (options.showMaximize ? 1 : 0)
            + (options.showMinimize ? 1 : 0);
        for (const auto& button : options.captionButtons)
        {
            if (button.left)
                continue;
            if (cursor++ == index)
                return &button;
        }
        for (const auto& button : options.captionButtons)
        {
            if (!button.left)
                continue;
            if (cursor++ == index)
                return &button;
        }
        return nullptr;
    }

    /**
     * 把标题栏按钮动作推迟到当前消息处理结束后执行。
     * 必要性：回调（尤其自定义按钮）可能关闭本窗口，而在消息处理栈内销毁窗口会释放正在执行的
     * 状态对象；推迟到消息返回后执行即可避开。
     */
    void PostCaptionInvoke(int index)
    {
        if (window == nullptr || index < 0)
            return;
        const auto handle = reinterpret_cast<::HWND>(window->NativeHandle().value);
        ::PostMessageW(handle, kInvokeCaptionMessage, static_cast<::WPARAM>(index), 0);
    }

    /** @return 客户坐标命中的标题栏按钮下标；未命中返回 -1。 */
    [[nodiscard]] int CaptionIndexAt(core::Point point) const
    {
        const auto rectangles = CaptionRects();
        for (std::size_t index = 0; index < rectangles.size(); ++index)
        {
            if (rectangles[index].Contains(point))
                return static_cast<int>(index);
        }
        return -1;
    }

    void Invalidate()
    {
        if (window == nullptr)
            return;
        ::InvalidateRect(reinterpret_cast<::HWND>(window->NativeHandle().value), nullptr, FALSE);
    }

    /** 记录按下的标题栏按钮并请求重绘（按下反馈色）。 */
    void BeginCaptionPress(core::Point point)
    {
        const int index = CaptionIndexAt(point);
        if (index == pressedCaption)
            return;
        pressedCaption = index;
        Invalidate();
    }

    /** 抬起：命中同一下标时把动作推迟到消息栈外执行。 */
    void FinishCaptionPress(core::Point point)
    {
        const int pressed = pressedCaption;
        const int index = CaptionIndexAt(point);
        if (pressedCaption != -1)
        {
            pressedCaption = -1;
            Invalidate();
        }
        if (pressed != -1 && pressed == index)
            PostCaptionInvoke(index);
    }

    /** 更新标题栏按钮悬停态并请求重绘。 */
    void UpdateCaptionHover(core::Point point)
    {
        const int index = CaptionIndexAt(point);
        if (index == hoveredCaption)
            return;
        hoveredCaption = index;
        Invalidate();
    }

    static ::LRESULT CALLBACK Procedure(::HWND handle, ::UINT message, ::WPARAM word, ::LPARAM data,
                                         ::UINT_PTR, ::DWORD_PTR reference)
    {
        auto* state = reinterpret_cast<Impl*>(reference);
        if (message == kInvokeCaptionMessage)
        {
            state->InvokeCaptionByIndex(static_cast<int>(word));
            return 0;
        }
        if (message == WM_NCCALCSIZE)
        {
            // 无边框自定义标题栏：把客户区显式设为整个窗口。
            // 仅 `return 0` 对 WS_OVERLAPPEDWINDOW 这类带 WS_CAPTION 的样式并不生效，
            // 原生标题栏会与自绘标题栏重叠（表现为标题栏上多出一排系统按钮）。
            if (word == TRUE && data != 0)
            {
                auto* params = reinterpret_cast<::NCCALCSIZE_PARAMS*>(data);
                ::RECT windowRect{};
                if (::GetWindowRect(handle, &windowRect))
                {
                    // 顶层窗口的 rgrc 使用屏幕坐标
                    params->rgrc[0] = windowRect;
                }
            }
            return 0;
        }
        if (message == WM_GETMINMAXINFO) {
            auto* info = reinterpret_cast<::MINMAXINFO*>(data);
            const core::DuiDpiScale scale(static_cast<int>(::GetDpiForWindow(handle)));
            const core::Size minimum = scale.Scale(state->options.minimumSize);
            const core::Size maximum = scale.Scale(state->options.maximumSize);
            info->ptMinTrackSize.x = ClampPositive(minimum.width, 1);
            info->ptMinTrackSize.y = ClampPositive(minimum.height, 1);
            if (state->options.maximumSize.width > 0) info->ptMaxTrackSize.x = maximum.width;
            if (state->options.maximumSize.height > 0) info->ptMaxTrackSize.y = maximum.height;
            // 无边框自定义标题栏：必须把最大化范围钉在工作区上。
            // 否则系统按窗口样式（WS_OVERLAPPEDWINDOW）把窗口矩形撑到工作区外，
            // 表现为最大化后盖住任务栏、标题栏顶到屏幕外、内容对不齐。
            ::MONITORINFO monitor{};
            monitor.cbSize = sizeof(monitor);
            if (::GetMonitorInfoW(::MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor)) {
                info->ptMaxPosition.x = monitor.rcWork.left - monitor.rcMonitor.left;
                info->ptMaxPosition.y = monitor.rcWork.top - monitor.rcMonitor.top;
                info->ptMaxSize.x = monitor.rcWork.right - monitor.rcWork.left;
                info->ptMaxSize.y = monitor.rcWork.bottom - monitor.rcWork.top;
            }
        }
        if (message == WM_SYSCOMMAND) {
            // 最大化/还原由本类自实现（见 ToggleMaximize），系统菜单与双击标题栏都走这里
            const ::WPARAM command = word & 0xFFF0;
            if (command == SC_MAXIMIZE) {
                state->ToggleMaximize();
                return 0;
            }
            if (command == SC_RESTORE && state->maximized) {
                state->ToggleMaximize();
                return 0;
            }
        }
        if (message == WM_EXITSIZEMOVE) {
            // 用户拖动/缩放了"最大化"的窗口：其矩形已不在工作区上，取消最大化标记，
            // 否则标题栏会一直显示"还原"图标
            if (state->maximized && state->window != nullptr) {
                ::RECT current{};
                ::MONITORINFO monitor{};
                monitor.cbSize = sizeof(monitor);
                if (::GetWindowRect(reinterpret_cast<::HWND>(state->window->NativeHandle().value), &current)
                    && ::GetMonitorInfoW(::MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor)
                    && (current.left != monitor.rcWork.left || current.top != monitor.rcWork.top
                        || current.right - current.left != monitor.rcWork.right - monitor.rcWork.left
                        || current.bottom - current.top != monitor.rcWork.bottom - monitor.rcWork.top)) {
                    state->maximized = false;
                    state->Invalidate();
                }
            }
        }
        if (message == WM_NCHITTEST) {
            const ::POINT point{static_cast<int>(static_cast<short>(LOWORD(data))),
                                static_cast<int>(static_cast<short>(HIWORD(data)))};
            ::RECT windowBounds{};
            ::GetWindowRect(handle, &windowBounds);
            const core::DuiDpiScale scale(static_cast<int>(::GetDpiForWindow(handle)));
            const int border = state->options.resizable
                ? scale.Scale(ClampPositive(state->options.resizeBorder, 8)) : 0;
            const bool left = point.x < windowBounds.left + border;
            const bool right = point.x >= windowBounds.right - border;
            const bool top = point.y < windowBounds.top + border;
            const bool bottom = point.y >= windowBounds.bottom - border;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            const core::Point client = scale.Unscale(core::Point{
                point.x - windowBounds.left, point.y - windowBounds.top});
            for (const auto& button : state->CaptionRects()) if (button.Contains(client)) return HTCLIENT;
            if (client.y < ClampPositive(state->options.titleBarHeight, 36)) return HTCAPTION;
        }
        if (message == WM_LBUTTONUP) {            const core::Point pixels{static_cast<int>(static_cast<short>(LOWORD(data))),
                                     static_cast<int>(static_cast<short>(HIWORD(data)))};
            state->FinishCaptionPress(core::DuiDpiScale(
                static_cast<int>(::GetDpiForWindow(handle))).Unscale(pixels));
        }
        if (message == WM_LBUTTONDOWN) {
            const core::Point pixels{static_cast<int>(static_cast<short>(LOWORD(data))),
                                     static_cast<int>(static_cast<short>(HIWORD(data)))};
            state->BeginCaptionPress(core::DuiDpiScale(
                static_cast<int>(::GetDpiForWindow(handle))).Unscale(pixels));
        }
        if (message == WM_MOUSEMOVE) {
            const core::Point pixels{static_cast<int>(static_cast<short>(LOWORD(data))),
                                     static_cast<int>(static_cast<short>(HIWORD(data)))};
            state->UpdateCaptionHover(core::DuiDpiScale(
                static_cast<int>(::GetDpiForWindow(handle))).Unscale(pixels));
        }
        if (message == WM_NCDESTROY)
            state->NotifyClosed();
        return ::DefSubclassProc(handle, message, word, data);
    }
};

Win32FrameHost::Win32FrameHost() : impl_(std::make_unique<Impl>()) {}
Win32FrameHost::~Win32FrameHost() { Close(); }

bool Win32FrameHost::Show(const ui::DuiFrameOptions& options, std::unique_ptr<core::Control> content,
                          LayoutHandler layout, PaintHandler paint)
{
    if (!content || options.size.Empty())
        return false;
    Close();
    impl_->options = options;
    impl_->layout = std::move(layout);
    impl_->paint = std::move(paint);
    impl_->host = std::make_shared<core::Host>();
    impl_->host->SetRoot(std::move(content));
    impl_->LayoutContent();
    impl_->window = std::make_unique<Window>(impl_->host);
    impl_->window->SetPaintHandler([state = impl_.get()](render::Canvas& canvas, core::Rect dirty) { state->PaintFrame(canvas, dirty); });
    impl_->window->SetResizeHandler([state = impl_.get()](core::Size size) {
        state->options.size = size;
        state->LayoutContent();
        // 缩放/最大化后必须请求重绘：否则内容按新尺寸排好了，画面仍是旧的
        state->Invalidate();
    });
    WindowOptions native;
    native.title = options.title;
    native.size = options.size;
    native.owner = {reinterpret_cast<std::uintptr_t>(detail::ResolveHost(options.owner))};
    if (!impl_->window->Create(native)) { Close(); return false; }
    const auto handle = reinterpret_cast<::HWND>(impl_->window->NativeHandle().value);
    ::SetWindowSubclass(handle, Impl::Procedure, 1, reinterpret_cast<::DWORD_PTR>(impl_.get()));
    const core::Rect pixels = impl_->window->DpiScale().Scale(
        {options.position.x, options.position.y,
         options.position.x + options.size.width, options.position.y + options.size.height});
    ::SetWindowPos(handle, nullptr, pixels.left, pixels.top, pixels.Width(), pixels.Height(),
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    ApplySquareFrameCorners(impl_->window->NativeHandle(), options.preferSquareCorners);
    impl_->visible = true;
    impl_->closeNotified = false;
    return true;
}

void Win32FrameHost::SetOptions(const ui::DuiFrameOptions& options)
{
    impl_->options = options;
    if (!impl_->window)
        return;
    const auto handle = reinterpret_cast<::HWND>(impl_->window->NativeHandle().value);
    const std::wstring title = detail::Utf8ToWide(options.title);
    ::SetWindowTextW(handle, title.c_str());
    const core::Rect pixels = impl_->window->DpiScale().Scale(
        {options.position.x, options.position.y,
         options.position.x + options.size.width, options.position.y + options.size.height});
    ::SetWindowPos(handle, nullptr, pixels.left, pixels.top, pixels.Width(), pixels.Height(),
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    ApplySquareFrameCorners(impl_->window->NativeHandle(), options.preferSquareCorners);
    impl_->LayoutContent();
    ::InvalidateRect(handle, nullptr, FALSE);
}

void Win32FrameHost::SetContent(std::unique_ptr<core::Control> content, LayoutHandler layout, PaintHandler paint)
{
    if (!content)
        return;
    impl_->host->SetRoot(std::move(content));
    impl_->layout = std::move(layout);
    impl_->paint = std::move(paint);
    impl_->LayoutContent();
}

void Win32FrameHost::Close()
{
    const bool notify = impl_->visible && !impl_->closeNotified;
    if (impl_->window) {
        const auto handle = reinterpret_cast<::HWND>(impl_->window->NativeHandle().value);
        if (handle != nullptr) ::RemoveWindowSubclass(handle, Impl::Procedure, 1);
        impl_->window->Close();
    }
    impl_->window.reset();
    impl_->host.reset();
    impl_->visible = false;
    if (notify)
        impl_->NotifyClosed();
}

void Win32FrameHost::RequestClose() { if (impl_->window) impl_->window->RequestClose(); }
std::unique_ptr<core::Control> Win32FrameHost::DetachContent()
{
    if (!impl_->host)
        return {};
    // 摘出后窗口的布局/绘制回调仍指向原内容，先清空避免后续重绘触到已迁走的对象
    impl_->layout = {};
    impl_->paint = {};
    return impl_->host->ReleaseRoot();
}
bool Win32FrameHost::Visible() const { return impl_->visible; }
ui::HostRef Win32FrameHost::Reference() const
{
    return impl_->window ? impl_->window->Reference() : ui::HostRef{};
}
void Win32FrameHost::SetCaptionButtonHandler(std::function<void(int)> handler) { impl_->captionButton = std::move(handler); }
void Win32FrameHost::SetClosedHandler(std::function<void()> handler) { impl_->closed = std::move(handler); }

} // namespace ysDui::platform::win32
