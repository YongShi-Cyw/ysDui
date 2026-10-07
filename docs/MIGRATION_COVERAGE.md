# ysDui v2 迁移覆盖审计

## 审计基线

- 源快照：`D:\00-ClionCode\balloonui-main` 的 Git 提交 `d1ab3f3`。
- 目标：本仓库的 `include/ysDui`、`src` 与 `CMakeLists.txt`。
- 结论仅描述 v2 的功能归属，不保留旧 Win32 C++ API 或 ABI。

## 控件覆盖

| 源快照功能组 | v2 归属 | 说明 |
| --- | --- | --- |
| Basic：Avatar、Badge、Breadcrumb、Button、Chip、Expander、GroupBox、InfoBar、Label、SegmentedControl、Separator、StatusBar、Toast、ToastCenter、ToolBar | `controls/basic` | 使用 `core::Control` 与 `render::DuiRenderable`。`DuiChip` 为对话/表单场景新增。 |
| Chart：BarChart、LineChart、PieChart | `controls/chart` | 纯 Canvas 绘制；数值轴范围与网格分段由控件自行取整，无平台依赖。 |
| Chat：ChatBubble、ChatList | `controls/chat` | 对话消息气泡与变高虚拟列表：分侧、限宽、头像、元信息行；条目惰性测量、窗口内排版与绘制。正文由调用方注入（通常为 `controls/content/DuiMarkdownView`）。 |
| Content：MarkdownView | `controls/content` | GFM 解析与排版在控件层完成，含表格、代码围栏、公式与图片管线。 |
| Graph：NodeEditor、NodeGraph | `controls/graph` | 蓝图节点编辑器与图模型；视图变换由控件作用于绘制与命中的坐标，文件 I/O 仅在 `.cpp` 内经 UTF-8 路径转换。 |
| Feedback：BusyIndicator、EmojiPanel、ProgressBar、ToolTip、TypingIndicator | `controls/feedback` | 弹出宿主由 `ui::IPopupHost` 和平台后端提供；`DuiTypingIndicator` 为对话等待态新增。 |
| Input：ColorPicker、ComboBox、DateTimePicker、DoubleSpinBox、EditHost、HotKey、PathEdit、RangeSlider、RatingControl、RichEditHost、SearchBox、Slider、SpinBox、Switch | `controls/input` | 原生编辑、文件和颜色能力通过平台接口注入。 |
| Layout：Canvas、Dock、Flow、Layout、Splitter、Stack、UniformGrid | `controls/layout` | 纯控件树布局，不引用平台后端。 |
| List：DataGrid、ListBox、Menu、MenuBar、PropertyGrid、Tab、TabPage、TreeView | `controls/list` | 添加了平台无关的 `DuiVirtualList`。 |
| Imaging：AsyncImageLoader、Gif、Image、ImageViewer | `controls/media` | 解码由平台后端提供；异步加载与缩放/平移查看在控件层实现。 |
| Window：Dialog、Flyout、MessageBox、NativeHost、FrameWindow | `controls/window` 与 `ui` | `IEmbeddedHost`、`IPopupHost`、`ILayeredHost`、`IFrameHost` 替代 Child/Popup/Layered/Frame 的公开 Win32 语义。 |

`DuiMonthCalendar` 的实现合并在 `src/controls/input/DuiDateTimePicker.cpp`，并随 `DuiDateTimePicker.cpp` 编译，不是缺失实现。

## 基础设施覆盖

| 源快照功能 | v2 归属 | 说明 |
| --- | --- | --- |
| Application、Control、Host、DPI、主题、动画、覆盖层、助记符、无障碍 | `core` | 值类型与状态对象不含平台类型。 |
| Canvas、路径、文本、图像、九宫格、图标字体、SVG、XML、基准图像 | `render` | Canvas 仅声明平台无关绘制协议。 |
| GDI/GDI+、窗口、消息循环、文件对话框、输入法、OLE、UIA、拖放 | `platform/win32` | Windows 类型与 COM/OLE 实现仅存在私有后端文件。 |
| 资源包与打包器 | `resource::DuiResourcePackage`、`ysdui_resource_pack` | 使用可选 ZIP/Deflate；可选 libsodium 条目加密不进入公开头。 |

## 破坏性 API 映射

