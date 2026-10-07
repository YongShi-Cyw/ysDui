#define NOMINMAX
#include <windows.h>

#include <cassert>
#include <cstdint>
#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/platform/win32/DuiWin32HostRefAdapter.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"
#include "ysDui/platform/win32/DuiWin32PopupHost.hpp"

#include "DuiWin32HostRef.hpp"

namespace {
class DoubleClickControl final : public ysDui::core::Control
{
public:
    bool OnEvent(const ysDui::core::Event& event) override
    {
        if (event.type != ysDui::core::EventType::PointerDoubleClick)
            return false;
        ++doubleClickCount;
        lastPosition = event.position;
        return true;
    }

    int doubleClickCount{};
    ysDui::core::Point lastPosition;
};

::LRESULT HitTestAt(::HWND window, int x, int y)
{
    ::POINT point{x, y};
    assert(::ClientToScreen(window, &point));
    return ::SendMessageW(window, WM_NCHITTEST, 0, MAKELPARAM(point.x, point.y));
}

::LPARAM ScreenPointAt(::HWND window, int x, int y)
{
    ::POINT point{x, y};
    assert(::ClientToScreen(window, &point));
    return MAKELPARAM(point.x, point.y);
}
}

int main()
{
    ::HWND parent = ::CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                       100, 100, 240, 160, nullptr, nullptr,
                                       ::GetModuleHandleW(nullptr), nullptr);
    assert(parent != nullptr);
    ysDui::platform::win32::Win32HostRefAdapter parentAdapter(
        {reinterpret_cast<std::uintptr_t>(parent)});
    ysDui::platform::win32::Win32PopupHost popup(parentAdapter.Reference());
    ysDui::ui::DuiPopupOptions options;
    options.anchor = {20, 60, 50, 90};
    options.size = {80, 40};
    const struct PlacementCase final
    {
        ysDui::ui::DuiPopupPlacement placement;
        ::POINT expected;
    } cases[]{{ysDui::ui::DuiPopupPlacement::Below, {options.anchor.left, options.anchor.bottom}},
              {ysDui::ui::DuiPopupPlacement::Above,
               {options.anchor.left, options.anchor.top - options.size.height}},
              {ysDui::ui::DuiPopupPlacement::Right, {options.anchor.right, options.anchor.top}}};
    for (const PlacementCase& placementCase : cases)
    {
        options.placement = placementCase.placement;
        auto currentContent = std::make_unique<ysDui::core::Control>();
        ysDui::core::Control* currentRaw = currentContent.get();
        assert(popup.Show(options, std::move(currentContent),
                          [currentRaw](ysDui::core::Rect bounds) { currentRaw->SetBounds(bounds); }, {}));
        assert(popup.Visible());
        ::POINT expectedPosition = placementCase.expected;
        assert(::ClientToScreen(parent, &expectedPosition));
        const auto popupWindow = ysDui::platform::win32::detail::ResolveHost(popup.Reference());
        ::RECT popupBounds{};
        assert(popupWindow != nullptr && ::GetWindowRect(popupWindow, &popupBounds));
        assert(popupBounds.left == expectedPosition.x && popupBounds.top == expectedPosition.y);
        popup.RequestHide();
        assert(!popup.Visible());
    }

    options.placement = ysDui::ui::DuiPopupPlacement::Below;
    options.dismissOnFocusLost = true;
    options.anchor = {-10000, -10000, -9990, -9990};
    auto dismissibleContent = std::make_unique<ysDui::core::Control>();
    assert(popup.Show(options, std::move(dismissibleContent), {}, {}));
    const auto dismissibleWindow = ysDui::platform::win32::detail::ResolveHost(popup.Reference());
    assert(dismissibleWindow != nullptr);
    assert((::GetWindowLongPtrW(dismissibleWindow, GWL_STYLE) & WS_BORDER) == 0);
    ::SetFocus(dismissibleWindow);
    assert(::GetFocus() == dismissibleWindow);
    ::SetFocus(parent);
    assert(!popup.Visible());

    options.placement = ysDui::ui::DuiPopupPlacement::CenterOwner;
    options.size = {320, 180};
    options.dismissOnFocusLost = false;
    options.modality = ysDui::ui::DuiPopupModality::OwnerModal;
    options.resizable = true;
    options.followOwner = true;
    options.chrome = ysDui::ui::DuiPopupChrome{24, 8, 24, 72, true};
    ysDui::core::Rect layoutBounds;
    int paintCount{};
    auto doubleClickContent = std::make_unique<DoubleClickControl>();
    DoubleClickControl* doubleClickRaw = doubleClickContent.get();
    assert(popup.Show(options, std::move(doubleClickContent),
                      [&layoutBounds, doubleClickRaw](ysDui::core::Rect bounds) {
                          layoutBounds = bounds;
                          doubleClickRaw->SetBounds(bounds);
                      }, [&paintCount](ysDui::render::Canvas&, ysDui::core::Rect) {
                          ++paintCount;
                      }));
    const auto modalWindow = ysDui::platform::win32::detail::ResolveHost(popup.Reference());
    ::RECT ownerBounds{};
    ::RECT modalBounds{};
    assert(modalWindow != nullptr && ::GetWindowRect(parent, &ownerBounds) && ::GetWindowRect(modalWindow, &modalBounds));
    assert(modalBounds.left == ownerBounds.left + (ownerBounds.right - ownerBounds.left - options.size.width) / 2);
    assert(modalBounds.top == ownerBounds.top + (ownerBounds.bottom - ownerBounds.top - options.size.height) / 2);
    constexpr int OwnerMoveX = 31;
    constexpr int OwnerMoveY = 19;
    assert(::SetWindowPos(parent, nullptr, ownerBounds.left + OwnerMoveX, ownerBounds.top + OwnerMoveY,
                          0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    assert(::GetWindowRect(modalWindow, &modalBounds));
    assert(modalBounds.left == ownerBounds.left
        + (ownerBounds.right - ownerBounds.left - options.size.width) / 2 + OwnerMoveX);
    assert(modalBounds.top == ownerBounds.top
        + (ownerBounds.bottom - ownerBounds.top - options.size.height) / 2 + OwnerMoveY);
    // 跟随 owner 移出屏幕再移回：须触发重绘，避免 CopyBits 残影。
    const int paintCountBeforeOffscreenFollow = paintCount;
    assert(::SetWindowPos(parent, nullptr, -2000, -2000, 0, 0,
                          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    assert(::SetWindowPos(parent, nullptr, ownerBounds.left + OwnerMoveX,
                          ownerBounds.top + OwnerMoveY, 0, 0,
                          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    assert(::RedrawWindow(modalWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW));
    assert(paintCount > paintCountBeforeOffscreenFollow);
    options.followOwner = false;
    popup.SetOptions(options);
    const ::RECT stationaryBounds = modalBounds;
    assert(::SetWindowPos(parent, nullptr, ownerBounds.left + OwnerMoveX * 2,
                          ownerBounds.top + OwnerMoveY * 2,
                          0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    assert(::GetWindowRect(modalWindow, &modalBounds));
    assert(::EqualRect(&modalBounds, &stationaryBounds));
    assert(!::IsWindowEnabled(parent));
    assert((::GetWindowLongPtrW(modalWindow, GWL_STYLE) & WS_THICKFRAME) != 0);
    assert((::GetClassLongPtrW(modalWindow, GCL_STYLE) & CS_DBLCLKS) != 0);
    assert(layoutBounds.Width() > 0 && layoutBounds.Height() > 0);
    ::RECT modalClient{};
    assert(::GetClientRect(modalWindow, &modalClient));
    assert(modalClient.right == modalBounds.right - modalBounds.left);
    assert(modalClient.bottom == modalBounds.bottom - modalBounds.top);
    assert(HitTestAt(modalWindow, 80, 12) == HTCLIENT);
    assert(HitTestAt(modalWindow, 80, 2) == HTCLIENT);
    assert(HitTestAt(modalWindow, 12, 12) == HTCLIENT);
    assert(HitTestAt(modalWindow, 300, 12) == HTCLIENT);
    assert(HitTestAt(modalWindow, 2, 2) == HTTOPLEFT);
    constexpr int StaleLayoutWidth = 520;
    constexpr int StaleLayoutHeight = 360;
    ::SendMessageW(modalWindow, WM_SIZE, SIZE_RESTORED,
                   MAKELPARAM(StaleLayoutWidth, StaleLayoutHeight));
    assert(layoutBounds.Width() == modalClient.right && layoutBounds.Height() == modalClient.bottom);
    const int paintCountBeforeMoveFinished = paintCount;
    ::SendMessageW(modalWindow, WM_EXITSIZEMOVE, 0, 0);
    assert(layoutBounds.Width() == modalClient.right && layoutBounds.Height() == modalClient.bottom);
    assert(paintCount > paintCountBeforeMoveFinished);
    ::SendMessageW(modalWindow, WM_LBUTTONDBLCLK, 0, MAKELPARAM(80, 12));
    assert(doubleClickRaw->doubleClickCount == 1);
    assert((doubleClickRaw->lastPosition == ysDui::core::Point{80, 12}));

    const ::RECT expandedBounds = modalBounds;
    options.size.height = 24;
    options.resizable = false;
    popup.SetOptions(options);
    assert(::GetWindowRect(modalWindow, &modalBounds));
    assert(modalBounds.left == expandedBounds.left && modalBounds.top == expandedBounds.top);
    assert(modalBounds.right - modalBounds.left == 320 && modalBounds.bottom - modalBounds.top == 24);
    options.size = {320, 180};
    options.resizable = true;
    popup.SetOptions(options);
    assert(::GetWindowRect(modalWindow, &modalBounds));
    assert(modalBounds.left == expandedBounds.left && modalBounds.top == expandedBounds.top);
    assert(modalBounds.right == expandedBounds.right && modalBounds.bottom == expandedBounds.bottom);

    options.size = {120, 70};
    options.resizable = false;
    popup.SetOptions(options);
    assert(::GetWindowRect(modalWindow, &modalBounds));
    assert(modalBounds.right - modalBounds.left == 120 && modalBounds.bottom - modalBounds.top == 70);
    assert((::GetWindowLongPtrW(modalWindow, GWL_STYLE) & WS_THICKFRAME) == 0);
    const ::RECT specifiedBounds = modalBounds;

    ::MONITORINFO modalMonitor{};
    modalMonitor.cbSize = sizeof(modalMonitor);
    assert(::GetMonitorInfoW(::MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST), &modalMonitor));
    options.sizeMode = ysDui::ui::DuiPopupSizeMode::WorkArea;
    options.resizable = true;
    popup.SetOptions(options);
    assert(::GetWindowRect(modalWindow, &modalBounds));
    assert(modalBounds.left == modalMonitor.rcWork.left && modalBounds.top == modalMonitor.rcWork.top);
    assert(modalBounds.right == modalMonitor.rcWork.right && modalBounds.bottom == modalMonitor.rcWork.bottom);

    options.sizeMode = ysDui::ui::DuiPopupSizeMode::Specified;
    options.modality = ysDui::ui::DuiPopupModality::Modeless;
    popup.SetOptions(options);
    assert(::GetWindowRect(modalWindow, &modalBounds));
    assert(modalBounds.left == specifiedBounds.left && modalBounds.top == specifiedBounds.top);
    assert(modalBounds.right == specifiedBounds.right && modalBounds.bottom == specifiedBounds.bottom);
    assert(::IsWindowEnabled(parent));
    options.modality = ysDui::ui::DuiPopupModality::OwnerModal;
    popup.SetOptions(options);
    assert(!::IsWindowEnabled(parent));
    popup.RequestHide();
    assert(::IsWindowEnabled(parent));
    popup.Hide();

    ::MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    assert(::GetMonitorInfoW(::MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST), &monitorInfo));
    ysDui::platform::win32::Win32PopupHost edgePopup({});
    options.anchor = {monitorInfo.rcWork.left + 20, monitorInfo.rcWork.bottom - 10,
                      monitorInfo.rcWork.left + 50, monitorInfo.rcWork.bottom - 1};
    options.placement = ysDui::ui::DuiPopupPlacement::Below;
    auto edgeContent = std::make_unique<ysDui::core::Control>();
    assert(edgePopup.Show(options, std::move(edgeContent), {}, {}));
    ::RECT edgeBounds{};
    assert(::GetWindowRect(ysDui::platform::win32::detail::ResolveHost(edgePopup.Reference()), &edgeBounds));
    assert(edgeBounds.left == options.anchor.left
        && edgeBounds.top == options.anchor.top - options.size.height);
    edgePopup.Hide();
    ::DestroyWindow(parent);
}
