# 规约与已知陷阱

本文汇总 ysDui 的三条硬性不变量、控件尺寸协议，以及排查问题时最常遇到的一批陷阱。
目的与 `ARCHITECTURE.md` / `THREADING.md` 互补：那两篇讲"应该怎么设计"，本篇讲"照着症状怎么排"。

---

## 一、三条不变量（破坏它们必出诡异 Bug）

### 1. 坐标单位是 DIP

布局、`Point` / `Size` / `Rect`、线宽、圆角、字号**全部是 96-DPI 逻辑像素**。
物理像素只出现在 Win32 边界：

| 位置 | 说明 |
| --- | --- |
| `platform::win32::Window` 的原生窗口矩形 | 由 `DuiDpiScale` 从 DIP 换算 |
| `platform::win32::Win32TextInput` 的原生子窗口位置 | 内部 `Scale().Scale()` 换算 |
| `DuiWin32EventConverter` 的返回值 | 把 Win32 鼠标坐标转成 DIP 后才派发 |

Win32 消息里的鼠标坐标是**物理像素**，经过事件转换器后才是 DIP。控件层不得自行做像素换算。

### 2. 控件树的所有权

- 父节点以 `std::unique_ptr<Control>` 持有子节点（`Control::AddChild`）。
- `Control::Parent()` 是**非拥有**的裸指针。
- 控件非拥有地引用外部能力对象（`SetTextInput` / `SetPopupHost` / 选择器 / 颜色选择器等）：
  提供方必须比该绑定活得更久；替换或解绑时先调 `SetTextInput(nullptr)` 再销毁旧提供方。
- 不要在回调外长期缓存可能被移除的 `Control*`；回调内可移除当前控件或替换宿主根。

### 3. 结构变化走失效，几何写入不冒泡

`AddChild` / `RemoveChild` / `SetVisible` 会 `InvalidateLayout()`，从本节点标脏到根。
宿主在绘制前 `PrepareFrame` 对根调用 `PerformLayout()`，以当前 `Bounds()` 重排。
不要在指针拖拽过程中的 `Dispatch` 里套用，以免把正在拖的分割条写回。
布局容器对子节点调用 `Layout` 而不是 `SetBounds`，嵌套容器会一并向下排列。

| 你改了什么 | 要做什么 |
| --- | --- |
| 增删子节点、显隐 | 通常无需手写父级 `Layout`；下一帧自动重排 |
| 内容尺寸变化（文字变长等） | 调用本控件或祖先的 `InvalidateLayout()` |
| 只改自身矩形 | `SetBounds()`（**不**冒泡，也不标脏） |
| 只影响外观 | `InvalidateRect` 由平台层负责 |

> 关键点：`Control::SetBounds()` 只写自己的矩形，**不冒泡**。不要用它代替 `Layout` / `InvalidateLayout`。
> `Layout` 进行中再 `InvalidateLayout` 会被忽略，避免滚动条显隐等在排列期内循环标脏。

### 4. 显隐与启停都沿父链求值

`EffectivelyVisible()` 与 `Enabled()` 都从当前节点向上逐级判断，因此**作用于一个节点即覆盖整棵子树**：

- 禁用内容根即可让整个区域变灰并停止交互（`DuiGroupBox` 的勾选态就是这么实现的）。
- `HitTest` 在禁用节点处直接落空，所以后代收不到指针事件；Tab 焦点也会跳过。
- 反过来说：`SetEnabled(true)` **救不回**祖先被禁用的后代——必须先恢复祖先。

> 关键点：`Enabled()` 不是"本节点自己的标志"，而是"有效启用状态"。需要读自己那一份时请用别的状态成员。

---

## 二、控件尺寸协议 `DesiredSize()`

`core::Control::DesiredSize()` 是**虚协议**（基类返回 `{0, 0}`，表示"无偏好、由容器决定"）。
- 具体控件重写时**必须**写 `override`（编译器会校验签名，漏写即编译失败，不会静默失效）。
- 可经 `core::Control&` 多态查询，容器不必知道具体类型。
- 容器侧：`controls/layout/DuiLayoutHint` 提供三种主轴尺寸来源

