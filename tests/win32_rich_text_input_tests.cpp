#define NOMINMAX
#include <windows.h>

#include <cassert>
#include <cstdint>

#include "ysDui/platform/win32/DuiNativeWindowHandle.hpp"
#include "ysDui/platform/win32/DuiWin32RichTextInput.hpp"

int main()
{
    ::HWND parent = ::CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                       0, 0, 1, 1, nullptr, nullptr,
                                       ::GetModuleHandleW(nullptr), nullptr);
    assert(parent != nullptr);

    {
        ysDui::platform::win32::Win32RichTextInput input(
            {reinterpret_cast<std::uintptr_t>(parent)});
        input.SetOptions({true, true, false, false, 128});
        input.SetText("Hello");
        input.SetSelection({1, 4});
        assert((input.Selection() == ysDui::ui::DuiTextRange{1, 4}));
        input.ReplaceSelection("i");
        assert(input.Text() == "Hio");
        input.AppendText("!");
        assert(input.Text() == "Hio!");
        input.SetSelection({0, 1});
        input.SetSelectionFormat({{10, 20, 30, 255}, true, false, true});
        input.SetAutomaticLinkDetection(true);
        input.SetSelection({-1, -1});
        input.InsertQuoteBlock("Alice", "Hello");
        input.InsertFileCard("report.txt", 2048);
        assert(input.Text().find("Alice") != std::string::npos);
        assert(input.Text().find("report.txt") != std::string::npos);
        assert(input.CanUndo());
    }

    ::DestroyWindow(parent);
}
