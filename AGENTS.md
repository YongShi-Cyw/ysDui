# AGENTS.md

本文件为 Codex / Cursor 等 AI 编码助手提供仓库级指引。**以本文件描述的目录、架构边界与工程约定为准。**

## 1. 先思考，后编码

**不要假设，不要掩饰困惑，把取舍摆到台面上。**

在实现之前：

- 明确陈述假设；存在不确定性时先说明或提问。
- 存在多种解读时列出差异，不悄悄选择一种。
- 有更简单的实现时主动指出；有正当理由时可以反驳。
- 需求或边界不清楚时暂停，指出具体困惑并请求确认。

## 2. 简单至上

**用最少的代码解决问题，不做投机性开发。**

- 不添加需求之外的功能。
- 不为一次性代码创建抽象。
- 不增加未被要求的配置项或扩展点。
- 不为不可能发生的场景编写复杂错误处理。
- 如果实现明显超过问题本身所需的规模，应先简化再提交。

## 3. 手术式修改

**只动必须动的，只清理本次改动产生的残留。**

- 不顺手重构、格式化或改写相邻代码。
- 匹配既有代码风格，不因个人偏好批量改名。
- 发现无关废弃代码时可以说明，但未经要求不得删除。
- 删除本次改动导致的不再使用的 include、变量或函数。
- 每一行改动都应能直接追溯到当前需求。

## 4. 目标驱动执行

**先定义成功标准，再循环验证直至达成。**

将任务转化为可验证目标：

- “添加校验” -> 先为无效输入编写测试，再让测试通过。
- “修复 Bug” -> 先添加可稳定复现的回归测试，再修复实现。
- “重构 X” -> 确保重构前后的既有行为和测试结果一致。

多步骤任务开始前给出简短计划：

```text
1. [步骤] -> 验证：[检查点]
2. [步骤] -> 验证：[检查点]
3. [步骤] -> 验证：[检查点]
```

## 项目概述

ysDui v2 是从 BalloonUI 提交 `d1ab3f3` 迁移而来的 C++20 UI 库。项目采用破坏性 API 重构，目标是让公开 API、核心逻辑和渲染协议保持平台无关，同时通过独立后端保留 Windows 能力。

- `core`：控件树、布局、事件、状态、主题、动画时钟和宿主抽象。
- `render`：Canvas、文本、图像、路径、显示列表和资源渲染抽象。
- `controls`：仅依赖 core 与 render 的平台无关控件。
- `resource`：可选的 ZIP 资源包模块。
- `platform-win32`：窗口、消息转换、文本输入、GDI/GDI+、DPI、COM/OLE、UIA 和原生对话框实现。

依赖方向必须保持为 `core -> render -> controls`；平台后端可以依赖前三层，前三层不得依赖平台后端。不得修改 `D:\00-ClionCode\balloonui-main`，它只作为指定快照的行为与样式基线。

## 角色与质量基线

以资深 C++ 工程师标准设计和实现代码，默认使用 C++20，并按生产环境交付：

- 高内聚、低耦合、RAII、异常安全、const 正确、生命周期明确。
- 可测试、可维护，并最小化编译依赖。
- 避免超大文件、循环依赖和跨层实现泄漏。
- 对长期维护的公共 API 考虑 ABI；公共有状态类优先使用 PImpl。
- Windows 后端必须使用 MSVC 构建；无平台模块应能在不链接 Windows SDK 的配置下独立构建和测试。

## 公共 API 与平台边界

- `include/ysDui` 下的公开头只能依赖标准 C++ 库和 ysDui 自身公开头。
- 公开头严禁包含 Windows SDK、COM、OLE、ATL 或 WTL 头文件。
- 公开 API 严禁出现 `RECT`、`POINT`、`SIZE`、`COLORREF`、`HWND`、`HDC`、`WPARAM`、`LPARAM`、`LRESULT`、`HRESULT`、`IUnknown` 等平台类型。
- 不得用同名自定义类型规避扫描；使用 `Point`、`Size`、`Rect`、`Color`、`Event`、`Result` 和能力接口等精确的平台无关类型。
- 原生窗口、设备上下文、GDI/GDI+ 对象和 COM/OLE 对象只能存在于 Win32 后端私有实现中。
- 对外文本输入、输出和存储统一使用 UTF-8 `std::string`；新增公共 API 不得引入 `std::u16string`。
- 原生能力通过平台无关接口和工厂获取，上层控件不得感知 Child、Popup、Layered 或具体平台句柄。

## PImpl 与头文件纯净

对外暴露、跨模块或长期维护的有状态类优先使用 PImpl：

- `.hpp` 只公开接口、公有枚举或值类型和必要的前置声明。
- 私有数据与实现逻辑放入 `.cpp` 中定义的 `struct Impl`。
- 避免在公开头中暴露第三方库、平台库、实现容器或后端对象。
- 析构函数必须在 `Impl` 完整定义之后实现，避免 `std::unique_ptr<Impl>` 不完整类型问题。
- 默认禁止拷贝；确需复制时实现深拷贝或显式 `Clone()`。
- PImpl 类默认支持 `noexcept` 移动，资源持有类显式声明析构函数。
- 既有 PImpl 成员名保持原样，例如 `std::unique_ptr<Impl> avatar_;`，不得仅为统一前缀而批量改名。