| Hint | 含义 |
| --- | --- |
| `Fixed(main, cross)` | 主轴固定像素 |
| `Flexible(weight)` | 吃剩余空间，按权重分配 |
| `Auto()` | **主轴取子控件 `DesiredSize()`**（内容驱动尺寸） |

`Auto()` 与 `Flexible()` 混排时：`Auto` 计入固定部分，`Flexible` 分配余量。

---

## 三、陷阱速查表（照着症状排能省半天）

| 症状 | 原因 | 对策 |
| --- | --- | --- |
| 想整体停用一个区域（表单区、可勾选分组框），但逐个控件 `SetEnabled(false)` 很啰嗦，且重排/换内容后要重做 | — | `Control::Enabled()` **沿父链求值**（与 `EffectivelyVisible()` 同构）：只需禁用**内容根**一个节点，整棵子树自动取禁用配色、`HitTest` 直接落空、Tab 焦点跳过、UIA 报告为禁用。例：`DuiGroupBox` 的勾选态即内容根的 enabled 标志 |
| 禁用容器里的文字仍是正常黑色，看起来没变灰 | `DuiLabel` 原先不读 `Enabled()` | 已在 `DuiLabel::Paint` 中按 `Enabled()` 取 `ButtonChoiceDisabledText`；自定义控件若有"禁用态外观"，也应在 Paint 里读 `Enabled()` |
| 标签关闭/重排回调内重建控件树导致崩溃 | 回调由 `DuiTab::OnEvent` 内部发出，此刻销毁它自己（如重建停靠布局）会释放正在执行的栈帧 | 回调内只改模型并置挂起标记，在回调栈退出后的安全点（`Layout` / `Paint`）再重建。参考 `DuiDockManager::FlushRebuild`；同类先例见 `DuiToastCenter` 的 `Purge` |
| 关闭悬浮窗口时崩溃 / 回调参数变成空串 | 在 `IFrameHost::SetClosedHandler` 回调里直接 `erase` 该窗口，会销毁**正在执行回调的那个宿主对象**（闭包内存随之释放） | 回调内只登记待释放（`DuiDockManager` 的 `retiredFloats` + `pendingClosed`），统一在 `FlushRebuild` 里销毁并通知。任何「容器在成员回调里移除自己」的场景都适用 |
| 悬浮窗里的内容无法收回停靠区 | `IFrameHost` 原先只能 `Close()`，控件树归宿主所有且无法交出 | 用 `IFrameHost::DetachContent()`（默认返回空 = 不支持）配合 `core::Host::ReleaseRoot()` 取回内容；收回前先清空窗口的布局/绘制回调，再 `Close()`。参考 `DuiDockManager::DockFloatingPane` |
| 自定义标题栏按钮的回调里关闭该窗口导致崩溃 | 回调由窗口自身的 `WM_LBUTTONUP` 处理栈内发出；在栈内销毁窗口会释放正在执行的状态对象 | 标题栏动作一律经 `PostMessage` 推迟到消息返回后执行（`Win32FrameHost::PostCaptionInvoke` / `PostCaptionInvoke` 的 `kInvokeCaptionMessage`）。测试需先泵消息再断言回调 |
| 自定义标题栏窗口上多出一排系统按钮 / 最大化盖住任务栏 | `WM_NCCALCSIZE` 只 `return 0` 对带 `WS_CAPTION` 的样式（如 `WS_OVERLAPPEDWINDOW`）并不会真正去掉非客户区；系统还会把最大化矩形按边框厚度撑到工作区之外 | `WM_NCCALCSIZE` 里把 `rgrc[0]` 显式设为窗口矩形；最大化由自己实现（`Win32FrameHost::ToggleMaximize` + `WM_SYSCOMMAND` 拦截），按 `GetMonitorInfo` 的工作区落位并保留还原矩形 |
| 关闭一个次要窗口把主窗口一起关掉 | `Window` 的 `WM_DESTROY` 对所有 `Kind::TopLevel` 窗口都 `PostQuitMessage`，而 `PostQuitMessage` 是**进程级**的 | 只有正在跑消息循环的窗口才结束循环：`Window::Run` 期间置 `messageLoopActive`，`WM_DESTROY` 据此判断 |
| 自定义标题栏按钮回调里关闭该窗口崩溃 | 回调在窗口自身的消息处理栈内执行，销毁窗口会释放正在执行的状态对象与回调闭包 | 标题栏动作一律经 `PostMessage` 推迟到消息返回后执行（`Win32FrameHost::PostCaptionInvoke`，自定义消息 `WM_APP+0x51`）；宿主要销毁悬浮窗体时也只登记、留到安全点（`DuiDockManager::retiredFrames`） |
| 缩放/最大化后界面不更新 | 尺寸回调里只重排了内容，没有请求重绘 | 在尺寸回调中同时 `Invalidate`（`Win32FrameHost` 的 `SetResizeHandler`） |
| 停靠布局里控件被重复持有 / 无法在容器间搬迁内容 | `DuiTabPage` 原先只能销毁页面（`RemovePage`），不能交出所有权 | 用 `DuiTabPage::ReleasePage` 摘出后挂到新容器；重建前先把内容摘进临时池，重建后未被消费的即已从模型移除，随池释放 |
| 布局文件丢了窗格 / 静默改名 | 布局同时存了窗格集合与标题，与调用方的登记不一致时结论冲突 | 布局只存结构与窗格 id（`DuiDockTree::SaveLayout`）：窗格集合与标题以调用方登记为准，未出现在布局中的窗格补进第一个组，出现但当前不存在的忽略 |
| 弹出层边缘看不出范围（与宿主窗口背景混在一起） | `IPopupHost` **不画背景也不画边框**，只用内容注册的 Paint 处理器呈现 | 弹出内容必须自绘表面：`render::PaintPopupBackground` + 内容 + `render::PaintPopupBorder`（见 `DuiFlyout` / `DuiComboBox`） |
| 控件外框被视口裁掉、且框不住滚动条（如 TreeView 放进 ScrollView） | 内容的矩形比可视区高，底边画在视口外被裁掉；滚动条又属于外层容器 | 外框由**能看见完整范围的一方**绘制：滚动条归谁就由谁画（`DuiScrollView` 画自己的 `Bounds`；`DuiListBox` / `DuiVirtualList` 自带滚动条，各自画 `Bounds`）。此时内容须关掉自绘外框：`DuiTreeView::SetBorderVisible(false)`，否则会在滚动条左侧留下半截框线 |
| 需要缩放/旋转绘制几何 | `Canvas::PushTransform` 提供相似变换（缩放+平移） | 在画布坐标下绘制后 `PushTransform`；命中测试仍用同一套 `DuiCanvasTransform`。字号/描边随变换缩放；`MeasureText` 不受影响 |
| 画圆弧、贝塞尔曲线或饼图扇区 | `render::DuiPath` **只有 `MoveTo`/`LineTo`/`Close`**（无弧线与曲线指令） | 折线逼近：扇区按角度分段（`ArcPath`），连线按三次贝塞尔采样（`LinkPolyline`）。段数需按尺寸调参，过密会拖慢绘制 |
| 缩放后内嵌控件文字溢出 | 控件内部字号/描边/圆角默认是绝对 DIP | `DuiNodeEditor` **任意缩放都显示**内嵌控件。`DuiSpinBox` / `DuiDoubleSpinBox` / `DuiEditHost` 按 `Bounds` 高度相对默认高度缩放字号与铬件。其它控件若未适配，放大后仍可能框大字小；根治需 Canvas 变换 |
| 子控件被父容器遮住或被画到父容器外 | `render::PaintChildren` **不做裁剪** | 自行 `PushClip(视口)` → 手动 `PaintChildren` → `PopClip`，见 `DuiScrollView` 与 `DuiNodeEditor`。注意**自绘内容同样要裁剪**：自己算出的屏幕矩形可能落在控件之外 |
| 自定义视图缩放后文字相对图形过大（观感"粗糙"） | 与几何一起缩放时，若字号仍是绝对 DIP，小缩放下文字会撑满甚至溢出图形 | **必须同步缩放字号**：用 `ScaledText(style, scale)` 把 `pointSize` 乘以缩放（并设下限）。线条同理需设最小宽度，否则会细成发丝 |
| 内联控件（放进父控件布局的控件）静默不占位 | 布局按 `DesiredSize()` 取尺寸，而基类返回 `{0,0}`——未重写该接口的控件会被当成"无尺寸" | 布局方应做**回退**：`DesiredSize()` 为 0 时改用 `Bounds().Width()`（`DuiNodeEditor` 的引脚内联控件即此做法）；内联控件本身最好重写 `DesiredSize()` |
| `DuiChatBubble` 气泡只剩一条细缝、正文看不到 | 气泡高度随正文收缩，而正文（如 `DuiLabel`）`DesiredSize()` 返回 `{0,0}`，气泡被收缩成空气泡 | `DuiChatBubble` 内已做回退：正文报告高度 ≤ 0 时**沿用调用方给定高度**而非收缩。但仍建议正文用能报告高度的控件（`DuiMarkdownView`），或显式给出足够的 Bounds 高度 |
| 系统提示气泡在白底页面上"看不见" | 助手气泡底色是白色，系统提示若沿用同一配色就等于隐形 | `DuiChatBubble` 的 `System` 角色取 `SurfaceAlternateBackground` + `BorderLight` 描边；新增角色配色时务必验证"与页面底色不同" |
| 标签/单行文本溢出到相邻控件上 | `DrawText` 不裁剪，超长文本会画到自家矩形之外 | 自绘控件在绘制文字/图标前 `PushClip(自身矩形)`，结束再 `PopClip`（见 `DuiChip::Paint`）。注意这不是自动的 |
| 变高列表里 `Item(i)->Bounds()` 是空的 / 贴底时最后一条没跟到底 / 快速滚动时出现空白 | 窗口外的条目不会被 `Layout`；条目内容变化后没失效 | 取位置用 `ItemRect(i)`（按偏移推算，任何下标都有值）；内容变化后调 `InvalidateItem(i)`；空白是预布局余量太小，调大 `SetOverscan`。详见「AI 对话相关控件」 |
| 后台投递了任务，界面却要等到动一下鼠标才更新 | 队列由空转非空时的唤醒回调没接，消息循环仍阻塞在 `GetMessage` | 宿主后端把 `Dispatcher().SetPendingHandler` 接到"唤醒一帧"（Win32 用 `InvalidateRect`）。ysDui 的 `window.cpp` 已接好；自建宿主需自行接线 |
| 流式输出的最后一段被"回退"成较早的片段 | 结束前挂起的合并增量在最终内容之后执行，把它覆盖回旧前缀 | 应用最终内容前先 `CancelCoalesced(key)`，再投递最终全文。合并槽按首次投递位置排序，所以在同一次 Drain 内必须显式取消 |
| 关闭页面后偶发崩溃 / 访问已释放控件 | 挂起的跨线程任务在控件树销毁后仍然执行 | 销毁前 `Dispatcher().Clear()`；平台桥接（如 Win32 `Window::Close`）已内置该清理。任务内部若销毁了内容，`Drain()` 会在该任务后立即停止本批次 |
| 多行输入框按 Enter 没反应、无法提交 | 平台层在**多行**输入下把 Enter 交给原生控件换行，因此不触发提交；控制层原本也没暴露提交回调 | 打开 `ui::DuiTextInputOptions::submitOnEnter`（Enter 提交、Shift+Enter 换行），用 `DuiEditHost::SetSubmitHandler` 接收回调。单行输入无需该选项，Enter 始终提交 |
| 测试里 `assert(control.Foo() == Rect{1, 2, 3, 4})` 编译报"宏参数过多" | `assert` 由 `test_assertions.hpp` 强制定义为**单参数宏**，而宏参数只被圆括号保护，`Rect{}` / `Color{}` 里的逗号会被当成参数分隔符 | 含花括号初始化列表的断言要多套一层圆括号：`assert((control.Foo() == Rect{1, 2, 3, 4}))`。计算式无逗号时才可省（如 `assert(size.width == 10)`) |
| 新增 `.cpp` 后 `LNK2019` | `ysdui_*` 的 `add_library` 用**显式文件列表**（无 GLOB） | 把文件加进 `ysDui/CMakeLists.txt` 对应目标。注意规则不同：`ysAppMain` / `ysAppDevelop` / `ysAppCmd` / `ysAppTest` 用 `GLOB_RECURSE` 会自动收录 |
| 公开头报平台依赖 | `public_header_audit` 拒绝 `HWND` / `RECT` / `D2D1_*` 等 | 平台类型只能留在 `platform-win32` 私有实现。**审计会先剥离注释再扫**，所以注释里出现这些名字是安全的 |
| 隐藏或离树控件的动画还在跑 | 重复型动画（`DuiBusyIndicator` / `DuiGif` / `ProgressBar` 不确定态）在回调里无限自我重排；`SetVisible` **不是**虚函数，控件感知不到自己被隐藏 | 显式 `SetActive(false)` / 停止动画；移除控件前主动取消。这是当前已知限制 |
| 动画/定时器回调里销毁控件 → 随机崩溃 | `AnimationClock::Advance` 会在遍历中调用各任务的回调；若回调栈内销毁控件，返回后仍会访问已释放对象 | 回调内**只做标记**，把真正的移除推迟到安全时机（`Layout` / 显式 `Purge`）。参考 `DuiToastCenter`：消息结束时只置 `retired`，由 `Purge()` 在回调之外 `RemoveChild` |
| 枚举属性读出乱值 / NX 报 **3520043** | NX 8.5 JA 属性路径下，enum 型属性不能用 `get_string` 读取 | 用 `Value()` + `GetEnumMembers()`；慎用 `ValueAsString()` |
| 设置树节点颜色报**错误 65** | 在 `dialogShown_cb` 之前操作了未插入的树节点 | 列插入与 `SetOn*Handler` 一律放在 `dialogShown_cb` |
| 树回调编译报 `C2664` | `make_callback` 签名与 `ysUiTree.h` 的 `CallbackN` 不一致 | 必须**完全一致**：`std::string` / `std::vector` 按值传递，不要用 `const&` |
| 旧栈 + 新栈 Face 头编译冲突 `C2872` | `06-ugui` 与 `06-uguiNew` 双栈并存 | uguiNew 的 Face 头放在旧栈头**之后**，或仅前置声明；`.hpp` 禁止 `using namespace ysUI` |
| 重建受保护 DLL 后启动被拦 | `ysAppGuard.dat` 完整性清单未刷新 | 在发证端执行"刷新完整性清单"。清单**缺失同样拒绝**，删掉 `.dat` 不能绕过 |
| 改了 Face 层控件指针名后行为异常 | 控件指针须与 `FindBlock` 块名一致 | 遵循「UI 开发约定」：`theDialog` / `theDlxFileName` / 控件指针 = 块名 |
| AI 流式 Markdown 未闭合代码围栏把后续段落吞进代码块 | 全量 parse 时 `` ``` `` 未闭合 | `DuiMarkdownView::SetStreaming(true)`：内部会预补全闭合符再解析；结束后 `SetStreaming(false)` 做最终解析。高频 `SetContent` 配合 `SetStreamThrottleMs` / `SetAnimationClock` |

> 对照：CUI 的同类文档中，"忘记把新 `.cpp` 写进工程"与"没有失效 API 导致改了不重绘"是两条最高频陷阱，
> 在 ysDui 中分别对应上表第 1 条与「三条不变量」第 3 条。

---

## 三之二、AI 对话相关控件

`DuiChatBubble` / `DuiChip` / `DuiTypingIndicator` 是为对话场景新增的控件，与既有控件组合使用：

| 控件 | 职责 | 搭配 |
| --- | --- | --- |
| `DuiChatList` | 变高虚拟列表：只测量/排版可见窗口内的条目，未测条目按固定估值参与偏移；`SetFollowBottom` 贴底；`UnseenCount()` 报告未读 | 条目放 `DuiChatBubble`；流式更新后对**该条**调 `InvalidateItem` |
| `DuiChatBubble` | 分侧、限宽、圆角、头像、元信息行（发送者/时间戳/状态）；**不改写正文的文字颜色** | 正文放 `DuiMarkdownView`；等待首个片段时正文放 `DuiTypingIndicator` |
| `DuiChip` | 单行标签：附件名、建议提问、模型名；可关闭 | `DuiFlow` 里排布；超长文本由控件自身裁剪 |
| `DuiTypingIndicator` | 三点脉冲，表达"正在生成" | 需要 `core::AnimationClock`；Gallery 由 `AttachPageAnimations` 统一接线，未接时钟时静态显示 |
| `DuiImageViewer` | 图片灯箱：锚点缩放、拖拽平移、贴合/原始/铺满、关闭**请求** | 铺满根节点即为灯箱；放在局部矩形里即内嵌预览 |

- **未读与"回到底部"**：`DuiChatList::UnseenCount()` 是**派生**值（已到末尾恒为 0，且会在到达末尾时把计数真正落零，
  避免"跳到底再上滚"时旧计数复现）。按钮与角标**由页面组合**（`DuiButton` 叠在列表上），
  用 `SetUnseenChangedHandler` 同步；参考 Gallery 的 ChatStream 页。
- **`DuiImageViewer` 不隐藏自己**：只调用 `SetCloseRequestedHandler`，由调用方决定隐藏/销毁/忽略，
  避免控件在回调栈里销毁自身。背板取主题槽 `ImageViewerBackdrop`（**必须**是带透明度的深色，
  取浅色会与页面同色而看不出层）。

- 输入区：`DuiEditHost` 开 `multiline + submitOnEnter` 即得"Enter 发送、Shift+Enter 换行"，
  用 `SetSubmitHandler` 接发送动作；附件用 `DuiChip` 放 `DuiFlow`；发送/停止按钮用 `DuiButton` 切换。
  这几件都能组合出来，**不需要**再做专用的"聊天输入框"控件——除非出现组合不出来的需求。
- 后台增量落地：`core::Host::Dispatcher()` 是唯一的跨线程通道，契约见 `THREADING.md`。
  流式增量用 `PostCoalesced(消息序号, …)`；**收尾也用同一个合并键**投递最终全文，
  这样顺序天然正确，不需要额外取消。只有"收尾走另一条通道"或"目标已销毁"时才用 `CancelCoalesced`。
- 参考实现：Gallery 的 **ChatStream** 页（`GalleryCatalog.cpp` 的 `ChatStreamDemo`）是一个可运行的最小对话页，
  含后台线程产出、派发、贴底跟随与销毁时的 join + `CancelCoalesced`；`ysdui_chat_stream_tests` 用真实线程验证同一条链路。
- 贴底跟随：`DuiScrollView::SetFollowBottom(true)` 让流式追加时视图停在末尾；用户把视图移离末尾会**自动关闭**该状态，
  位置回到末尾或调用 `ScrollToBottom()` 会重新开启。
- 主题槽：`ChatUserBubbleFill` / `ChatUserBubbleText` / `ChatAssistantBubbleFill` / `ChatAssistantBubbleText` /
  `ChatMetaText` / `ChipFill` / `ChipText` / `ChipBorder` / `ImageViewerBackdrop`。**新增角色或气泡配色时必须确认它与页面底色不同**，
  白底气泡在白底页面上等于隐形。

**`DuiChatList` 的使用边界（务必读）**

- 窗口外的条目**不会被 `Layout`**，因此也拿不到有效 `Bounds`；不要依赖 `Item(i)->Bounds()` 去判断条目是否存在，
  应该用 `Item(i) != nullptr` 与 `ItemRect(i)`（后者按偏移推算，任何下表都可用）。
- 窗口外条目在测量前按 `EstimatedItemHeight()` 估值，**滚动条长度会随沿途测量逐步收敛**；这是虚拟化的固有代价。
  需要精确滚动条长度就得放弃虚拟化。
- 改变条目内容后必须 `InvalidateItem(i)`，否则高度仍按旧值参与偏移（贴底时表现为"最后一屏不再跟到底"）。
- 估值是固定常量、不随测量漂移；这样"窗口之上条目的偏移在测量前后不变"，测量不会把用户正在看的内容挤动。
  若把估值改成"测量结果的平均值"，就会破坏这个不变量，改动前请先想清楚。
- `AppendItem` 是"新消息到达"路径（贴底关闭时计入未读）；`InsertItem` / `SetItemCount` 是**结构性装配**，
  会清除未读计数。用错会让"载入历史"把几百条都算成未读。

---

## 四、调试技巧

**ysDui（平台无关，无需 NX）**

- 全量单测：见 `CMakePresets.json` 的 `windows-msvc`（31 个 CTest，含公开头审计与其自检）。
- 界面不靠肉眼：`ysExeDuiGallery --capture-all <目录>` 离屏渲染 96 页 PNG，便于逐像素比对样式。
- 交互回归：`ysExeDuiGallery --run-tests`。
- 布局问题：在容器 `Layout()` 打印传入 `bounds` 与各子节点 `Bounds()`，对照 `DesiredSize()`。
- 绘制问题：先用 `FillRect` 把控件画满，确认 `Bounds()` 符合预期，再排查细节。

**NX 集成**

- 错误窗口默认关闭。临时打开并复原，不要改代码重编：

```cpp
const bool old = ysGetErrorInfoWindow();
ysSetErrorInfoWindow(true);
// ... 待排查的逻辑 ...
ysSetErrorInfoWindow(old);
```

- 纯记录信息用 `ysLogError` 系宏（自动带 `__FILE__` / `__LINE__` / `__FUNCTION__`）；
  只有需要解码 NX 错误或受窗口开关控制时才用上报宏。

**关于时序敏感的测试**

`ysdui_win32_*` 系列的测试依赖真实窗口与消息泵，历史上出现过"固定 `Sleep` 后直接断言"导致的假失败
（资源计数受 GDI 批处理影响、光标闪烁受 `WM_TIMER` 合并影响）。写这类断言时：
不要依赖单次采样，改用**有界轮询 + 只断言确定性的可观察结果**。

`UpdateWindow` / `GetUpdateRect` 在会话繁忙或窗口最小化残留下也会偶发"脏区明明应有却读不到 / 不派发
`WM_PAINT`"。验证 `WM_DISPLAYCHANGE` 丢缓冲后的重绘时，优先 `RedrawWindow(..., RDW_INVALIDATE |
RDW_UPDATENOW)` 并断言绘制回调次数，不要把 `UpdateWindow` 当成必然派发。

**关于窗口查找**

`::FindWindowW(类名, nullptr)` 返回的是**全系统**首个同类窗口，不是"刚刚创建的那个"。
同进程内的弹窗宿主、框架宿主或其它用例残留都会命中，导致断言随机器负载随机失败。
查找自己创建的窗口时，请按 **owner** 精确匹配（必要时叠加标题），
参考 `win32_ui_host_factory_tests.cpp` 的 `FindOwnedWindow()`。
