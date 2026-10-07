#define NOMINMAX
#include <windows.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "ysDui/controls/list/DuiSpreadsheet.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/controls/window/DuiDialog.hpp"
#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/platform/win32/DuiWin32Application.hpp"
#include "ysDui/platform/win32/DuiWin32HostRefAdapter.hpp"
#include "ysDui/platform/win32/DuiWin32ModalDialog.hpp"
#include "ysDui/platform/win32/DuiWin32NativeViewRef.hpp"
#include "ysDui/platform/win32/DuiWin32TextInput.hpp"
#include "ysDui/platform/win32/DuiWin32UiHostFactory.hpp"
#include "ysDui/platform/win32/DuiWindow.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"

#include "DuiWin32NativeViewRef.hpp"
#include "DuiHostRefAccess.hpp"

namespace {
constexpr ::WPARAM AnimationTimerId = 0xD001;

class OtherHostBinding final : public ysDui::ui::detail::HostBinding
{
public:
    [[nodiscard]] bool Valid() const override { return true; }
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

/**
 * 查找属于指定 owner 的顶层 ysDui 窗口。
 * 说明：`FindWindowW(类名, nullptr)` 返回的是**全系统**首个同类窗口，若同进程或其它进程
 *       存在其它同类窗口（弹窗宿主/框架宿主/其它测试用例残留）就会取错对象，导致断言随
 *       机器状态随机失败。这里改为按 owner 精确匹配，只依赖被测行为本身。
 */
::HWND FindOwnedWindow(::HWND owner)
{
    for (::HWND window = ::FindWindowW(L"ysDui.v2.Window", nullptr); window != nullptr;
         window = ::FindWindowExW(nullptr, window, L"ysDui.v2.Window", nullptr))
    {
        if (::GetWindow(window, GW_OWNER) == owner)
            return window;
    }
    return nullptr;
}

bool SameBounds(const ::RECT& left, const ::RECT& right)
{
    return left.left == right.left && left.top == right.top
        && left.right == right.right && left.bottom == right.bottom;
}

struct GuiResources final
{
    DWORD gdi{};
    DWORD user{};
};

/**
 * 两次采样间允许的资源增长。
 * 资源计数存在固有噪声：GDI 批处理、USER 对象的延迟回收、OS 首次使用的惰性分配
 * 都会带来 ±1~2 的抖动，容差取 1 会低于噪声下限而产生假失败。
 * 采样相隔 100 次创建/销毁循环，每循环泄漏 1 个对象即累积约 100，
 * 因此 4 仍远低于真实泄漏信号，泄漏检测能力不受影响。
 */
constexpr DWORD kResourceTolerance = 4;

GuiResources ReadGuiResources()
{
    // GDI 批处理会延迟对象销毁，使 GR_GDIOBJECTS 在采样点出现与泄漏无关的跳变。
    // 采样前 flush，保证计数反映真实存活对象；批处理本身在压力测试入口关闭。
    // 注意：不在此抽干消息队列——那是进程级副作用，可能干扰同进程后续窗口测试。
    ::GdiFlush();
    const ::HANDLE process = ::GetCurrentProcess();
    return {::GetGuiResources(process, GR_GDIOBJECTS), ::GetGuiResources(process, GR_USEROBJECTS)};
}

class SpreadsheetStressModel final : public ysDui::controls::list::IDuiWorkbookModel
{
public:
    [[nodiscard]] int WorksheetCount() const override { return 1; }
    [[nodiscard]] ysDui::controls::list::DuiWorksheetId WorksheetIdAt(int) const override { return 1; }
    [[nodiscard]] std::string WorksheetName(ysDui::controls::list::DuiWorksheetId) const override
    {
        return "Stress";
    }
    bool RenameWorksheet(ysDui::controls::list::DuiWorksheetId, std::string) override { return true; }
    [[nodiscard]] ysDui::controls::list::DuiWorksheetId AddWorksheet(std::string) override { return 0; }
    bool RemoveWorksheet(ysDui::controls::list::DuiWorksheetId) override { return false; }
    [[nodiscard]] int RowCount(ysDui::controls::list::DuiWorksheetId) const override { return 100; }
    [[nodiscard]] int ColumnCount(ysDui::controls::list::DuiWorksheetId) const override { return 20; }
    [[nodiscard]] std::string CellText(ysDui::controls::list::DuiWorksheetId,
                                       ysDui::controls::list::DuiCellAddress cell) const override
    {
        return cell == ysDui::controls::list::DuiCellAddress{1, 1} ? editedText : std::string{};
    }
    bool SetCellText(ysDui::controls::list::DuiWorksheetId,
                     ysDui::controls::list::DuiCellAddress cell, std::string text) override
    {
        if (cell != ysDui::controls::list::DuiCellAddress{1, 1})
            return false;
        editedText = std::move(text);
        ++writeCount;
        return true;
    }
    [[nodiscard]] int RowHeight(ysDui::controls::list::DuiWorksheetId, int) const override { return 24; }
    [[nodiscard]] int ColumnWidth(ysDui::controls::list::DuiWorksheetId, int) const override { return 96; }
    bool SetRowHeight(ysDui::controls::list::DuiWorksheetId, int, int) override { return true; }
    bool SetColumnWidth(ysDui::controls::list::DuiWorksheetId, int, int) override { return true; }

