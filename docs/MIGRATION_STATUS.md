# ysDui v2 迁移状态

源快照固定为 `D:\00-ClionCode\balloonui-main` 的提交 `d1ab3f3`。该仓库只作为行为和样式基线，未被本项目修改。

## 已完成

- `core`：控件树、布局、事件、主题、DIP/DPI、动画、无障碍、皮肤目录和宿主上下文。
- `render`：完整平台无关 Canvas 协议、文本度量、路径、图像、SVG、XML、显示列表和像素基准工具。
- `ui`：`HostRef`、`NativeViewRef` 以及文本输入、剪贴板、选择器和窗口宿主能力接口。
- `controls`：Basic、Input、Feedback、Layout、List & Navigation、Media、Window & Host、Resources & XML 中的全部公开控件均已迁移，并由 Gallery 目录覆盖。
- `platform-win32`：Win32 窗口、DIP 转换、消息转换、DirectWrite/GDI/GDI+ 绘制、原生输入、COM/OLE、UIA、拖放、文件对话框和图像解码均留在后端私有实现中。
- `resource`：可选 ZIP/Deflate 资源包、打包器和可选 libsodium 条目加密；公开接口使用 UTF-8 `std::string` 路径和条目名。

公开头自包含编译和平台依赖审计覆盖 `core`、`render`、`ui`、`controls` 与 `resource`。无平台模块不链接 Windows SDK 即可构建和测试。

## 持续验证

- Linux GCC、Clang 构建并测试无平台模块；Windows MSVC 构建完整后端和 Gallery。
- Ubuntu 资源模块任务启用 ZIP/Deflate 并运行资源安装消费者；独立加密任务使用 vcpkg libsodium 覆盖正确密钥和错误密钥路径。加密仍是显式可选配置，不在默认构建中启用。
- `ysdui_gallery --run-tests` 保持目录和交互结构回归测试；`ysdui_gallery_capture_all` 捕获全部 69 个页面，并校验文件集合及 `tests/gallery_golden_sha256.txt` 中的 SHA-256 基线。
- Windows 视觉哈希基线针对固定的 GDI+/DirectWrite 离屏渲染环境。样式改动必须先人工确认与 `d1ab3f3` 的预期一致，再有意更新清单。

## 当前限制

- 目前只有 Win32 窗口后端；Linux GCC/Clang 仅证明无平台模块可以独立构建，不提供 Linux 原生宿主。
- Win32 文本排版由 DirectWrite 统一 shaping、双向文本、换行、字体回退、测量和绘制；其他后端仍需自行实现相同的 Canvas 文本契约。
- 资源模块已支持安装和 `find_package(ysDui COMPONENTS resource)` 消费；加密构建仍要求消费者提供 libsodium CMake package。
- Gallery 像素基线是 Win32 后端回归保护，不替代跨操作系统的字体和图形一致性策略。

## 后续门槛

新增平台后端必须实现完整 Canvas 协议、DIP 事件转换和需要的 `ui` 能力接口，并通过公开头审计、无平台构建及后端回归测试。resource 的安装/消费契约已经验证；后续只允许在同步更新依赖导出、安装消费者和许可证文件后升级第三方版本。
