#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <thread>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/platform/win32/DuiWin32TextInput.hpp"
#include "ysDui/platform/win32/DuiWindow.hpp"

int main() {
    ::HWND parent = ::CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                       0, 0, 1, 1, nullptr, nullptr,
                                       ::GetModuleHandleW(nullptr), nullptr);
    assert(parent != nullptr);

    int changes{};
    int submits{};
    {
        ysDui::platform::win32::Win32TextInput input(
            {reinterpret_cast<std::uintptr_t>(parent)});
        const ::HWND edit = ::FindWindowExW(parent, nullptr, L"EDIT", nullptr);
        assert(edit != nullptr);
        const auto nativeBounds = [parent](::HWND child)
        {
            ::RECT bounds{};
            assert(::GetWindowRect(child, &bounds));
            ::MapWindowPoints(nullptr, parent, reinterpret_cast<::POINT*>(&bounds), 2);
            return ysDui::core::Rect{bounds.left, bounds.top, bounds.right, bounds.bottom};
        };
        input.SetBounds({4, 6, 104, 36});
        const ysDui::core::Rect centeredBounds = nativeBounds(edit);
        assert(centeredBounds.Height() < input.Bounds().Height());
        assert(centeredBounds.top - input.Bounds().top
            == (input.Bounds().Height() - centeredBounds.Height() + 1) / 2 + 1);
        const auto inputFont = reinterpret_cast<::HFONT>(::SendMessageW(edit, WM_GETFONT, 0, 0));
        ::LOGFONTW font{};
        assert(inputFont != nullptr && ::GetObjectW(inputFont, sizeof(font), &font) != 0);
        ::HDC context = ::GetDC(parent);
        assert(context != nullptr);
        const int expectedFontHeight = -::MulDiv(10, ::GetDeviceCaps(context, LOGPIXELSY), 72);
        ::ReleaseDC(parent, context);
        assert(::lstrcmpW(font.lfFaceName, L"Segoe UI") == 0 && font.lfHeight == expectedFontHeight);
        input.SetSubmitHandler([&submits] { ++submits; });
        (void)::SendMessageW(edit, WM_KEYDOWN, VK_RETURN, 0);
        assert(submits == 1);
        input.SetOptions({true, true, false, false, 0});
        (void)::SendMessageW(edit, WM_KEYDOWN, VK_RETURN, 0);
        assert(submits == 1);
        // 多行 + submitOnEnter：Enter 触发提交，且换行字符被吞掉（否则会多出一个空行）
        input.SetText("ab");
        input.SetOptions({true, true, false, false, 0, true});
        (void)::SendMessageW(edit, WM_CHAR, L'\r', 0);
        assert(input.Text() == "ab");
        (void)::SendMessageW(edit, WM_KEYDOWN, VK_RETURN, 0);
        assert(submits == 2);
        // 关闭该选项后多行 Enter 回到"换行、不提交"
        input.SetOptions({true, true, false, false, 0});
        (void)::SendMessageW(edit, WM_KEYDOWN, VK_RETURN, 0);
        assert(submits == 2);
        assert(nativeBounds(edit) == input.Bounds());
        input.SetBorderVisible(false);
        assert((::GetWindowLongPtrW(edit, GWL_STYLE) & WS_BORDER) == 0);
        assert((::GetWindowLongPtrW(edit, GWL_EXSTYLE) & WS_EX_CLIENTEDGE) == 0);
        assert(::SendMessageW(edit, WM_GETFONT, 0, 0) != 0);
        input.SetVisible(true);
        assert((::GetWindowLongPtrW(edit, GWL_STYLE) & WS_VISIBLE) == 0);
        input.Focus();
        assert(::GetFocus() == edit);
        assert(input.CaretVisible());
        const ::UINT caretBlinkTime = ::GetCaretBlinkTime();
        if (caretBlinkTime != INFINITE && caretBlinkTime > 0 && caretBlinkTime <= 1000)
        {
            ::ValidateRect(parent, nullptr);
            // 只断言可观察且确定性的契约：自绘光标相位会随闪烁周期翻转。
            // 不断言 WM_TIMER —— 闪烁定时器由控件在 WM_SETFOCUS 时内部装载，实测本进程对该
            // 定时器的触发时机不可靠（同一窗口上自装定时器可稳定收到 WM_TIMER，但控件的定时器
            // 约每 3~9 次运行就有一次在 2.6s 窗口内收不到），原实现固定 Sleep 一次后直接断言
            // 因此长期以约 40% 概率假失败。定时器→重绘这条路径的改动请人工复核。
            const bool initialCaretVisible = input.CaretVisible();
            bool caretPhaseChanged{};
            const ::ULONGLONG deadline = ::GetTickCount64()
                + static_cast<::ULONGLONG>(caretBlinkTime) * 4 + 500;
            while (!caretPhaseChanged && ::GetTickCount64() < deadline)
            {
                ::Sleep(10);
                if (input.CaretVisible() != initialCaretVisible)
                    caretPhaseChanged = true;
            }
            assert(caretPhaseChanged);
            ::SendMessageW(edit, EM_SETSEL, 0, 0);
            assert(input.CaretVisible());
        }
        input.SetOptions({false, false, false, true, 0});
        ::HDC editContext = ::GetDC(edit);
        assert(editContext != nullptr);
        ::SetBkColor(editContext, RGB(1, 2, 3));
        ::SendMessageW(parent, WM_CTLCOLORSTATIC, reinterpret_cast<::WPARAM>(editContext),
                       reinterpret_cast<::LPARAM>(edit));
        assert(::GetBkColor(editContext) == RGB(255, 255, 255));
        ::ReleaseDC(edit, editContext);
        input.SetPlaceholder("Search");
        input.SetChangedHandler([&changes] { ++changes; });
        input.SetText("ysDui");
        assert(input.Text() == "ysDui");
        assert(changes == 1);
        input.SetText("\xE4\xB8\xAD" "A");
        input.SetOptions({false, false, false, false, 0});
        ::SendMessageW(edit, EM_SETSEL, 1, 1);
        assert(input.CaretPosition() == 3);
        ::SendMessageW(edit, EM_SETSEL, 2, 2);
        ::SendMessageW(edit, WM_CHAR, L'B', 1);
        assert(input.Text() == "\xE4\xB8\xAD" "AB");
        input.SetText("A\xE4\xB8\xAD\xE6\x96\x87" "B");
        input.BeginSelection({10000, 20});
        input.UpdateSelection({-10000, 20});
        assert((input.TextSelection() == ysDui::ui::DuiTextSelection{0, 8}));
        input.EndSelection();
        ::SendMessageW(edit, WM_CHAR, L'Z', 1);
        assert(input.Text() == "Z");
        input.SetText("\xE4\xB8\xAD" "AB");
        input.FocusAt({4, 20});
        assert(input.CaretPosition() == 0);
        // SelectWordAt：隐藏输入会话收不到鼠标消息，双击选词须由平台代理转发，
        // 并应选中坐标所在的单词（而非把选区折叠为插入点）。
        input.SetOptions({false, false, false, false, 0});
        input.SetText("alpha beta gamma");
        input.Focus();
        input.SelectWordAt({input.Bounds().left + 8, input.Bounds().top + 8});
        const ysDui::ui::DuiTextSelection word = input.TextSelection();
        const std::string selectedWord = input.Text().substr(word.start, word.end - word.start);
        assert(word.start == 0 && !selectedWord.empty()
            && selectedWord.size() <= sizeof("alpha ") - 1
            && input.Text().compare(0, selectedWord.size(), selectedWord) == 0);
        input.SetVisible(false);
        assert(::GetFocus() != edit);
    }

    auto host = std::make_shared<ysDui::core::Host>();
    ysDui::platform::win32::Window window(host);
    ysDui::platform::win32::WindowOptions options;
    options.kind = ysDui::platform::win32::WindowOptions::Kind::Popup;
    options.owner = {reinterpret_cast<std::uintptr_t>(parent)};
    options.size = {40, 30};
    assert(window.Create(options));
    const auto nativeWindow = reinterpret_cast<::HWND>(window.NativeHandle().value);
    assert((::GetWindowLongPtrW(nativeWindow, GWL_STYLE) & WS_CLIPCHILDREN) != 0);
    assert(reinterpret_cast<::HCURSOR>(::GetClassLongPtrW(nativeWindow, GCLP_HCURSOR))
        == ::LoadCursor(nullptr, IDC_ARROW));
    {
        ysDui::platform::win32::Win32TextInput input(window.NativeHandle());
        const ::HWND edit = ::FindWindowExW(nativeWindow, nullptr, L"EDIT", nullptr);
        assert(edit != nullptr);
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
        (void)::SendMessageW(edit, WM_SETCURSOR, reinterpret_cast<::WPARAM>(edit),
                             MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        assert(::GetCursor() == ::LoadCursor(nullptr, IDC_IBEAM));
        (void)::SendMessageW(nativeWindow, WM_SETCURSOR, reinterpret_cast<::WPARAM>(nativeWindow),
                             MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        assert(::GetCursor() == ::LoadCursor(nullptr, IDC_ARROW));
    }
    // 派发器唤醒：后台线程投递后必须让窗口出现待重绘区域，
    // 否则任务要等到下一次输入事件才执行（界面表现为"卡住不动"）。
    // 断言用 GetUpdateRect 而不是计时：前者与机器负载无关。
    {
        std::atomic<bool> taskRan{};
        ::ValidateRect(nativeWindow, nullptr);
        assert(::GetUpdateRect(nativeWindow, nullptr, FALSE) == 0);
        std::thread poster([host, &taskRan]
        {
            host->Dispatcher().Post([&taskRan] { taskRan = true; });
        });
        poster.join();
        // Post 在返回前就同步调用了唤醒回调，因此此处必然已有待重绘区域
        assert(::GetUpdateRect(nativeWindow, nullptr, FALSE) != 0);
        (void)::SendMessageW(nativeWindow, WM_PAINT, 0, 0);
        assert(taskRan);
        // 队列已排空：再次投递仍能重新唤醒
        host->Dispatcher().Clear();
        ::ValidateRect(nativeWindow, nullptr);
        assert(::GetUpdateRect(nativeWindow, nullptr, FALSE) == 0);
        host->Dispatcher().Post([] {});
        assert(::GetUpdateRect(nativeWindow, nullptr, FALSE) != 0);
        host->Dispatcher().Clear();
    }

    auto clickable = std::make_unique<ysDui::core::Control>();    auto* clickableRaw = clickable.get();
    clickable->SetBounds({0, 0, 40, 30});
    clickable->SetPointerCursor(ysDui::core::DuiPointerCursor::Hand);
    host->SetRoot(std::move(clickable));
    ::POINT originalCursor{};
    if (::GetCursorPos(&originalCursor))
    {
        ::POINT clickablePoint{10, 10};
        assert(::ClientToScreen(nativeWindow, &clickablePoint));
        assert(::SetCursorPos(clickablePoint.x, clickablePoint.y));
        (void)::SendMessageW(nativeWindow, WM_SETCURSOR, reinterpret_cast<::WPARAM>(nativeWindow),
                             MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        const ::HCURSOR clickableCursor = ::GetCursor();
        assert(clickableCursor == ::LoadCursor(nullptr, IDC_HAND));
        clickableRaw->SetPointerCursor(ysDui::core::DuiPointerCursor::IBeam);
        (void)::SendMessageW(nativeWindow, WM_SETCURSOR, reinterpret_cast<::WPARAM>(nativeWindow),
                             MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        assert(::GetCursor() == ::LoadCursor(nullptr, IDC_IBEAM));
        (void)::SetCursorPos(originalCursor.x, originalCursor.y);
    }
    window.Close();

    ::DestroyWindow(parent);
}
