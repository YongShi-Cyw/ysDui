/**
 * 文件名：DuiWin32GraphicsContext.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现 Win32 图形运行时的进程级 RAII 管理。
 */
#include "DuiWin32GraphicsContext.hpp"

#include <windows.h>
#include <gdiplus.h>

namespace ysDui::platform::win32 {
namespace {

class GraphicsRuntime final
{
public:
    GraphicsRuntime()
    {
        Gdiplus::GdiplusStartupInput input;
        ready_ = Gdiplus::GdiplusStartup(&token_, &input, nullptr) == Gdiplus::Ok;
    }

    ~GraphicsRuntime()
    {
        if (ready_)
            Gdiplus::GdiplusShutdown(token_);
    }

    [[nodiscard]] bool Ready() const
    {
        return ready_;
    }

private:
    ULONG_PTR token_{};
    bool ready_{};
};

GraphicsRuntime& Runtime()
{
    static GraphicsRuntime runtime;
    return runtime;
}

} // namespace

bool EnsureGdiPlus()
{
    return Runtime().Ready();
}

} // namespace ysDui::platform::win32
