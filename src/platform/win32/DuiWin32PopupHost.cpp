#include "ysDui/platform/win32/DuiWin32PopupHost.hpp"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <string>
#include <utility>

#include "DuiWin32HostRef.hpp"
#include "ysDui/platform/win32/DuiFrameChrome.hpp"
#include "ysDui/platform/win32/DuiWindow.hpp"
#include "ysDui/core/DuiDpi.hpp"

namespace ysDui::platform::win32 {
namespace {

/**
 * 贴边诊断日志（临时定位用）：追加写入 %TEMP%\ysDuiEdgeDiag.log
 * 说明：ysDui 为独立 UI 库、不依赖 ysBase 日志，故此处自行落盘；
 *       仅在状态跃迁处调用（非每帧），定位完成后应移除。
 */
void EdgeDiag(const char* format, ...)
{
    wchar_t tempPath[MAX_PATH]{};
    if (::GetTempPathW(MAX_PATH, tempPath) == 0)
        return;
    std::wstring fullPath = tempPath;
    fullPath += L"ysDuiEdgeDiag.log";
    FILE* file = nullptr;
    if (::_wfopen_s(&file, fullPath.c_str(), L"ab") != 0 || file == nullptr)
        return;

    std::time_t now = std::time(nullptr);
    std::tm local{};
    ::localtime_s(&local, &now);
    char stamp[32]{};
    std::strftime(stamp, sizeof(stamp), "%H:%M:%S ", &local);
    std::fputs(stamp, file);

    va_list args;
    va_start(args, format);
    std::vfprintf(file, format, args);
    va_end(args);
    std::fputc('\n', file);
    std::fclose(file);
}

constexpr UINT_PTR kAutoCollapseTimerId = 0x5A11; // 自动折叠轮询定时器 id（宿主窗口内唯一）
constexpr UINT kAutoCollapsePollMs = 50;          // 光标轮询间隔（毫秒）
constexpr UINT_PTR kEdgeHideTimerId = 0x5A12;     // 贴边隐藏定时器 id（轮询 + 动画帧）
constexpr UINT kEdgeHideFrameMs = 16;             // 贴边隐藏帧间隔（约 60fps，兼顾动画平滑与开销）
constexpr int kDockLeft = 1;                      // 贴靠左边
constexpr int kDockRight = 2;                     // 贴靠右边
constexpr int kDockTop = 3;                       // 贴靠上边
constexpr BYTE kLayeredWindowAlpha = 255;         // 分层窗口整体不透明度：255=完全不透明（仅借分层合成）

::HWND OwnerWindow(const ui::HostRef& owner)
{
    auto window = detail::ResolveHost(owner);
    if (window == nullptr || !::IsWindow(window))
        return nullptr;
    if (const auto root = ::GetAncestor(window, GA_ROOT); root != nullptr)
        window = root;
    return window;
}

core::DuiDpiScale ScaleFor(const ui::HostRef& owner)
{
    if (const auto window = detail::ResolveHost(owner); window != nullptr)
        return core::DuiDpiScale(static_cast<int>(::GetDpiForWindow(window)));
    return core::DuiDpiScale(static_cast<int>(::GetDpiForSystem()));
}

core::Rect ScreenAnchor(const ui::DuiPopupOptions& options, const ui::HostRef& owner)
{
    const core::Rect pixels = ScaleFor(owner).Scale(options.anchor);
    ::POINT topLeft{pixels.left, pixels.top};
    ::POINT bottomRight{pixels.right, pixels.bottom};
    if (const auto window = detail::ResolveHost(owner); window != nullptr)
    {
        ::ClientToScreen(window, &topLeft);
        ::ClientToScreen(window, &bottomRight);
    }
    return {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
}

bool FocusMovesToOwnerAtAnchor(const ui::DuiPopupOptions& options,
                               const ui::HostRef& owner, ::HWND nextFocus)
{
    const ::HWND ownerWindow = OwnerWindow(owner);
    if (ownerWindow == nullptr || nextFocus == nullptr
        || (nextFocus != ownerWindow && !::IsChild(ownerWindow, nextFocus)))
        return false;
    ::POINT cursor{};
    if (!::GetCursorPos(&cursor))
        return false;
    return ScreenAnchor(options, owner).Contains({cursor.x, cursor.y});
}

::RECT WorkArea(const ui::DuiPopupOptions& options, const ui::HostRef& owner)
{
    const core::Rect anchor = ScreenAnchor(options, owner);
    ::RECT probe{anchor.left, anchor.top, anchor.right, anchor.bottom};
    ::MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const ::HWND ownerWindow = OwnerWindow(owner);
    const ::HMONITOR monitor = ownerWindow != nullptr
        ? ::MonitorFromWindow(ownerWindow, MONITOR_DEFAULTTONEAREST)
        : ::MonitorFromRect(&probe, MONITOR_DEFAULTTONEAREST);
    ::GetMonitorInfoW(monitor, &monitorInfo);
    return monitorInfo.rcWork;
}

core::Size PopupSize(const ui::DuiPopupOptions& options, const ui::HostRef& owner)
{
    if (options.sizeMode == ui::DuiPopupSizeMode::WorkArea)
    {
        const ::RECT work = WorkArea(options, owner);
        return {static_cast<int>(work.right - work.left), static_cast<int>(work.bottom - work.top)};
    }
    return ScaleFor(owner).Scale(options.size);
}

core::Point PopupPosition(const ui::DuiPopupOptions& options, const ui::HostRef& owner)
{
    const core::Size size = PopupSize(options, owner);
    const ::RECT work = WorkArea(options, owner);
    if (options.sizeMode == ui::DuiPopupSizeMode::WorkArea)
        return {static_cast<int>(work.left), static_cast<int>(work.top)};

    if (options.placement == ui::DuiPopupPlacement::CenterOwner)
    {
        ::RECT target = work;
        if (const ::HWND ownerWindow = OwnerWindow(owner); ownerWindow != nullptr)
            ::GetWindowRect(ownerWindow, &target);
        const int x = static_cast<int>(target.left + (target.right - target.left - size.width) / 2);
        const int y = static_cast<int>(target.top + (target.bottom - target.top - size.height) / 2);
        return {std::clamp(x, static_cast<int>(work.left),
                           (std::max)(static_cast<int>(work.left), static_cast<int>(work.right) - size.width)),
                std::clamp(y, static_cast<int>(work.top),
                           (std::max)(static_cast<int>(work.top), static_cast<int>(work.bottom) - size.height))};
    }

    const core::Rect anchor = ScreenAnchor(options, owner);
    int x = anchor.left;
    int y = anchor.bottom;
    if (options.placement == ui::DuiPopupPlacement::Above)
    {
        y = anchor.top - size.height;
        if (y < work.top && anchor.bottom + size.height <= work.bottom)
            y = anchor.bottom;
    }
    else if (options.placement == ui::DuiPopupPlacement::Right)
    {
        x = anchor.right;
        y = anchor.top;
        if (x + size.width > work.right && anchor.left - size.width >= work.left)
            x = anchor.left - size.width;
    }
    else if (y + size.height > work.bottom && anchor.top - size.height >= work.top)
    {
        y = anchor.top - size.height;
    }

    const int workLeft = static_cast<int>(work.left);
    const int workTop = static_cast<int>(work.top);
    const int workRight = static_cast<int>(work.right);
    const int workBottom = static_cast<int>(work.bottom);
    x = std::clamp(x, workLeft, (std::max)(workLeft, workRight - size.width));
    y = std::clamp(y, workTop, (std::max)(workTop, workBottom - size.height));
    return {x, y};
}

core::Rect PopupBounds(const ui::DuiPopupOptions& options, const ui::HostRef& owner)
{
    const core::Point position = PopupPosition(options, owner);
    const core::Size size = PopupSize(options, owner);
    return {position.x, position.y, position.x + size.width, position.y + size.height};
}
} // namespace

class Win32PopupHost::Impl {
public:
    explicit Impl(ui::HostRef value) : owner(std::move(value)) {}