    std::string editedText;
    int writeCount{};
};

void VerifySpreadsheetDirectTextInput()
{
    SpreadsheetStressModel model;
    auto host = std::make_shared<ysDui::core::Host>();
    auto spreadsheet = std::make_unique<ysDui::controls::list::DuiSpreadsheet>();
    spreadsheet->SetWorkbookModel(&model);
    auto* const rawSpreadsheet = spreadsheet.get();
    host->SetRoot(std::move(spreadsheet));

    ysDui::platform::win32::Window window(host);
    window.SetResizeHandler([rawSpreadsheet](ysDui::core::Size size)
    {
        rawSpreadsheet->Layout({0, 0, size.width, size.height});
    });
    ysDui::platform::win32::WindowOptions options;
    options.title = "Spreadsheet direct input";
    options.kind = ysDui::platform::win32::WindowOptions::Kind::Popup;
    options.size = {360, 260};
    assert(window.Create(options));

    ysDui::platform::win32::Win32TextInput input(window.NativeHandle());
    rawSpreadsheet->SetTextInput(&input);
    rawSpreadsheet->SetActiveCell({1, 1});
    ysDui::core::Event textEvent;
    textEvent.type = ysDui::core::EventType::TextInput;
    textEvent.text = "1";
    assert(rawSpreadsheet->OnEvent(textEvent));
    assert(rawSpreadsheet->Editing() && input.Text() == "1");

    const ::HWND edit = ::GetFocus();
    assert(edit != nullptr && edit != reinterpret_cast<::HWND>(window.NativeHandle().value));
    ::SendMessageW(edit, WM_CHAR, L'2', 0);
    ::SendMessageW(edit, WM_CHAR, L'3', 0);
    assert(input.Text() == "123");
    rawSpreadsheet->CommitEdit();
    assert(model.editedText == "123");

    rawSpreadsheet->SetTextInput(nullptr);
    window.Close();
    host->SetRoot({});
}

void VerifySpreadsheetWindowStress()
{
    // 关闭 GDI 批处理：默认批处理会攒够一定数量才真正销毁 GDI 对象，
    // 使 211 次创建/销毁循环中的三处采样互相差出多个对象，产生与泄漏无关的抖动。
    ::GdiSetBatchLimit(1);
    GuiResources before;
    GuiResources middle;
    for (int iteration = 0; iteration <= 210; ++iteration)
    {
        SpreadsheetStressModel model;
        auto host = std::make_shared<ysDui::core::Host>();
        auto spreadsheet = std::make_unique<ysDui::controls::list::DuiSpreadsheet>();
        spreadsheet->SetWorkbookModel(&model);
        auto* const rawSpreadsheet = spreadsheet.get();
        host->SetRoot(std::move(spreadsheet));

        ysDui::platform::win32::Window window(host);
        window.SetResizeHandler([rawSpreadsheet](ysDui::core::Size size)
        {
            rawSpreadsheet->Layout({0, 0, size.width, size.height});
        });
        window.SetPaintHandler([rawSpreadsheet](ysDui::render::Canvas& canvas, ysDui::core::Rect dirty)
        {
            rawSpreadsheet->Paint(canvas, dirty);
        });
        ysDui::platform::win32::WindowOptions options;
        options.title = "Spreadsheet stress";
        options.kind = ysDui::platform::win32::WindowOptions::Kind::Popup;
        options.size = {360, 260};
        assert(window.Create(options));
        const auto handle = window.NativeHandle();
        const auto nativeHandle = reinterpret_cast<::HWND>(handle.value);
        assert(nativeHandle != nullptr);

        ysDui::platform::win32::Win32TextInput input(handle);
        rawSpreadsheet->SetTextInput(&input);
        rawSpreadsheet->SetActiveCell({1, 1});
        assert(rawSpreadsheet->BeginEdit());
        input.SetText("stress value");
        rawSpreadsheet->CommitEdit();
        assert(model.writeCount == 1 && model.editedText == "stress value");

        ysDui::core::Event wheel;
        wheel.type = ysDui::core::EventType::PointerWheel;
        wheel.position = {180, 130};
        wheel.wheelDelta = -120;
        assert(host->Dispatch(wheel));
        assert(::RedrawWindow(nativeHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW));
        rawSpreadsheet->SetTextInput(nullptr);
        window.Close();
        assert(!::IsWindow(nativeHandle));
        host->SetRoot({});
        if (iteration == 10)
            before = ReadGuiResources();
        if (iteration == 110)
            middle = ReadGuiResources();
    }
    const GuiResources after = ReadGuiResources();
    assert(middle.gdi <= before.gdi + kResourceTolerance
        && middle.user <= before.user + kResourceTolerance);
    assert(after.gdi <= middle.gdi + kResourceTolerance
        && after.user <= middle.user + kResourceTolerance);
}

void VerifySpreadsheetWindowDpiChange()
{
    SpreadsheetStressModel model;
    auto host = std::make_shared<ysDui::core::Host>();
    auto spreadsheet = std::make_unique<ysDui::controls::list::DuiSpreadsheet>();
    spreadsheet->SetWorkbookModel(&model);
    auto* const rawSpreadsheet = spreadsheet.get();
    host->SetRoot(std::move(spreadsheet));

    ysDui::platform::win32::Window window(host);
    window.SetResizeHandler([rawSpreadsheet](ysDui::core::Size size)
    {
        rawSpreadsheet->Layout({0, 0, size.width, size.height});
    });
    window.SetPaintHandler([rawSpreadsheet](ysDui::render::Canvas& canvas, ysDui::core::Rect dirty)
    {
        rawSpreadsheet->Paint(canvas, dirty);
    });
    ysDui::platform::win32::WindowOptions options;
    options.title = "Spreadsheet DPI";
    options.kind = ysDui::platform::win32::WindowOptions::Kind::Popup;
    options.size = {360, 260};
    assert(window.Create(options));
    const auto handle = window.NativeHandle();
    const auto nativeHandle = reinterpret_cast<::HWND>(handle.value);
    assert(nativeHandle != nullptr);

    ::RECT windowBounds{};
    assert(::GetWindowRect(nativeHandle, &windowBounds));
    const ysDui::core::DuiDpiScale scale(144);
    const ysDui::core::Size suggestedSize = scale.Scale(options.size);
    const ::RECT suggestedBounds{windowBounds.left, windowBounds.top,
                                 windowBounds.left + suggestedSize.width,
                                 windowBounds.top + suggestedSize.height};
    ::SendMessageW(nativeHandle, WM_DPICHANGED, MAKEWPARAM(144, 144),
                   reinterpret_cast<::LPARAM>(&suggestedBounds));
    assert(host->DpiScale().Dpi() == 144);

    ::RECT client{};
    assert(::GetClientRect(nativeHandle, &client));
    const ysDui::core::Size logicalClient = scale.Unscale(
        ysDui::core::Size{client.right - client.left, client.bottom - client.top});
    assert((rawSpreadsheet->Bounds() == ysDui::core::Rect{0, 0, logicalClient.width, logicalClient.height}));
    assert(rawSpreadsheet->VisibleCellBounds({0, 0}).has_value());
    assert(::RedrawWindow(nativeHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW));

    window.Close();
    host->SetRoot({});
}

void VerifySpreadsheetWindowMinimizeRestore()
{
    SpreadsheetStressModel model;
    auto host = std::make_shared<ysDui::core::Host>();
    auto spreadsheet = std::make_unique<ysDui::controls::list::DuiSpreadsheet>();
    spreadsheet->SetWorkbookModel(&model);
    auto* const rawSpreadsheet = spreadsheet.get();
    host->SetRoot(std::move(spreadsheet));

    ysDui::platform::win32::Window window(host);
    int resizeCount{};
    window.SetResizeHandler([rawSpreadsheet, &resizeCount](ysDui::core::Size size)
    {
        ++resizeCount;
        rawSpreadsheet->Layout({0, 0, size.width, size.height});
    });
    window.SetPaintHandler([rawSpreadsheet](ysDui::render::Canvas& canvas, ysDui::core::Rect dirty)
    {
        rawSpreadsheet->Paint(canvas, dirty);
    });
    ysDui::platform::win32::WindowOptions options;
    options.title = "Spreadsheet minimize";
    options.kind = ysDui::platform::win32::WindowOptions::Kind::Popup;
    options.size = {360, 260};
    assert(window.Create(options));
    const ::HWND nativeHandle = reinterpret_cast<::HWND>(window.NativeHandle().value);
    assert(nativeHandle != nullptr);

    ::RECT client{};
    assert(::GetClientRect(nativeHandle, &client));
    ::SendMessageW(nativeHandle, WM_SIZE, SIZE_RESTORED,
                   MAKELPARAM(client.right - client.left, client.bottom - client.top));
    const int resizeCountBeforeMinimize = resizeCount;
    const ysDui::core::Rect boundsBeforeMinimize = rawSpreadsheet->Bounds();
    ::SendMessageW(nativeHandle, WM_SIZE, SIZE_MINIMIZED, 0);
    assert(resizeCount == resizeCountBeforeMinimize);
    assert(rawSpreadsheet->Bounds() == boundsBeforeMinimize);

    ::SendMessageW(nativeHandle, WM_SIZE, SIZE_RESTORED,
                   MAKELPARAM(client.right - client.left, client.bottom - client.top));
    assert(resizeCount == resizeCountBeforeMinimize + 1);
    assert(rawSpreadsheet->VisibleCellBounds({0, 0}).has_value());
    assert(::RedrawWindow(nativeHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW));

    int displayChangePaintCount{};
    window.SetPaintHandler([rawSpreadsheet, &displayChangePaintCount](ysDui::render::Canvas& canvas,
                                                                       ysDui::core::Rect dirty)
    {
        ++displayChangePaintCount;
        rawSpreadsheet->Paint(canvas, dirty);
    });
    ::ValidateRect(nativeHandle, nullptr);
    ::SendMessageW(nativeHandle, WM_DISPLAYCHANGE, 32, MAKELPARAM(1280, 720));
    // DISPLAYCHANGE 会丢弃离屏缓冲。不依赖 GetUpdateRect / UpdateWindow：在最小化残留、
    // 遮挡或会话繁忙时前者可恒为 0，后者可偶发不派发 WM_PAINT（全量 CTest 约 1/5~1/15
    // 假失败）。强制整窗重绘即可验证"缓冲已 Reset 后仍能重建并绘制一次"。
    displayChangePaintCount = 0;
    assert(::RedrawWindow(nativeHandle, nullptr, nullptr,
                          RDW_INVALIDATE | RDW_UPDATENOW) != FALSE);
    assert(displayChangePaintCount == 1);

    window.Close();
    host->SetRoot({});
}
}

int main()
{
    ysDui::platform::win32::DuiWin32Application application;
    assert(application.Initialize());
    assert(application.Initialize());
    assert(application.Initialized());
    VerifySpreadsheetDirectTextInput();
    VerifySpreadsheetWindowStress();
    VerifySpreadsheetWindowDpiChange();
    VerifySpreadsheetWindowMinimizeRestore();

    ::HWND parent = ::CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                       137, 91, 240, 160, nullptr, nullptr,
                                       ::GetModuleHandleW(nullptr), nullptr);
    assert(parent != nullptr);