```cpp
class UserService
{
public:
    UserService();
    ~UserService();

    UserService(const UserService&) = delete;
    UserService& operator=(const UserService&) = delete;
    UserService(UserService&&) noexcept;
    UserService& operator=(UserService&&) noexcept;

    void LoginUser();

private:
    struct Impl;
    std::unique_ptr<Impl> service_;
};
```

## C++ 设计与实现规范

### 智能指针与 RAII

- 所有权优先级为 `std::unique_ptr` > `std::shared_ptr` > 裸指针。
- 裸指针只能表达非占有关系，并由调用方保证生命周期。
- 禁止裸 `new` / `delete` 和手动 `malloc` / `free`；第三方 C API 必须使用 RAII 包装。
- 锁、文件和平台句柄必须使用 RAII，禁止手动跨多行 `lock` / `unlock`。

### const 与异常安全

- 不修改对象状态的方法加 `const`。
- 只读大对象参数优先使用 `const T&`，小型值类型按值传递。
- 修改复杂状态时先构造临时结果再提交，避免对象处于半失效状态。

### 返回值与错误表达

- 可能无结果时优先使用 `std::optional<T>`。
- 避免新增 `bool Func(T& out)` 风格接口，兼容既有接口除外。
- 复杂错误使用项目错误枚举、`std::error_code` 或现有 `Result` 抽象表达。

### 多线程

- 明确控件树、平台宿主和回调所在线程。
- 共享状态必须有清晰同步策略；回调和 I/O 不得在持锁状态下执行。
- 后台任务不得直接访问已销毁控件；取消、所有权和 UI 线程回收必须可验证。

### Include 顺序

```cpp
#include "CurrentClass.hpp"  // 当前文件对应头

#include <memory>             // 标准库
#include <string>

// 第三方库

#include "ysDui/core/control.hpp"  // 项目内部
```

公开头尽量少 `#include`，能用前置声明时优先前置声明。

### 禁止事项

- 禁止新增全局可变状态或滥用单例。
- 禁止在公开头中写 `using namespace`。
- 禁止 C 风格强制转换、C 风格数组、隐式窄化和未初始化变量。
- 禁止用宏实现业务逻辑或在公开头泄漏内部实现。
- 禁止让平台类型回流到 core、render、controls 或公开 API。

## 代码规范

### 花括号与缩进

- 使用 4 空格缩进。
- 左花括号独占一行；namespace 使用项目既有格式。
- 单行建议不超过 120 个字符。
- `if` / `for` / `while` 的单行语句体可不写花括号，多行语句体必须使用花括号。

### 命名

| 类型 | 规则 | 示例 |
|------|------|------|
| 命名空间 | 小写 | `ysdui::core` |
| 类名 | PascalCase | `DuiButton` |
| 公有函数 | 匹配现有公共 API | `SetText()` |
| 私有函数 | lowerCamelCase | `updateLayout()` |
| 新增普通成员 | 匹配所在文件既有风格 | `m_filePath` 或 `filePath_` |
| 参数 | lowerCamelCase | `userId` |
| 常量 | 匹配模块既有风格 | `DEFAULT_DELAY_MS` 或 `kDefaultDelayMs` |

不要为了统一命名风格修改需求范围外的既有符号。

### 注释与文件说明

- 新增 `.hpp` / `.cpp` 文件必须在开头写明文件名、开发者“青蓝”、开发时间和用途。
- 新增公共类和 public 方法使用 Doxygen 风格说明用途、参数和返回值。
- 注释使用中文和 UTF-8 编码，说明设计原因、边界和不直观逻辑，避免复述代码。
- 注释应充分覆盖公共契约与复杂实现；不得通过无意义注释机械凑比例。
- 枚举值应说明业务含义，必要的布局或协议常量应使用具名常量并说明用途。

## 验证与交付

- 改动前先读 `docs/PITFALLS.md`（三条不变量、`DesiredSize` 尺寸协议与陷阱速查表）。
- 修复 Bug 时优先添加回归测试；新增功能必须有与风险相称的测试。
- 涉及公开头时运行自包含编译和 `public_header_audit`。
- 涉及模块边界时扫描禁止的平台头和平台类型。
- 涉及 Gallery 样式时运行 `ysdui_gallery --run-tests`，并对相关页面执行截图验证。
- 完整阶段至少运行受影响目标构建、CTest、`git diff --check` 和工作区状态检查。
- 无法执行的验证必须明确说明，不得把未运行的测试描述为通过。
- 每个提交必须功能完整、业务可用；提交信息使用中文。
- 提交前检查暂存区，只提交本次任务文件，不覆盖或回退用户已有改动。

## 文件删除要求

禁止批量删除文件或目录，不得使用：

- `del /s`
- `rd /s`
- `rmdir /s`
- `Remove-Item -Recurse`
- `rm -rf`

需要删除时只能一次删除一个已确认的明确文件路径。若任务需要批量删除文件，必须停止并请求用户手动处理。