    [[nodiscard]] core::DuiDpiScale Scale() const
    {
        return window ? window->DpiScale() : ScaleFor(owner);
    }

    void StopFollowingOwner()
    {
        if (followedOwner != nullptr && ::IsWindow(followedOwner))
        {
            ::RemoveWindowSubclass(followedOwner, OwnerProcedure,
                                   reinterpret_cast<::UINT_PTR>(this));
        }
        followedOwner = nullptr;
        hasOwnerBounds = false;
    }

    void UpdateOwnerTracking()
    {
        const ::HWND requestedOwner = options.followOwner ? OwnerWindow(owner) : nullptr;
        if (requestedOwner == followedOwner)
            return;
        StopFollowingOwner();
        if (requestedOwner == nullptr)
            return;

        ::RECT bounds{};
        if (!::GetWindowRect(requestedOwner, &bounds)
            || !::SetWindowSubclass(requestedOwner, OwnerProcedure,
                                    reinterpret_cast<::UINT_PTR>(this),
                                    reinterpret_cast<::DWORD_PTR>(this)))
        {
            return;
        }
        followedOwner = requestedOwner;
        ownerBounds = bounds;
        hasOwnerBounds = true;
    }

    void FollowOwnerMove(::HWND handle)
    {
        if (!visible || handle != followedOwner || ::IsIconic(handle))
            return;
        ::RECT nextOwnerBounds{};
        if (!::GetWindowRect(handle, &nextOwnerBounds))
            return;
        if (!hasOwnerBounds)
        {
            ownerBounds = nextOwnerBounds;
            hasOwnerBounds = true;
            return;
        }
        // 贴边（已收起或已落位待收起）时位置由该边缘决定，不跟随主窗口移动；
        // 但基准须持续更新，否则解贴后首次跟随会按贴边期间的累计位移跳位
        if (edgeDockSide != 0)
        {
            ownerBounds = nextOwnerBounds;
            return;
        }

        const int deltaX = nextOwnerBounds.left - ownerBounds.left;
        const int deltaY = nextOwnerBounds.top - ownerBounds.top;
        ownerBounds = nextOwnerBounds;
        if (deltaX == 0 && deltaY == 0)
            return;
        if (hasRestoredBounds)
        {
            restoredBounds.left += deltaX;
            restoredBounds.right += deltaX;
            restoredBounds.top += deltaY;
            restoredBounds.bottom += deltaY;
        }
        if (options.sizeMode == ui::DuiPopupSizeMode::WorkArea)
        {
            UpdateWindowBounds();
            return;
        }

        const auto popup = window == nullptr
            ? nullptr : reinterpret_cast<::HWND>(window->NativeHandle().value);
        ::RECT popupBounds{};
        if (popup != nullptr && ::GetWindowRect(popup, &popupBounds))
        {
            // 禁止拷贝旧客户区位图：移出屏幕再移回时 CopyBits 会留下残影。
            ::SetWindowPos(popup, nullptr, popupBounds.left + deltaX, popupBounds.top + deltaY,
                           0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
            ::InvalidateRect(popup, nullptr, FALSE);
        }
    }

    static ::LRESULT CALLBACK OwnerProcedure(::HWND handle, ::UINT message, ::WPARAM word,
                                              ::LPARAM data, ::UINT_PTR, ::DWORD_PTR reference)
    {
        auto* state = reinterpret_cast<Impl*>(reference);
        if (message == WM_WINDOWPOSCHANGED)
            state->FollowOwnerMove(handle);
        if (message == WM_NCDESTROY)
        {
            state->followedOwner = nullptr;
            state->hasOwnerBounds = false;
            ::RemoveWindowSubclass(handle, OwnerProcedure,
                                   reinterpret_cast<::UINT_PTR>(state));
        }
        return ::DefSubclassProc(handle, message, word, data);
    }

    void RestoreModalOwner()
    {
        if (modalOwner != nullptr && ownerWasEnabled && ::IsWindow(modalOwner))
        {
            ::EnableWindow(modalOwner, TRUE);
            ::SetForegroundWindow(modalOwner);
        }
        modalOwner = nullptr;
        ownerWasEnabled = false;
    }

    void ApplyModality()
    {
        const ::HWND requestedOwner = options.modality == ui::DuiPopupModality::OwnerModal
            ? OwnerWindow(owner) : nullptr;
        if (requestedOwner == modalOwner)
            return;
        RestoreModalOwner();
        if (requestedOwner != nullptr)
        {
            modalOwner = requestedOwner;
            ownerWasEnabled = ::IsWindowEnabled(modalOwner) != FALSE;
            if (ownerWasEnabled)
                ::EnableWindow(modalOwner, FALSE);
        }
    }

    void LayoutClient() const
    {
        if (!layout || !window)
            return;
        // 不可缩放弹出层（如菜单）：固定用 options.size，避免 owner/窗口 DPI 不一致压窄客户区。
        // 可缩放对话框：必须跟实际客户区，否则拖拽改尺寸后内部不刷新。
        if (!options.resizable && options.sizeMode == ui::DuiPopupSizeMode::Specified
            && options.size.width > 0 && options.size.height > 0)
        {
            layout({0, 0, options.size.width, options.size.height});
            return;
        }
        const auto handle = reinterpret_cast<::HWND>(window->NativeHandle().value);
        ::RECT bounds{};
        if (handle != nullptr && ::GetClientRect(handle, &bounds))
        {
            const core::Size size = Scale().Unscale(core::Size{
                static_cast<int>(bounds.right), static_cast<int>(bounds.bottom)});
            layout({0, 0, size.width, size.height});
        }
    }

    [[nodiscard]] core::Rect NativeBounds() const
    {
        if (!window)
            return {};
        const auto handle = reinterpret_cast<::HWND>(window->NativeHandle().value);
        ::RECT bounds{};
        if (handle == nullptr || !::GetWindowRect(handle, &bounds))
            return {};
        return {bounds.left, bounds.top, bounds.right, bounds.bottom};
    }

    void UpdateWindowBounds(const core::Rect* requestedBounds = nullptr)
    {
        if (!window)
            return;
        // 贴边（收起/展开）期间忽略外部布局请求，改按贴边状态重新落位，
        // 否则 SetOptions / 内容刷新会把窗口拉回屏幕内。
        // 用户正在拖动时必须放行，否则会把窗口拽回原贴边位置（表现为拖不走）。
        if (edgeDockSide != 0 && !edgeAnimActive && !edgeUserMoving)
        {
            ReapplyEdgeBounds();
            LayoutClient();
            return;
        }
        const auto handle = reinterpret_cast<::HWND>(window->NativeHandle().value);
        if (handle == nullptr)
            return;
        ::LONG_PTR style = ::GetWindowLongPtrW(handle, GWL_STYLE);
        style &= ~static_cast<::LONG_PTR>(WS_BORDER);
        style = options.resizable ? style | WS_THICKFRAME : style & ~static_cast<::LONG_PTR>(WS_THICKFRAME);
        ::SetWindowLongPtrW(handle, GWL_STYLE, style);
        core::Rect bounds;
        if (requestedBounds != nullptr)
        {
            bounds = *requestedBounds;
        }
        else if (!options.resizable && options.sizeMode == ui::DuiPopupSizeMode::Specified
                 && options.size.width > 0 && options.size.height > 0)
        {
            // 菜单位置沿用 PopupBounds；宽高按弹出窗口自身 DPI，与固定逻辑尺寸对齐。
            const core::Rect positioned = PopupBounds(options, owner);
            const core::Size physical = Scale().Scale(options.size);
            bounds = {positioned.left, positioned.top, positioned.left + physical.width,
                      positioned.top + physical.height};
        }
        else
        {
            bounds = PopupBounds(options, owner);
        }
        ::SetWindowPos(handle, nullptr, bounds.left, bounds.top, bounds.Width(), bounds.Height(),
                       SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        if (options.chrome)
            ApplySquareFrameCorners(window->NativeHandle(), options.chrome->preferSquareCorners);
        LayoutClient();
    }

    [[nodiscard]] ::LRESULT ChromeHitTest(::HWND handle, ::LPARAM data) const
    {
        if (!options.chrome)
            return HTCLIENT;
        const auto& chrome = *options.chrome;
        const int screenX = static_cast<int>(static_cast<short>(LOWORD(data)));
        const int screenY = static_cast<int>(static_cast<short>(HIWORD(data)));
        ::RECT windowBounds{};
        ::GetWindowRect(handle, &windowBounds);
        const int width = static_cast<int>(windowBounds.right - windowBounds.left);
        const int height = static_cast<int>(windowBounds.bottom - windowBounds.top);
        const int x = screenX - static_cast<int>(windowBounds.left);
        const int y = screenY - static_cast<int>(windowBounds.top);
        const core::DuiDpiScale scale = Scale();
        const int titleHeight = scale.Scale((std::max)(0, chrome.titleBarHeight));
        // 标题栏整段走 HTCLIENT：由 DuiDialog 客户区命中并发起 RequestMove。
        // 若返回 HTCAPTION，Windows 改发 NC 鼠标消息，内容区收不到 Move/Leave，易残留悬停高亮。
        if (options.resizable && options.sizeMode == ui::DuiPopupSizeMode::Specified)
        {
            const int border = scale.Scale((std::max)(0, chrome.resizeBorder));
            const bool left = x < border;
            const bool right = x >= width - border;
            const bool top = y < border;
            const bool bottom = y >= height - border;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top && y < titleHeight) return HTCLIENT; // 标题区优先客户区，勿抢成 HTTOP
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
        }
        return HTCLIENT;
    }

    static ::LRESULT CALLBACK Procedure(::HWND handle, ::UINT message, ::WPARAM word,
                                         ::LPARAM data, ::UINT_PTR, ::DWORD_PTR reference)
    {
        auto* state = reinterpret_cast<Impl*>(reference);
        if (!state->options.chrome)
            return ::DefSubclassProc(handle, message, word, data);
        if (message == WM_TIMER && word == kAutoCollapseTimerId)
        {
            state->PollAutoCollapse();
            return 0;
        }
        if (message == WM_TIMER && word == kEdgeHideTimerId)
        {
            state->PollEdgeHide();
            return 0;
        }
        if (message == WM_ENTERSIZEMOVE)
        {
            // 用户开始拖动/缩放：先置位，确保随后的 SetOptions（如上层持久化尺寸）
            // 不再把窗口拽回原贴边位置
            state->edgeUserMoving = true;
            EdgeDiag("[贴边诊断] WM_ENTERSIZEMOVE side=%d hidden=%d", state->edgeDockSide,
                       state->edgeHidden ? 1 : 0);
            return ::DefSubclassProc(handle, message, word, data);
        }
        if (message == WM_EXITSIZEMOVE)
        {
            EdgeDiag("[贴边诊断] WM_EXITSIZEMOVE side=%d hidden=%d animActive=%d", state->edgeDockSide,
                       state->edgeHidden ? 1 : 0, state->edgeAnimActive ? 1 : 0);
            const ::LRESULT result = ::DefSubclassProc(handle, message, word, data);
            // 拖动结束后判定是否贴边收起（在宿主自身的 WM_EXITSIZEMOVE 上处理）
            state->OnEdgeDragEnd();
            state->LayoutClient();
            ::RedrawWindow(handle, nullptr, nullptr,
                           RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            return result;
        }
        if (message == WM_NCCALCSIZE)
            return 0;        if (message == WM_SIZE)
        {
            const ::LRESULT result = ::DefSubclassProc(handle, message, word, data);
            ApplySquareFrameCorners(state->window->NativeHandle(),
                                    state->options.chrome->preferSquareCorners);
            state->LayoutClient();
            return result;
        }
        if (message == WM_NCHITTEST)
            return state->ChromeHitTest(handle, data);
        // 兼容仍收到 HTCAPTION 的路径：清悬停后交系统拖动。
        if (message == WM_NCLBUTTONDOWN && word == HTCAPTION)
        {
            if (state->host)
            {
                state->host->Dispatch({core::EventType::PointerCancel});
                state->host->Dispatch({core::EventType::PointerLeave});
            }
            ::ReleaseCapture();
            ::SendMessageW(handle, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, data);
            return 0;
        }
        if (message == WM_NCLBUTTONDBLCLK && (word == HTCAPTION || word == HTTOP))
        {
            ::POINT point{static_cast<int>(static_cast<short>(LOWORD(data))),
                          static_cast<int>(static_cast<short>(HIWORD(data)))};
            ::ScreenToClient(handle, &point);
            if (state->host)
                state->host->Dispatch({core::EventType::PointerDoubleClick,
                    state->Scale().Unscale(core::Point{point.x, point.y})});
            ::InvalidateRect(handle, nullptr, FALSE);
            return 0;
        }
        if (message == WM_NCACTIVATE)
            return ::DefWindowProcW(handle, message, word, -1);
        if (message == WM_NCPAINT || message == 0x00AE || message == 0x00AF)
            return 0;
        return ::DefSubclassProc(handle, message, word, data);
    }

    void Dismiss()
    {
        if (!visible)
            return;
        visible = false;
        StopFollowingOwner();
        RestoreModalOwner();
        const auto handler = dismissed;
        if (handler)
            handler();
        if (window)
            window->RequestClose();
    }

    ui::HostRef owner;
    std::shared_ptr<core::Host> host;
    std::unique_ptr<Window> window;
    std::function<void()> dismissed;
    LayoutHandler layout;
    ui::DuiPopupOptions options;
    core::Rect restoredBounds;
    ::HWND modalOwner{};
    ::HWND followedOwner{};
    ::RECT ownerBounds{};
    bool ownerWasEnabled{};
    bool hasRestoredBounds{};
    bool hasOwnerBounds{};
    bool visible{};

    // ---- 自动折叠（光标移出宿主后折叠；平台层负责定时器与光标探测）----
    bool autoCollapse{};
    int autoCollapseExpandDelayMs{200};
    int autoCollapseCollapseDelayMs{250};
    bool autoCollapseState{};       // 目标折叠态（true=折叠）
    bool autoCollapsePending{};     // 待切换（驻留计时中）
    bool autoCollapsePendingState{};// 待切换目标态
    ::ULONGLONG autoCollapseSince{};// 计时起点（GetTickCount64）
    std::function<void(bool)> autoCollapseToggle;

    // ---- 贴边自动隐藏（平台层负责边缘判定、工作区约束与滑动动画）----
    ui::DuiEdgeAutoHideOptions edge{};
    int edgeDockSide{};              // 0=未贴边；1=左 2=右 3=上
    bool edgeHidden{};               // 已收起（只留细边）
    core::Rect edgeShownRect{};      // 收起前位置（滑出时回到此处）
    bool edgeLeavePending{};         // 光标已移出，等待延时到达
    ::ULONGLONG edgeLeaveSince{};
    bool edgeAnimActive{};           // 动画进行中
    bool edgeAnimToHidden{};         // 动画目标：true=收起
    core::Rect edgeAnimFrom{};
    core::Rect edgeAnimTo{};
    ::ULONGLONG edgeAnimStart{};
    int edgeAnimDurationMs{};
    bool edgeUserMoving{};           // 用户正在拖动/缩放窗口（期间不得再落位或收起）
    std::function<void()> edgeHiddenHandler; // 收起为细边时通知上层（如关闭已打开的菜单）
    std::function<void()> edgeHidePrepare;   // 即将贴边收起前的准备（折叠态先展开）

    /** @return 当前选项是否已处于折叠态（仅标题栏高度） */
    [[nodiscard]] bool IsCollapsedByOptions() const
    {
        return options.chrome.has_value()
               && options.sizeMode == ui::DuiPopupSizeMode::Specified
               && options.size.height <= options.chrome->titleBarHeight;
    }

    /** @return 光标当前是否位于宿主窗口（含非客户区）内 */
    [[nodiscard]] bool IsCursorInsideWindow() const
    {
        if (!window)
            return false;
        const auto handle = reinterpret_cast<::HWND>(window->NativeHandle().value);
        if (handle == nullptr || !::IsWindow(handle))
            return false;
        ::POINT cursor{};
        ::RECT rect{};
        if (!::GetCursorPos(&cursor) || !::GetWindowRect(handle, &rect))
            return false;
        return ::PtInRect(&rect, cursor) != FALSE;
    }

    void StopAutoCollapseWatch()
    {
        autoCollapsePending = false;
        if (!window)
            return;
        const auto handle = reinterpret_cast<::HWND>(window->NativeHandle().value);
        if (handle != nullptr && ::IsWindow(handle))
            ::KillTimer(handle, kAutoCollapseTimerId);
    }

    void StartAutoCollapseWatch()
    {
        StopAutoCollapseWatch();
        autoCollapseState = IsCollapsedByOptions();
        if (!window)
            return;
        const auto handle = reinterpret_cast<::HWND>(window->NativeHandle().value);
        if (handle != nullptr && ::IsWindow(handle))
            (void)::SetTimer(handle, kAutoCollapseTimerId, kAutoCollapsePollMs, nullptr);
    }

    /** 轮询光标位置，按驻留延时回调切换折叠态 */
    void PollAutoCollapse()
    {
        if (!autoCollapse || !autoCollapseToggle || !visible)
            return;

        const bool wantCollapse = !IsCursorInsideWindow();
        if (autoCollapseState == wantCollapse)
        {
            autoCollapsePending = false;
            return;
        }

        const ::ULONGLONG now = ::GetTickCount64();
        if (!autoCollapsePending || autoCollapsePendingState != wantCollapse)
        {
            autoCollapsePending = true;
            autoCollapsePendingState = wantCollapse;
            autoCollapseSince = now;
            return;
        }

        const ::ULONGLONG delay = wantCollapse ? static_cast<::ULONGLONG>(autoCollapseCollapseDelayMs)
                                              : static_cast<::ULONGLONG>(autoCollapseExpandDelayMs);
        if (now - autoCollapseSince < delay)
            return;

        autoCollapsePending = false;
        autoCollapseState = wantCollapse;
        autoCollapseToggle(wantCollapse);
    }

    // ---------------- 贴边自动隐藏 ----------------

    /** @return 宿主窗口句柄；无效返回 nullptr */
    [[nodiscard]] ::HWND WindowHandle() const
    {
        if (!window)
            return nullptr;
        const auto handle = reinterpret_cast<::HWND>(window->NativeHandle().value);
        return (handle != nullptr && ::IsWindow(handle)) ? handle : nullptr;
    }

    /** @return 窗口所在显示器的工作区（已排除任务栏） */
    [[nodiscard]] ::RECT MonitorWorkArea() const
    {
        ::RECT work{0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN)};
        const ::HWND handle = WindowHandle();
        if (handle == nullptr)
            return work;
        const ::HMONITOR monitor = ::MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST);
        if (monitor == nullptr)
            return work;
        ::MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (::GetMonitorInfoW(monitor, &info))
            work = info.rcWork;
        return work;
    }