    auto dpiHost = std::make_shared<ysDui::core::Host>();
    ysDui::platform::win32::Window dpiWindow(dpiHost);
    ysDui::platform::win32::WindowOptions dpiWindowOptions;
    dpiWindowOptions.title = "DPI lifecycle";
    dpiWindowOptions.size = {120, 80};
    dpiWindowOptions.kind = ysDui::platform::win32::WindowOptions::Kind::Popup;
    int dpiResizeCount{};
    ysDui::core::Size dpiResizeSize;
    bool observingDpiChange{};
    dpiWindow.SetResizeHandler([&](ysDui::core::Size size) {
        if (!observingDpiChange)
            return;
        ++dpiResizeCount;
        dpiResizeSize = size;
        assert(dpiHost->DpiScale().Dpi() == 144);
    });
    assert(dpiWindow.Create(dpiWindowOptions));
    const ::HWND dpiHandle = reinterpret_cast<::HWND>(dpiWindow.NativeHandle().value);

    int animationPaintCount{};
    ysDui::core::Rect lastDirty;
    dpiWindow.SetPaintHandler([&animationPaintCount, &lastDirty](ysDui::render::Canvas&,
                                                                 ysDui::core::Rect dirty)
    {
        ++animationPaintCount;
        lastDirty = dirty;
    });
    ::RedrawWindow(dpiHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    const int paintCountBeforeAnimation = animationPaintCount;
    const auto animationTask = dpiHost->Clock().Schedule(1000, [](double) {});
    ::SendMessageW(dpiHandle, WM_ENTERSIZEMOVE, 0, 0);
    ::SendMessageW(dpiHandle, WM_TIMER, AnimationTimerId, 0);
    assert(animationPaintCount == paintCountBeforeAnimation);
    ::SendMessageW(dpiHandle, WM_EXITSIZEMOVE, 0, 0);
    ::ValidateRect(dpiHandle, nullptr);
    const ::RECT partialPaint{8, 8, 24, 24};
    assert(::InvalidateRect(dpiHandle, &partialPaint, FALSE));
    ::UpdateWindow(dpiHandle);
    assert(lastDirty.Width() < dpiWindowOptions.size.width);
    assert(lastDirty.Height() < dpiWindowOptions.size.height);
    dpiHost->Clock().Cancel(animationTask);
    assert(dpiHandle != nullptr);
    ::RECT dpiBounds{};
    assert(::GetWindowRect(dpiHandle, &dpiBounds));
    const ysDui::core::Size requestedPixels = ysDui::core::DuiDpiScale(144).Scale(
        ysDui::core::Size{180, 100});
    const ::RECT suggestedDpiBounds{dpiBounds.left, dpiBounds.top,
                                    dpiBounds.left + requestedPixels.width,
                                    dpiBounds.top + requestedPixels.height};
    observingDpiChange = true;
    ::SendMessageW(dpiHandle, WM_DPICHANGED, MAKEWPARAM(144, 144),
                   reinterpret_cast<::LPARAM>(&suggestedDpiBounds));
    ::RECT dpiClient{};
    assert(::GetClientRect(dpiHandle, &dpiClient));
    assert(dpiResizeCount == 1);
    assert(dpiHost->DpiScale().Dpi() == 144);
    assert((dpiResizeSize == ysDui::core::DuiDpiScale(144).Unscale(
        ysDui::core::Size{dpiClient.right, dpiClient.bottom})));
    dpiWindow.Close();

    const ysDui::platform::win32::NativeWindowHandle handle{reinterpret_cast<std::uintptr_t>(parent)};
    ysDui::platform::win32::Win32HostRefAdapter parentAdapter(handle);
    const ysDui::ui::HostRef parentReference = parentAdapter.Reference();
    assert(parentReference.Valid() && !parentReference.Empty());
    ysDui::ui::HostRef copiedReference = parentReference;
    ysDui::ui::HostRef movedReference = std::move(copiedReference);
    assert(movedReference.Valid() && copiedReference.Empty());
    const ysDui::ui::HostRef copiedMovedFromReference = copiedReference;
    assert(copiedMovedFromReference.Empty());
    copiedReference = parentReference;
    assert(copiedReference.Valid());

    ysDui::ui::HostRef expiredReference;
    {
        ysDui::platform::win32::Win32HostRefAdapter temporaryAdapter(handle);
        expiredReference = temporaryAdapter.Reference();
        assert(expiredReference.Valid());
    }
    assert(expiredReference.Empty() && !expiredReference.Valid());

    const auto otherBinding = std::make_shared<OtherHostBinding>();
    const ysDui::ui::HostRef otherReference =
        ysDui::ui::detail::HostRefAccess::Create(otherBinding);
    assert(otherReference.Valid());
    ysDui::platform::win32::Win32UiHostFactory backendFactory;
    ysDui::ui::IUiHostFactory& factory = backendFactory;
    assert(!factory.CreateNativeViewHost(otherReference));
    auto incompatibleEmbedded = factory.CreateEmbeddedHost(otherReference);
    assert(incompatibleEmbedded && !incompatibleEmbedded->Show(
        {{0, 0, 20, 20}, false}, std::make_unique<ysDui::core::Control>(), {}, {}));

    auto frame = factory.CreateFrameHost();
    assert(frame);
    bool frameClosed{};
    int frameCaptionId{};
    frame->SetClosedHandler([&frameClosed] { frameClosed = true; });
    frame->SetCaptionButtonHandler([&frameCaptionId](int id) { frameCaptionId = id; });
    ysDui::ui::DuiFrameOptions frameOptions;
    frameOptions.title = "Frame";
    frameOptions.position = {0, 0};
    frameOptions.size = {200, 120};
    frameOptions.minimumSize = {100, 80};
    ysDui::core::Rect frameContentBounds;
    assert(frame->Show(frameOptions, std::make_unique<ysDui::core::Control>(),
                       [&frameContentBounds](ysDui::core::Rect bounds) { frameContentBounds = bounds; }, {}));
    assert(frame->Visible());
    assert((frameContentBounds == ysDui::core::Rect{0, 36, 200, 120}));
    frameOptions.size = {240, 160};
    frameOptions.minimumSize = {160, 120};
    frameOptions.maximumSize = {320, 260};
    frameOptions.showMinimize = false;
    frameOptions.showMaximize = false;
    frameOptions.captionButtons.push_back(
        {77, ysDui::ui::DuiFrameCaptionAction::Custom, {}, {}, "Custom", false});
    frame->SetOptions(frameOptions);
    assert((frameContentBounds == ysDui::core::Rect{0, 36, 240, 160}));
    ::HWND frameWindow = ::FindWindowW(L"ysDui.v2.Window", L"Frame");
    assert(frameWindow != nullptr);
    ::MINMAXINFO frameLimits{};
    ::SendMessageW(frameWindow, WM_GETMINMAXINFO, 0, reinterpret_cast<::LPARAM>(&frameLimits));
    assert(frameLimits.ptMinTrackSize.x == 160 && frameLimits.ptMinTrackSize.y == 120);
    assert(frameLimits.ptMaxTrackSize.x == 320 && frameLimits.ptMaxTrackSize.y == 260);
    // 自定义无边框窗口：最大化范围必须钉在工作区上（否则会盖住任务栏、标题栏顶到屏幕外）
    {
        ::MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        assert(::GetMonitorInfoW(::MonitorFromWindow(frameWindow, MONITOR_DEFAULTTONEAREST), &monitor));
        assert(frameLimits.ptMaxSize.x == monitor.rcWork.right - monitor.rcWork.left);
        assert(frameLimits.ptMaxSize.y == monitor.rcWork.bottom - monitor.rcWork.top);
        assert(frameLimits.ptMaxPosition.x == monitor.rcWork.left - monitor.rcMonitor.left);
        assert(frameLimits.ptMaxPosition.y == monitor.rcWork.top - monitor.rcMonitor.top);
    }
    // 标题栏按钮动作被推迟到消息处理栈外执行（避免回调内关闭窗口的自毁），
    // 因此按下 + 抬起后需要泵一次消息才会收到回调。
    // 坐标按窗口 DPI 换算：物理像素进入平台层后才转回 DIP，固定像素在非 100% 缩放下会点空。
    const int frameDpi = static_cast<int>(::GetDpiForWindow(frameWindow));
    const int clickX = ::MulDiv(171, frameDpi, 96);
    const int clickY = ::MulDiv(18, frameDpi, 96);
    ::SendMessageW(frameWindow, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(clickX, clickY));
    ::SendMessageW(frameWindow, WM_LBUTTONUP, 0, MAKELPARAM(clickX, clickY));
    for (int pump = 0; pump < 2000 && frameCaptionId == 0; ++pump)
    {
        ::MSG message{};
        while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
        if (frameCaptionId != 0)
            break;
        // 不要用 WaitMessage：队列空时会无限阻塞
        ::Sleep(1);
    }
    assert(frameCaptionId == 77);
    // 缩放到新尺寸后必须重绘：否则内容按新尺寸排好、画面仍是旧的
    {
        int framePaintCount{};
        ysDui::core::Size observedContentSize{};
        frame->SetContent(std::make_unique<ysDui::core::Control>(),
            [&observedContentSize](ysDui::core::Rect bounds) { observedContentSize = {bounds.Width(), bounds.Height()}; },
            [&framePaintCount](ysDui::render::Canvas&, ysDui::core::Rect) { ++framePaintCount; });
        frameOptions.size = {300, 200};
        frame->SetOptions(frameOptions);
        assert(observedContentSize.height == 200 - 36);
        // SetOptions 内部会请求重绘
        ::UpdateWindow(frameWindow);
        assert(framePaintCount >= 1);
    }
    // 标题栏图标：为窗格提供「前面放图标」的落点
    {
        frameOptions.icon = ysDui::render::DuiImage::CreateBgra8Premultiplied(
            {2, 2}, std::vector<unsigned char>(16, 255));
        assert(static_cast<bool>(frameOptions.icon));
        frame->SetOptions(frameOptions);
        assert(frame->Visible());
    }
    // 自绘标题栏：必须没有原生非客户区，否则系统标题栏会叠在自绘标题栏上
    {
        ::RECT frameClient{};
        ::RECT frameWhole{};
        assert(::GetClientRect(frameWindow, &frameClient));
        assert(::GetWindowRect(frameWindow, &frameWhole));
        assert(frameClient.right - frameClient.left == frameWhole.right - frameWhole.left);
        assert(frameClient.bottom - frameClient.top == frameWhole.bottom - frameWhole.top);
    }
    // 最大化由本类自实现：先尊重调用方给定的最大尺寸，解除后精确落在工作区，还原回到原尺寸
    {
        ::MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        assert(::GetMonitorInfoW(::MonitorFromWindow(frameWindow, MONITOR_DEFAULTTONEAREST), &monitor));
        ::RECT before{};
        assert(::GetWindowRect(frameWindow, &before));
        ::SendMessageW(frameWindow, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        ::RECT zoomed{};
        assert(::GetWindowRect(frameWindow, &zoomed));
        // 此时仍设着 maximumSize(320x260)：不得超出它
        assert(zoomed.right - zoomed.left == 320 && zoomed.bottom - zoomed.top == 260);
        ::SendMessageW(frameWindow, WM_SYSCOMMAND, SC_RESTORE, 0);
        ::RECT restored{};
        assert(::GetWindowRect(frameWindow, &restored));
        assert(restored.left == before.left && restored.top == before.top);
        assert(restored.right == before.right && restored.bottom == before.bottom);

        // 解除最大尺寸后，最大化范围必须精确等于工作区（不能盖住任务栏）
        frameOptions.maximumSize = {};
        frame->SetOptions(frameOptions);
        ::SendMessageW(frameWindow, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        assert(::GetWindowRect(frameWindow, &zoomed));
        assert(zoomed.left == monitor.rcWork.left && zoomed.top == monitor.rcWork.top);
        assert(zoomed.right == monitor.rcWork.right && zoomed.bottom == monitor.rcWork.bottom);
        ::SendMessageW(frameWindow, WM_SYSCOMMAND, SC_RESTORE, 0);
        assert(::GetWindowRect(frameWindow, &restored));
        assert(restored.left == before.left && restored.top == before.top);
        assert(restored.right == before.right && restored.bottom == before.bottom);
    }
    frame->Close();
    assert(!frame->Visible() && frameClosed);
    // 关闭次要窗口后仍不应有 WM_QUIT 挂起（PostQuitMessage 是进程级的，
    // 只有正在跑消息循环的那个窗口销毁时才该结束循环）
    assert(::PeekMessageW(nullptr, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) == FALSE);

    auto embedded = factory.CreateEmbeddedHost(parentReference);
    assert(embedded);
    auto embeddedContent = std::make_unique<ysDui::core::Control>();
    ysDui::core::Control* embeddedControl = embeddedContent.get();
    assert(embedded->Show({{0, 0, 80, 40}, false}, std::move(embeddedContent),
                          [embeddedControl](ysDui::core::Rect bounds) { embeddedControl->SetBounds(bounds); }, {}));
    assert(embedded->Visible());
    ::HWND embeddedWindow = FindOwnedWindow(parent);
    assert(embeddedWindow != nullptr);
    embedded->SetBounds({10, 12, 110, 72});
    ::POINT expectedEmbeddedPosition{10, 12};
    assert(::ClientToScreen(parent, &expectedEmbeddedPosition));
    ::RECT embeddedBounds{};
    assert(::GetWindowRect(embeddedWindow, &embeddedBounds));
    assert(embeddedBounds.left == expectedEmbeddedPosition.x && embeddedBounds.top == expectedEmbeddedPosition.y);
    assert(embeddedBounds.right - embeddedBounds.left == 100 && embeddedBounds.bottom - embeddedBounds.top == 60);
    embedded->RequestHide();
    assert(!embedded->Visible());

    ::HWND childAnchor = ::CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                                            0, 0, 1, 1, parent, nullptr,
                                            ::GetModuleHandleW(nullptr), nullptr);
    assert(childAnchor != nullptr);
    ysDui::platform::win32::Win32HostRefAdapter childAdapter(
        {reinterpret_cast<std::uintptr_t>(childAnchor)});
    auto childEmbedded = factory.CreateEmbeddedHost(childAdapter.Reference());
    assert(childEmbedded);
    assert(childEmbedded->Show({{0, 0, 80, 40}, false}, std::make_unique<ysDui::core::Control>(), {}, {}));
    ::HWND childWindow = ::FindWindowExW(childAnchor, nullptr, L"ysDui.v2.Window", nullptr);
    assert(childWindow != nullptr);
    assert(::GetParent(childWindow) == childAnchor);
    childEmbedded->SetBounds({11, 12, 111, 72});
    ::RECT childBounds{};
    ::GetClientRect(childWindow, &childBounds);
    assert(childBounds.right == 100);
    assert(childBounds.bottom == 60);
    childEmbedded->Hide();
    ::DestroyWindow(childAnchor);

    auto nativeView = factory.CreateNativeViewHost(parentReference);
    assert(nativeView);
    nativeView->SetBounds({0, 0, 20, 10});
    nativeView->SetVisible(true);
    assert(!nativeView->HasContent());

    ::HWND nativeChild = ::CreateWindowExW(
        WS_EX_CLIENTEDGE, L"BUTTON", L"Native child", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        0, 0, 80, 40, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    assert(nativeChild != nullptr);
    const ::LONG_PTR nativeStyle = ::GetWindowLongPtrW(nativeChild, GWL_STYLE);
    const ::LONG_PTR nativeExStyle = ::GetWindowLongPtrW(nativeChild, GWL_EXSTYLE);
    nativeView->SetBounds({7, 9, 127, 69});
    ysDui::ui::NativeViewRef nativeReference = ysDui::platform::win32::CreateNativeViewRef(
        {reinterpret_cast<std::uintptr_t>(nativeChild)});
    ysDui::ui::NativeViewRef nativeReferenceCopy = nativeReference;
    ysDui::ui::NativeViewRef movedNativeReference = std::move(nativeReference);
    assert(movedNativeReference.Valid() && nativeReference.Empty() && nativeReferenceCopy.Valid());
    const ysDui::ui::NativeViewRef copiedMovedFromNativeReference = nativeReference;
    assert(copiedMovedFromNativeReference.Empty());
    assert(nativeView->Attach(std::move(movedNativeReference), {false, true}));
    assert(nativeView->HasContent());
    const ::HWND nativeContainer = ::GetParent(nativeChild);
    assert(nativeContainer != nullptr && nativeContainer != parent && nativeContainer != HWND_MESSAGE);
    const ::LONG_PTR embeddedStyle = ::GetWindowLongPtrW(nativeChild, GWL_STYLE);
    assert((embeddedStyle & WS_CHILD) != 0);
    assert((embeddedStyle & (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
                             | WS_SYSMENU | WS_POPUP | WS_OVERLAPPED)) == 0);
    ::RECT nativeBounds{};
    assert(::GetClientRect(nativeChild, &nativeBounds));
    assert(nativeBounds.right == 120 && nativeBounds.bottom == 60);
    nativeView->SetVisible(false);
    assert((::GetWindowLongPtrW(nativeContainer, GWL_STYLE) & WS_VISIBLE) == 0);
    nativeView->SetVisible(true);
    assert((::GetWindowLongPtrW(nativeContainer, GWL_STYLE) & WS_VISIBLE) != 0);
    const ysDui::ui::NativeViewRef detachedNative = nativeView->Detach();
    assert(ysDui::platform::win32::detail::ResolveNativeView(detachedNative) == nativeChild);
    assert(!nativeView->HasContent() && ::IsWindow(nativeChild));
    assert(::GetWindowLongPtrW(nativeChild, GWL_STYLE) == nativeStyle);
    assert(::GetWindowLongPtrW(nativeChild, GWL_EXSTYLE) == nativeExStyle);
    assert(::DestroyWindow(nativeChild));

    ::HWND ownedNativeChild = ::CreateWindowExW(
        0, L"BUTTON", L"Owned native child", WS_CHILD | WS_VISIBLE,
        0, 0, 40, 20, HWND_MESSAGE, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    assert(ownedNativeChild != nullptr);
    assert(nativeView->Attach(ysDui::platform::win32::CreateNativeViewRef(
        {reinterpret_cast<std::uintptr_t>(ownedNativeChild)}), {true, false}));
    assert(nativeView->Detach().Empty());
    assert(!::IsWindow(ownedNativeChild));

    auto popup = factory.CreatePopupHost(parentReference);
    assert(popup);
    auto popupContent = std::make_unique<ysDui::core::Control>();
    ysDui::core::Control* popupControl = popupContent.get();
    assert(popup->Show({{0, 0, 10, 10}, {80, 40}, ysDui::ui::DuiPopupPlacement::Below, false},
                       std::move(popupContent),
                       [popupControl](ysDui::core::Rect bounds) { popupControl->SetBounds(bounds); }, {}));
    assert(popup->Visible());
    popup->RequestHide();
    assert(!popup->Visible());

    ysDui::ui::DuiPopupOptions dialogOptions;
    dialogOptions.anchor = {0, 0, 10, 10};
    dialogOptions.size = {320, 180};
    dialogOptions.placement = ysDui::ui::DuiPopupPlacement::CenterOwner;
    dialogOptions.dismissOnFocusLost = false;
    dialogOptions.modality = ysDui::ui::DuiPopupModality::OwnerModal;
    dialogOptions.resizable = true;

    ysDui::controls::window::DuiDialog dialog;
    dialog.SetTitle("Collapsible");
    dialog.SetButtons({{"OK", ysDui::controls::window::DuiDialog::IdOk, true},
                       {"Apply", ysDui::controls::window::DuiDialog::IdNone, false},
                       {"Cancel", ysDui::controls::window::DuiDialog::IdCancel, false}});
    dialog.SetCollapsible(true);
    auto dialogContent = std::make_unique<ysDui::core::Control>();
    ysDui::core::Control* dialogContentRaw = dialogContent.get();
    assert(dialog.Show(factory, parentReference, dialogOptions, std::move(dialogContent)));
    ::HWND dialogWindow = ::GetWindow(parent, GW_ENABLEDPOPUP);
    assert(dialogWindow != nullptr && dialogWindow != parent);
    assert(HitTestAt(dialogWindow, 80, 12) == HTCLIENT);
    ::RECT dialogBounds{};
    assert(::SetWindowPos(dialogWindow, nullptr, 0, 0, 480, 320,
                          SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    ::SendMessageW(dialogWindow, WM_EXITSIZEMOVE, 0, 0);
    ::RECT dialogClient{};
    assert(::GetClientRect(dialogWindow, &dialogClient));
    assert(dialogClient.right == 480 && dialogClient.bottom == 320);
    // 内容区 = 面板矩形再内缩 panelPadding(10)：
    // 面板 {12, 36, 468, 275} -> 内容 {22, 46, 458, 265}
    assert((dialogContentRaw->Bounds() == ysDui::core::Rect{22, 46, 458, 265}));
    ::RedrawWindow(dialogWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    ::HDC dialogContext = ::GetDC(dialogWindow);
    assert(dialogContext != nullptr);
    assert(::GetPixel(dialogContext, dialogClient.right - 100, 12) == RGB(0, 95, 135));
    assert(::GetPixel(dialogContext, dialogClient.right - 20, 30) == RGB(242, 242, 242));
    assert(::GetPixel(dialogContext, 20, dialogClient.bottom - 5) == RGB(242, 242, 242));
    assert(::ReleaseDC(dialogWindow, dialogContext) == 1);
    const ::LPARAM settingsPoint = MAKELPARAM(12, 12);
    ::SendMessageW(dialogWindow, WM_MOUSEMOVE, 0, settingsPoint);
    ::RedrawWindow(dialogWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    ::HDC hoveredContext = ::GetDC(dialogWindow);
    assert(hoveredContext != nullptr);
    const ::COLORREF hoveredSettingsColor = ::GetPixel(hoveredContext, 12, 12);
    assert(::ReleaseDC(dialogWindow, hoveredContext) == 1);
    ::SendMessageW(dialogWindow, WM_ENTERSIZEMOVE, 0, 0);
    ::RedrawWindow(dialogWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    ::HDC clearedContext = ::GetDC(dialogWindow);
    assert(clearedContext != nullptr);
    const ::COLORREF clearedSettingsColor = ::GetPixel(clearedContext, 12, 12);
    assert(::ReleaseDC(dialogWindow, clearedContext) == 1);
    assert(hoveredSettingsColor != clearedSettingsColor);
    ::SendMessageW(dialogWindow, WM_EXITSIZEMOVE, 0, 0);
    assert(::GetWindowRect(dialogWindow, &dialogBounds));
    ::SendMessageW(dialogWindow, WM_LBUTTONDBLCLK, 0, MAKELPARAM(80, 12));
    ::RECT collapsedBounds{};
    assert(dialog.Collapsed() && ::GetWindowRect(dialogWindow, &collapsedBounds));
    assert(collapsedBounds.left == dialogBounds.left && collapsedBounds.top == dialogBounds.top);
    assert(collapsedBounds.bottom - collapsedBounds.top == 24);
    // 折叠后拖到新位置再展开：应保留拖动后的位置，而不是跳回折叠前坐标。
    constexpr int CollapsedDragOffsetX = 80;
    constexpr int CollapsedDragOffsetY = 60;
    assert(::SetWindowPos(dialogWindow, nullptr,
                          collapsedBounds.left + CollapsedDragOffsetX,
                          collapsedBounds.top + CollapsedDragOffsetY, 0, 0,
                          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    ::SendMessageW(dialogWindow, WM_EXITSIZEMOVE, 0, 0);
    ::RECT draggedCollapsedBounds{};
    assert(::GetWindowRect(dialogWindow, &draggedCollapsedBounds));
    ::SendMessageW(dialogWindow, WM_LBUTTONDBLCLK, 0, MAKELPARAM(80, 12));
    ::RECT restoredDialogBounds{};
    assert(!dialog.Collapsed() && ::GetWindowRect(dialogWindow, &restoredDialogBounds));
    assert(restoredDialogBounds.left == draggedCollapsedBounds.left);
    assert(restoredDialogBounds.top == draggedCollapsedBounds.top);
    assert(restoredDialogBounds.right - restoredDialogBounds.left
           == dialogBounds.right - dialogBounds.left);
    assert(restoredDialogBounds.bottom - restoredDialogBounds.top
           == dialogBounds.bottom - dialogBounds.top);
    dialog.Close(13);
    assert(ysDui::platform::win32::DuiWin32ModalDialog::Wait(dialog) == 13);

    ysDui::controls::window::DuiDialog maximizableDialog;
    maximizableDialog.SetTitle("Maximizable");
    maximizableDialog.SetMaximizable(true);
    dialogOptions.size = {340, 220};
    assert(maximizableDialog.Show(factory, parentReference, dialogOptions,
                                  std::make_unique<ysDui::core::Control>()));
    ::HWND maximizableWindow = ::GetWindow(parent, GW_ENABLEDPOPUP);
    assert(maximizableWindow != nullptr && maximizableWindow != parent);
    ::RECT normalBounds{};
    assert(::GetWindowRect(maximizableWindow, &normalBounds));
    ::SendMessageW(maximizableWindow, WM_LBUTTONDBLCLK, 0, MAKELPARAM(80, 12));
    ::RECT maximizedBounds{};
    ::MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    assert(maximizableDialog.Maximized());
    assert(::GetWindowRect(maximizableWindow, &maximizedBounds));
    assert(::GetMonitorInfoW(::MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST), &monitorInfo));
    assert(SameBounds(maximizedBounds, monitorInfo.rcWork));
    ::SendMessageW(maximizableWindow, WM_LBUTTONDBLCLK, 0, MAKELPARAM(80, 12));
    ::RECT restoredMaximizedBounds{};
    assert(!maximizableDialog.Maximized());
    assert(::GetWindowRect(maximizableWindow, &restoredMaximizedBounds));
    assert(SameBounds(restoredMaximizedBounds, normalBounds));
    maximizableDialog.Close(ysDui::controls::window::DuiDialog::IdCancel);

    auto layered = factory.CreateLayeredHost(parentReference);
    assert(layered);
    auto layeredContent = std::make_unique<ysDui::core::Control>();
    ysDui::core::Control* layeredControl = layeredContent.get();
    bool layeredPainted{};
    assert(layered->Show({{11, 12, 91, 52}, 192, false}, std::move(layeredContent),
                         [layeredControl](ysDui::core::Rect bounds) { layeredControl->SetBounds(bounds); },
                         [&layeredPainted](ysDui::render::Canvas&, ysDui::core::Rect) { layeredPainted = true; }));
    assert(layered->Visible());
    assert(layeredPainted);
    const ::HWND layeredWindow = ::GetWindow(parent, GW_ENABLEDPOPUP);
    assert(layeredWindow != nullptr);
    ::POINT expectedPosition{11, 12};
    assert(::ClientToScreen(parent, &expectedPosition));
    ::RECT layeredBounds{};
    assert(::GetWindowRect(layeredWindow, &layeredBounds));
    assert(layeredBounds.left == expectedPosition.x && layeredBounds.top == expectedPosition.y);
    layered->RequestHide();
    assert(!layered->Visible());

    ::DestroyWindow(parent);
    assert(!parentReference.Valid() && !parentReference.Empty());
    application.Terminate();
    assert(!application.Initialized());
}