下列源快照入口不以同名 API 迁移；其功能由 v2 的分层接口承接，或因旧 API 仅封装 Win32 细节而有意取消。

| 源快照入口 | v2 映射 | 处理结论 |
| --- | --- | --- |
| `DuiApplication` | `platform::win32::DuiWin32Application` | 初始化、消息循环和退出请求属于 Windows 后端，不进入无平台公开层。 |
| `HwndHostControl`、`DuiEmbeddedHost`、`DuiLayeredHost`、`DuiPopupHost` | `ui::IEmbeddedHost`、`ui::ILayeredHost`、`ui::IPopupHost`、`ui::IFrameHost` | 上层经 `IUiHostFactory` 获取宿主，不感知 Child、Popup 或 Layered 实现。 |
| `DuiNotify` 与 `WM_DUI_NOTIFY` | `core::Event`、控件专用回调 | 取消同步窗口消息、裸控件指针和整数载荷的公开协议。 |
| `File`、`Path`、`Utf8` | 标准 C++ 文件/路径能力、`DuiSkinSession`、资源包接口 | 旧辅助类仅服务旧皮肤加载且暴露 Win32 类型，v2 不保留。 |
| `DuiResMgr`、`ImageEx`、`DuiAsyncImage` | `render::DuiImage`、平台 `DuiImageDecoder`、`controls::media::DuiAsyncImageLoader`、`render::DuiFontRegistry` | 图像像素、解码和字体注册分别由渲染抽象、平台能力与控件层缓存负责。 |
| `DuiGraphicsContext`、`DuiDrawUtil`、`DuiPaintAA`、`DuiWin32Canvas` | `render::Canvas` 与私有 Win32 GDI/GDI+ 实现 | 绘制命令不再暴露设备上下文、字体或颜色句柄。 |
| `DuiRichEditHost::SaveText/LoadText` | `DuiRichEditHost::Text/SetText` | 使用 UTF-8 `std::string` 直接交换纯文本，不再定义后端字节流格式。 |

源 `DuiRichEditHost::SaveRTF/LoadRTF` 的格式、嵌入 OLE 图像和对象所有权不作为公开协议迁移。v2 使用 `DuiRichDocumentJson` 持久化 `DuiRichDocument`：文本格式、链接、引用和文件卡片直接保存为 UTF-8 JSON；图像仅保存资源包条目名和逻辑尺寸，载入时由调用方通过资源包和平台解码器解析。

## 验证证据

```powershell
cmake -S . -B build-core -G "NMake Makefiles" -DYSDUI_BUILD_WIN32_BACKEND=OFF
cmake --build build-core --target ysdui_tests
ctest --test-dir build-core --output-on-failure

cmake -S . -B build -G "NMake Makefiles"
cmake --build build --target ysdui_gallery ysdui_tests ysdui_platform_win32
ctest --test-dir build --output-on-failure

cmake -S . -B build-resource -G "NMake Makefiles" -DYSDUI_BUILD_WIN32_BACKEND=OFF -DYSDUI_BUILD_RESOURCE_MODULE=ON
cmake --build build-resource --target ysdui_resource_pack ysdui_resource_package_tests
ctest --test-dir build-resource --output-on-failure
```

## 后续迁移能力

以下源能力需要在已确认的跨平台产品语义下继续迁移：

`core::DuiSkinCatalog` 与 `core::DuiSkinSession` 已迁移 `SkinManager` 的皮肤目录、当前皮肤选择和逻辑资源寻址。图像解码与缓存由平台后端私有管理；v2 不迁移旧全局单例、`CImageEx*` 或手动引用计数。

`DuiFrameWindow` 已映射到 `ui::IFrameHost` 与 `ui::DuiFrameOptions`：接口覆盖顶层窗口生命周期、无边框标题栏、八向缩放、最小/最大尺寸、标准和自定义标题按钮、平台无关图标、九宫格背景与方角偏好。Win32 后端私有实现命中测试和系统窗口命令；SVG 图标先经 `RasterizeSvg` 转为 `render::DuiImage`。`DuiXmlBuilder::BuildFrameWithResult` 解析 `<frame-window>` 的平台无关配置和客户区控件树，通过既有资源和图像解析器装载背景资源，并返回包含元素、属性和源位置的结构化错误；旧 `BuildFrame` 在 2.x 过渡期保留但已弃用。