    /**
     * 边缘判定与贴边位置所依据的参考矩形（屏幕坐标）
     * 设置了参考窗口时取其客户区，否则取窗口所在显示器工作区。
     */
    [[nodiscard]] ::RECT EdgeReferenceRect() const
    {
        const auto reference = reinterpret_cast<::HWND>(edge.referenceWindow);
        if (reference != nullptr && ::IsWindow(reference))
        {
            ::RECT client{};
            if (::GetClientRect(reference, &client))
            {
                ::POINT topLeft{client.left, client.top};
                ::POINT bottomRight{client.right, client.bottom};
                if (::ClientToScreen(reference, &topLeft) && ::ClientToScreen(reference, &bottomRight))
                    return {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
            }
        }
        return MonitorWorkArea();
    }

    /**
     * 计算位置到参考区域各边的带符号距离
     * 约定：正值=仍在区域内（距该边多远）；0 或负值=已贴到/越过该边
     * @param pointBased true=用光标位置，false=用窗口矩形
     * @param outLeft 输出：到左边的带符号距离
     * @param outRight 输出：到右边的带符号距离
     * @param outTop 输出：到上边的带符号距离
     * @return 取到有效位置返回 true
     */
    [[nodiscard]] bool DockDistances(bool pointBased, int& outLeft, int& outRight, int& outTop) const
    {
        const ::RECT ref = EdgeReferenceRect();
        if (pointBased)
        {
            ::POINT cursor{};
            if (!::GetCursorPos(&cursor))
                return false;
            outLeft = static_cast<int>(cursor.x) - static_cast<int>(ref.left);
            outRight = static_cast<int>(ref.right) - static_cast<int>(cursor.x);
            outTop = static_cast<int>(cursor.y) - static_cast<int>(ref.top);
            return true;
        }

        ::RECT current{};
        const ::HWND handle = WindowHandle();
        if (handle == nullptr || !::GetWindowRect(handle, &current))
            return false;
        outLeft = static_cast<int>(current.left) - static_cast<int>(ref.left);
        outRight = static_cast<int>(ref.right) - static_cast<int>(current.right);
        outTop = static_cast<int>(current.top) - static_cast<int>(ref.top);
        return true;
    }

    /**
     * 按带符号距离判定贴靠边
     * 说明：采用单侧比较而非绝对值，故「已越过边缘」同样判定为贴边
     *       （拖拽时窗口常会被拖出参考区域，用绝对值会因超调量过大而漏判）。
     * @return 贴靠边；0=未贴边
     */
    [[nodiscard]] int DockSideFromDistances(int dLeft, int dRight, int dTop) const
    {
        const int threshold = (std::max)(1, edge.edgeThreshold);
        int side = 0;
        int best = threshold + 1; // 取最小（即最“越过”的一边）
        if (dLeft <= threshold && dLeft < best) { side = kDockLeft; best = dLeft; }
        if (dRight <= threshold && dRight < best) { side = kDockRight; best = dRight; }
        if (dTop <= threshold && dTop < best) { side = kDockTop; best = dTop; }
        return side;
    }

    /**
     * @return 当前窗口贴靠的边（含已越过边缘）；0=未贴边
     * 支持左、右、上三边：取最近且已贴到/越过的一边。
     */
    [[nodiscard]] int DetectDockSide() const
    {
        int dLeft = 0;
        int dRight = 0;
        int dTop = 0;
        if (!DockDistances(false, dLeft, dRight, dTop))
            return 0;
        return DockSideFromDistances(dLeft, dRight, dTop);
    }

    /**
     * @return 光标所在的贴靠边（含已越过边缘）；0=未贴边
     * 用途：拖动结束时以光标为准判定，兼容窗口被拖出参考区域（越界）的情况。
     */
    [[nodiscard]] int DetectCursorDockSide() const
    {
        int dLeft = 0;
        int dRight = 0;
        int dTop = 0;
        if (!DockDistances(true, dLeft, dRight, dTop))
            return 0;
        return DockSideFromDistances(dLeft, dRight, dTop);
    }

    /** @return 贴边收起后的窗口矩形（仅 stripWidth 可见，且不超出工作区） */
    [[nodiscard]] core::Rect HiddenRectForSide(int side, const core::Rect& shown) const
    {
        const ::RECT ref = EdgeReferenceRect();
        const ::RECT work = MonitorWorkArea();
        const int strip = (std::max)(1, edge.stripWidth);
        const int w = shown.Width();
        const int h = shown.Height();

        // 与参考边平行的方向保留原位置，但钳制在工作区内，
        // 避免露出屏幕边界或压住任务栏（工作区已排除任务栏）
        switch (side)
        {
        case kDockLeft:
        {
            const int maxY = static_cast<int>(work.bottom) - h;
            const int y = std::clamp(shown.top, static_cast<int>(work.top),
                                     (std::max)(static_cast<int>(work.top), maxY));
            const int left = static_cast<int>(ref.left) - (w - strip);
            return {left, y, left + w, y + h};
        }
        case kDockRight:
        {
            const int maxY = static_cast<int>(work.bottom) - h;
            const int y = std::clamp(shown.top, static_cast<int>(work.top),
                                     (std::max)(static_cast<int>(work.top), maxY));
            const int left = static_cast<int>(ref.right) - strip;
            return {left, y, left + w, y + h};
        }
        case kDockTop:
        {
            const int maxX = static_cast<int>(work.right) - w;
            const int x = std::clamp(shown.left, static_cast<int>(work.left),
                                     (std::max)(static_cast<int>(work.left), maxX));
            const int top = static_cast<int>(ref.top) - (h - strip);
            return {x, top, x + w, top + h};
        }
        default:
            return shown;
        }
    }

    /** @return 收起状态下可见的细边矩形（裁剪到参考区域，用于命中判定） */
    [[nodiscard]] ::RECT VisibleStripRect() const
    {
        ::RECT empty{};
        const ::HWND handle = WindowHandle();
        ::RECT current{};
        if (handle == nullptr || !::GetWindowRect(handle, &current))
            return empty;
        const ::RECT ref = EdgeReferenceRect();
        ::RECT strip{};
        if (!::IntersectRect(&strip, &current, &ref))
            return empty;
        return strip;
    }

    void StopEdgeWatch()
    {
        edgeAnimActive = false;
        edgeLeavePending = false;
        if (const ::HWND handle = WindowHandle(); handle != nullptr)
            ::KillTimer(handle, kEdgeHideTimerId);
    }

    void StartEdgeWatch()
    {
        StopEdgeWatch();
        if (const ::HWND handle = WindowHandle(); handle != nullptr)
            (void)::SetTimer(handle, kEdgeHideTimerId, kEdgeHideFrameMs, nullptr);
    }

    /** 把目标位置钳制到工作区内，避免超出屏幕可视区域 */
    [[nodiscard]] core::Rect ClampToWorkArea(const core::Rect& rect) const
    {
        const ::RECT work = MonitorWorkArea();
        const int w = rect.Width();
        const int h = rect.Height();
        const int left = static_cast<int>(work.left);
        const int top = static_cast<int>(work.top);
        const int maxX = (std::max)(left, static_cast<int>(work.right) - w);
        const int maxY = (std::max)(top, static_cast<int>(work.bottom) - h);
        const int x = std::clamp(rect.left, left, maxX);
        const int y = std::clamp(rect.top, top, maxY);
        return {x, y, x + w, y + h};
    }

    /** 开始贴边/滑出动画；时长非正时立即到位 */
    void BeginEdgeAnimation(bool toHidden)
    {
        const ::HWND handle = WindowHandle();
        if (handle == nullptr || edgeDockSide == 0)
            return;
        EdgeDiag("[贴边诊断] BeginEdgeAnimation toHidden=%d side=%d animActive=%d hidden=%d",
                   toHidden ? 1 : 0, edgeDockSide, edgeAnimActive ? 1 : 0, edgeHidden ? 1 : 0);

        const core::Rect from = NativeBounds();
        const core::Rect to = toHidden ? HiddenRectForSide(edgeDockSide, edgeShownRect)
                                       : ClampToWorkArea(edgeShownRect);

        const int duration = toHidden ? edge.hideMs : edge.slideOutMs;
        if (duration <= 0)
        {
            edgeHidden = toHidden;
            edgeLeavePending = false;
            ::SetWindowPos(handle, nullptr, to.left, to.top, to.Width(), to.Height(),
                           SWP_NOZORDER | SWP_NOACTIVATE);
            // 已收起为细边：通知上层关闭可能打开的菜单
            if (toHidden && edgeHiddenHandler)
                edgeHiddenHandler();
            return;
        }
        edgeAnimActive = true;
        edgeAnimToHidden = toHidden;
        edgeAnimFrom = from;
        edgeAnimTo = to;
        edgeAnimStart = ::GetTickCount64();
        edgeAnimDurationMs = duration;
    }

    /** 推进一帧动画；缓动为 ease-out（t*(2-t)） */
    void StepEdgeAnimation()
    {
        const ::HWND handle = WindowHandle();
        if (handle == nullptr)
        {
            edgeAnimActive = false;
            return;
        }
        const ::ULONGLONG now = ::GetTickCount64();
        const double elapsed = static_cast<double>(now - edgeAnimStart);
        double t = edgeAnimDurationMs > 0 ? elapsed / edgeAnimDurationMs : 1.0;
        if (t >= 1.0)
            t = 1.0;
        const double eased = t * (2.0 - t);

        const int left = edgeAnimFrom.left
                         + static_cast<int>((edgeAnimTo.left - edgeAnimFrom.left) * eased);
        const int top = edgeAnimFrom.top
                        + static_cast<int>((edgeAnimTo.top - edgeAnimFrom.top) * eased);
        ::SetWindowPos(handle, nullptr, left, top, edgeAnimTo.Width(), edgeAnimTo.Height(),
                       SWP_NOZORDER | SWP_NOACTIVATE);

        if (t >= 1.0)
        {
            edgeAnimActive = false;
            edgeHidden = edgeAnimToHidden;
            edgeLeavePending = false;
            // 动画结束且已收起为细边：通知上层关闭可能打开的菜单
            if (edgeHidden && edgeHiddenHandler)
                edgeHiddenHandler();
        }
    }

    /** 贴边隐藏轮询：动画推进 / 细边悬停滑出 / 移出延时收起 */
    void PollEdgeHide()
    {
        // 用户拖动期间不介入：否则拖动中光标短暂离开窗口会触发收起，把窗口拽走
        if (edgeUserMoving)
            return;
        if (!edge.enabled || !visible || edgeDockSide == 0)
            return;
        if (edgeAnimActive)
        {
            StepEdgeAnimation();
            return;
        }

        ::POINT cursor{};
        if (!::GetCursorPos(&cursor))
            return;

        if (edgeHidden)
        {
            // 收起态：光标进入细边即滑出
            const ::RECT strip = VisibleStripRect();
            if (::PtInRect(&strip, cursor))
                BeginEdgeAnimation(false);
            return;
        }

        // 展开且已贴边：光标移出后延时收起
        if (IsCursorInsideWindow())
        {
            edgeLeavePending = false;
            return;
        }
        const ::ULONGLONG now = ::GetTickCount64();
        if (!edgeLeavePending)
        {
            edgeLeavePending = true;
            edgeLeaveSince = now;
            return;
        }
        if (now - edgeLeaveSince < static_cast<::ULONGLONG>((std::max)(0, edge.leaveDelayMs)))
            return;
        edgeLeavePending = false;
        BeginEdgeAnimation(true);
    }

    /**
     * 命中边缘则记录贴边方向并立即收起
     * 未命中则清除贴边态但保留窗口当前位置（不得回退到原贴边位置覆盖用户拖动结果）
     * @return 已贴边返回 true
     */
    bool DockToDetectedEdge()
    {
        const ::HWND handle = WindowHandle();
        if (handle == nullptr)
            return false;

        // 折叠态下先展开，再按展开后的窗口落位收起：否则收起后只剩标题栏尺寸
        if (IsCollapsedByOptions() && edgeHidePrepare)
            edgeHidePrepare();

        ::RECT current{};
        if (!::GetWindowRect(handle, &current))
            return false;

        // 优先按光标判定：拖动结束时用户是把光标拖到了边缘/隐藏区，
        // 而窗口本身可能被系统限制在屏幕内或拖出参考区域（越界）
        int side = DetectCursorDockSide();
        const int cursorSide = side;
        if (side == 0)
            side = DetectDockSide();
        const int winSide = side;
        if (side == 0)
        {
            // 离开边缘只清除贴边态、保留用户拖到的位置；
            // 配置中的「贴边隐藏」保持不变（菜单勾选不受影响）
            edgeDockSide = 0;
            edgeHidden = false;
            edgeShownRect = {};
            edgeLeavePending = false;
            return false;
        }

        edgeDockSide = side;
        edgeShownRect = {current.left, current.top, current.right, current.bottom};
        EdgeDiag("[贴边诊断] DockToDetectedEdge side=%d cursorSide=%d winSide=%d shown=(%d,%d,%d,%d)",
                   side, cursorSide, winSide, edgeShownRect.left, edgeShownRect.top,
                   edgeShownRect.right, edgeShownRect.bottom);
        edgeLeavePending = false;
        BeginEdgeAnimation(true);
        return true;
    }

    /** 拖动结束：命中边缘则记录贴边方向并立即收起 */
    void OnEdgeDragEnd()
    {
        edgeUserMoving = false;
        if (!edge.enabled || !visible)
            return;
        EdgeDiag("[贴边诊断] OnEdgeDragEnd enabled=%d visible=%d dockSide=%d hidden=%d",
                   edge.enabled ? 1 : 0, visible ? 1 : 0, edgeDockSide, edgeHidden ? 1 : 0);
        (void)DockToDetectedEdge();
    }

    /** 停用贴边隐藏：清理状态并回到完整可见位置 */
    void RestoreFromEdge()
    {
        const ::HWND handle = WindowHandle();
        const core::Rect restore = edgeShownRect;
        const bool wasHidden = edgeHidden;
        StopEdgeWatch();
        if (handle != nullptr && edgeDockSide != 0 && wasHidden && !restore.Empty())
        {
            const core::Rect target = ClampToWorkArea(restore);
            ::SetWindowPos(handle, nullptr, target.left, target.top, target.Width(),
                           target.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
        }
        edgeDockSide = 0;
        edgeHidden = false;
        edgeShownRect = {};
    }

    /** 贴边（收起或展开）期间忽略外部布局请求，避免被贴回屏幕内 */
    void ReapplyEdgeBounds()
    {
        const ::HWND handle = WindowHandle();
        if (handle == nullptr || edgeDockSide == 0 || edgeAnimActive)
            return;
        const core::Rect target =
            edgeHidden ? HiddenRectForSide(edgeDockSide, edgeShownRect) : edgeShownRect;
        EdgeDiag("[贴边诊断] ReapplyEdgeBounds side=%d hidden=%d target=(%d,%d,%d,%d)",
                   edgeDockSide, edgeHidden ? 1 : 0, target.left, target.top, target.right,
                   target.bottom);
        ::SetWindowPos(handle, nullptr, target.left, target.top, target.Width(), target.Height(),
                       SWP_NOZORDER | SWP_NOACTIVATE);
    }
};

Win32PopupHost::Win32PopupHost(ui::HostRef owner) : impl_(std::make_unique<Impl>(std::move(owner))) {}
Win32PopupHost::~Win32PopupHost() { Hide(); }
Win32PopupHost::Win32PopupHost(Win32PopupHost&&) noexcept = default;
Win32PopupHost& Win32PopupHost::operator=(Win32PopupHost&&) noexcept = default;

bool Win32PopupHost::Show(const ui::DuiPopupOptions& options, std::unique_ptr<core::Control> content,
                          LayoutHandler layout, PaintHandler paint)
{
    if (!content || (options.sizeMode == ui::DuiPopupSizeMode::Specified
                     && (options.size.width <= 0 || options.size.height <= 0)))
        return false;
    Hide();
    impl_->options = options;
    impl_->layout = std::move(layout);
    impl_->host = std::make_shared<core::Host>();
    impl_->host->SetRoot(std::move(content));
    impl_->window = std::make_unique<Window>(impl_->host);
    impl_->window->SetPaintHandler([paint = std::move(paint)](render::Canvas& canvas, core::Rect dirty) {
        if (paint) paint(canvas, dirty);
    });
    Impl* state = impl_.get();
    impl_->window->SetResizeHandler([state](core::Size size) {
        if (!state->layout)
            return;
        // 不可缩放：保持创建时逻辑尺寸；可缩放：跟随 WM_SIZE 客户区。
        if (!state->options.resizable && state->options.sizeMode == ui::DuiPopupSizeMode::Specified
            && state->options.size.width > 0 && state->options.size.height > 0)
        {
            state->layout({0, 0, state->options.size.width, state->options.size.height});
            return;
        }
        state->layout({0, 0, size.width, size.height});
    });
    impl_->window->SetFocusLostHandler([state](NativeWindowHandle nextFocus) {
        if (!state->visible || !state->options.dismissOnFocusLost)
            return;
        const auto popup = reinterpret_cast<::HWND>(state->window->NativeHandle().value);
        const auto next = reinterpret_cast<::HWND>(nextFocus.value);
        if (next != nullptr && (next == popup || ::IsChild(popup, next)
            || ::GetWindow(next, GW_OWNER) == popup))
            return;
        if (FocusMovesToOwnerAtAnchor(state->options, state->owner, next))
            return;
        state->Dismiss();
    });
    impl_->window->SetClosedHandler([state] {
        state->StopFollowingOwner();
        if (!state->visible)
            return;
        state->visible = false;
        state->RestoreModalOwner();
        const auto handler = state->dismissed;
        if (handler)
            handler();
    });
    const core::Rect bounds = PopupBounds(options, impl_->owner);
    const core::DuiDpiScale scale = ScaleFor(impl_->owner);
    const core::Rect logicalBounds = scale.Unscale(bounds);
    WindowOptions windowOptions;
    windowOptions.size = {logicalBounds.Width(), logicalBounds.Height()};
    windowOptions.position = {logicalBounds.left, logicalBounds.top};
    windowOptions.owner = {reinterpret_cast<std::uintptr_t>(OwnerWindow(impl_->owner))};
    windowOptions.kind = WindowOptions::Kind::Popup;
    windowOptions.resizable = options.resizable;
    // 分层窗口的样式须在首次上屏前设置，否则会闪一次不透明画面
    windowOptions.initiallyVisible = !options.layeredWindow;
    if (!impl_->window->Create(windowOptions)) { impl_->window.reset(); impl_->host.reset(); return false; }
    const auto handle = reinterpret_cast<::HWND>(impl_->window->NativeHandle().value);
    if (handle == nullptr)
    {
        impl_->window.reset();
        impl_->host.reset();
        return false;
    }
    if (options.layeredWindow)
    {
        // 分层窗口由 DWM 合成：移动（含贴边自动隐藏的滑动）不再逐帧强制重绘下层窗口，
        // 避免下层为 NX 绘图区等重绘代价高的窗口时动画掉帧
        const ::LONG_PTR exStyle = ::GetWindowLongPtrW(handle, GWL_EXSTYLE);
        ::SetWindowLongPtrW(handle, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        ::SetLayeredWindowAttributes(handle, 0, kLayeredWindowAlpha, LWA_ALPHA);
        ::ShowWindow(handle, SW_SHOW);
    }
    ::SetWindowSubclass(handle, Impl::Procedure, 1, reinterpret_cast<::DWORD_PTR>(impl_.get()));
    impl_->restoredBounds = bounds;
    impl_->hasRestoredBounds = true;
    impl_->visible = true;
    // 每次上屏重置贴边状态，并（在启用时）启动贴边隐藏轮询
    impl_->edgeDockSide = 0;
    impl_->edgeHidden = false;
    impl_->edgeShownRect = {};
    impl_->edgeAnimActive = false;
    impl_->edgeLeavePending = false;
    if (impl_->edge.enabled)
        impl_->StartEdgeWatch();
    impl_->ApplyModality();
    impl_->UpdateWindowBounds();
    impl_->UpdateOwnerTracking();
    return true;
}

void Win32PopupHost::SetOptions(const ui::DuiPopupOptions& options)
{
    if (options.sizeMode == ui::DuiPopupSizeMode::Specified
        && (options.size.width <= 0 || options.size.height <= 0))
        return;
    if (!impl_->visible)
    {
        impl_->options = options;
        return;
    }
    const ui::DuiPopupOptions previous = impl_->options;
    const core::Rect currentBounds = impl_->NativeBounds();
    const core::DuiDpiScale scale = impl_->Scale();
    const bool wasCollapsed = previous.chrome && previous.sizeMode == ui::DuiPopupSizeMode::Specified
        && currentBounds.Height() <= scale.Scale(previous.chrome->titleBarHeight);
    const bool willCollapse = options.chrome && options.sizeMode == ui::DuiPopupSizeMode::Specified
        && options.size.height <= options.chrome->titleBarHeight;
    if (previous.chrome && previous.sizeMode == ui::DuiPopupSizeMode::Specified
        && (willCollapse || options.sizeMode == ui::DuiPopupSizeMode::WorkArea))
    {
        impl_->restoredBounds = currentBounds;
        impl_->hasRestoredBounds = !currentBounds.Empty();
    }
    core::Rect requestedBounds;
    const core::Rect* requested{};
    if (options.chrome && options.sizeMode == ui::DuiPopupSizeMode::Specified)
    {
        if ((previous.sizeMode == ui::DuiPopupSizeMode::WorkArea || wasCollapsed) && impl_->hasRestoredBounds)
        {
            requestedBounds = impl_->restoredBounds;
            // 折叠态拖动后展开：保留当前屏幕位置，只用记忆的展开尺寸。
            if (wasCollapsed && !currentBounds.Empty())
            {
                const int width = impl_->restoredBounds.Width();
                const int height = impl_->restoredBounds.Height();
                requestedBounds = {currentBounds.left, currentBounds.top,
                                   currentBounds.left + width, currentBounds.top + height};
                impl_->restoredBounds = requestedBounds;
            }
        }
        else if (willCollapse && !currentBounds.Empty())
        {
            requestedBounds = {currentBounds.left, currentBounds.top,
                               currentBounds.right,
                               currentBounds.top + scale.Scale(options.size.height)};
        }
        else if (!currentBounds.Empty())
        {
            requestedBounds = {currentBounds.left, currentBounds.top,
                               currentBounds.left + scale.Scale(options.size.width),
                               currentBounds.top + scale.Scale(options.size.height)};
        }
        requested = &requestedBounds;
    }
    impl_->options = options;
    impl_->ApplyModality();
    // 无 chrome 时也保留当前位置，仅按 options.size 更新尺寸（避免回弹到初始锚点）。
    if (requested == nullptr && options.sizeMode == ui::DuiPopupSizeMode::Specified
        && !currentBounds.Empty() && options.size.width > 0 && options.size.height > 0)
    {
        requestedBounds = {currentBounds.left, currentBounds.top,
                           currentBounds.left + scale.Scale(options.size.width),
                           currentBounds.top + scale.Scale(options.size.height)};
        requested = &requestedBounds;
    }
    impl_->UpdateWindowBounds(requested);
    impl_->UpdateOwnerTracking();
    // 折叠态由选项推导：调用方切换后需同步，避免自动折叠重复回调
    if (impl_->autoCollapse)
        impl_->autoCollapseState = impl_->IsCollapsedByOptions();
}

void Win32PopupHost::SetAutoCollapse(bool enabled, int expandDelayMs, int collapseDelayMs,
                                     bool collapseNow, std::function<void(bool)> toggle)
{
    if (!impl_)
        return;
    impl_->autoCollapse = enabled && static_cast<bool>(toggle);
    impl_->autoCollapseExpandDelayMs = expandDelayMs > 0 ? expandDelayMs : 0;
    impl_->autoCollapseCollapseDelayMs = collapseDelayMs > 0 ? collapseDelayMs : 0;
    impl_->autoCollapseToggle = impl_->autoCollapse ? std::move(toggle) : nullptr;
    impl_->autoCollapseState = collapseNow;
    if (!impl_->autoCollapse || !impl_->visible)
    {
        impl_->StopAutoCollapseWatch();
        return;
    }
    impl_->StartAutoCollapseWatch();
}

void Win32PopupHost::SetEdgeAutoHide(const ui::DuiEdgeAutoHideOptions& options)
{
    if (!impl_)
        return;
    const bool wasEnabled = impl_->edge.enabled;
    impl_->edge = options;
    if (!options.enabled)
    {
        if (wasEnabled)
            impl_->RestoreFromEdge();
        else
            impl_->StopEdgeWatch();
        return;
    }
    if (impl_->visible)
        impl_->StartEdgeWatch();
}

void Win32PopupHost::TryDockEdgeNow()
{
    if (!impl_ || !impl_->edge.enabled || !impl_->visible)
        return;
    // 已贴边或已收起时无需重复处理
    if (impl_->edgeDockSide != 0 || impl_->edgeHidden)
        return;
    (void)impl_->DockToDetectedEdge();
}

void Win32PopupHost::SetEdgeHiddenHandler(std::function<void()> handler)
{
    if (impl_)
        impl_->edgeHiddenHandler = std::move(handler);
}

bool Win32PopupHost::EdgeDocked() const
{
    return impl_ != nullptr && impl_->edgeDockSide != 0;
}

void Win32PopupHost::SetEdgeHidePrepareHandler(std::function<void()> handler)
{
    if (impl_)
        impl_->edgeHidePrepare = std::move(handler);
}

void Win32PopupHost::Hide()
{
    if (!impl_) return;
    impl_->StopAutoCollapseWatch();
    impl_->StopEdgeWatch();
    impl_->autoCollapseToggle = nullptr;
    impl_->visible = false;
    impl_->StopFollowingOwner();
    impl_->RestoreModalOwner();
    if (impl_->window)
    {
        const auto handle = reinterpret_cast<::HWND>(impl_->window->NativeHandle().value);
        if (handle != nullptr)
            ::RemoveWindowSubclass(handle, Impl::Procedure, 1);
        impl_->window->Close();
    }
    impl_->window.reset();
    impl_->host.reset();
    impl_->layout = {};
}

void Win32PopupHost::RequestHide()
{
    if (!impl_) return;
    impl_->StopAutoCollapseWatch();
    impl_->StopEdgeWatch();
    impl_->autoCollapseToggle = nullptr;
    impl_->visible = false;
    impl_->StopFollowingOwner();
    impl_->RestoreModalOwner();
    if (impl_->window) impl_->window->RequestClose();
}

void Win32PopupHost::RequestMinimize()
{
    if (!impl_ || !impl_->window)
        return;
    const auto handle = reinterpret_cast<::HWND>(impl_->window->NativeHandle().value);
    if (handle != nullptr)
        ::ShowWindow(handle, SW_MINIMIZE);
}

void Win32PopupHost::RequestMove()
{
    if (!impl_ || !impl_->window)
        return;
    const auto handle = reinterpret_cast<::HWND>(impl_->window->NativeHandle().value);
    if (handle == nullptr)
        return;
    // 先清悬停；PostMessage 让 SC_MOVE 在 LBUTTONDOWN 返回后再跑，避免与 SetCapture 打架。
    if (impl_->host)
    {
        impl_->host->Dispatch({core::EventType::PointerCancel});
        impl_->host->Dispatch({core::EventType::PointerLeave});
    }
    ::ReleaseCapture();
    ::PostMessageW(handle, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}

bool Win32PopupHost::Visible() const { return impl_ && impl_->visible; }
ui::HostRef Win32PopupHost::Reference() const
{
    return impl_ && impl_->window ? impl_->window->Reference() : ui::HostRef{};
}
std::uintptr_t Win32PopupHost::NativeHandle() const
{
    return impl_ && impl_->window ? impl_->window->NativeHandle().value : 0;
}
void Win32PopupHost::SetDismissedHandler(std::function<void()> handler) { impl_->dismissed = std::move(handler); }

} // namespace ysDui::platform::win32
