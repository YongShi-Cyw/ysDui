/**
 * 文件名：DuiWin32Application.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现 Windows 应用初始化、释放和消息循环。
 */
#include "ysDui/platform/win32/DuiWin32Application.hpp"

#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <ole2.h>

namespace ysDui::platform::win32 {

class DuiWin32Application::Impl
{
public:
    bool initialized{};
    bool apartmentInitialized{};
};

DuiWin32Application::DuiWin32Application() : application_(std::make_unique<Impl>()) {}
DuiWin32Application::~DuiWin32Application() { Terminate(); }
DuiWin32Application::DuiWin32Application(DuiWin32Application&&) noexcept = default;
DuiWin32Application& DuiWin32Application::operator=(DuiWin32Application&&) noexcept = default;

bool DuiWin32Application::Initialize()
{
    if (application_->initialized)
        return true;
    // 进程清单或宿主可能已先设置 DPI；ERROR_ACCESS_DENIED 表示该状态不可再次修改。
    if (!::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
        && ::GetLastError() != ERROR_ACCESS_DENIED)
    {
        return false;
    }
    const HRESULT result = ::OleInitialize(nullptr);
    if (FAILED(result) && result != RPC_E_CHANGED_MODE)
        return false;
    application_->apartmentInitialized = SUCCEEDED(result);
    const INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES};
    ::InitCommonControlsEx(&controls);
    application_->initialized = true;
    return true;
}

void DuiWin32Application::Terminate()
{
    if (application_->apartmentInitialized) {
        ::OleUninitialize();
        application_->apartmentInitialized = false;
    }
    application_->initialized = false;
}

int DuiWin32Application::Run()
{
    ::MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void DuiWin32Application::Quit(int exitCode) { ::PostQuitMessage(exitCode); }
bool DuiWin32Application::Initialized() const { return application_->initialized; }

} // namespace ysDui::platform::win32
