// main.cpp
// 开发者：青蓝
// 开发时间：2026-07-29
// 用途：链接安装后的 ysDui 目标并验证基础控件可用。

#include "ysDui/controls/basic/DuiButton.hpp"
#include "ysDui/controls/list/DuiWorkbookText.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/core/DuiVersion.hpp"
#if defined(YSDUI_INSTALL_HAS_RESOURCE)
#include "ysDui/resource/DuiResourcePackage.hpp"
#endif

int main()
{
    ysDui::core::DuiTheme theme;
    ysDui::controls::basic::DuiButton button;
    button.SetTheme(&theme);
    button.SetText("Installed ysDui");
    const auto format = ysDui::controls::list::DuiDelimitedTextFormat::Csv;
    bool resourceAvailable = true;
#if defined(YSDUI_INSTALL_HAS_RESOURCE)
    resourceAvailable = !ysDui::resource::DuiResourcePackage::Open("missing-package.zip").has_value();
#endif
    return resourceAvailable && button.Text() == "Installed ysDui" && ysDui::VersionMajor == 2
        && ysDui::VersionMinor == 0 && ysDui::VersionPatch == 0
        && ysDui::Version == "2.0.0" && format == ysDui::controls::list::DuiDelimitedTextFormat::Csv ? 0 : 1;
}
