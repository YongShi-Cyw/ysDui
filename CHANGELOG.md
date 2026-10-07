# ysDui 变更日志

本项目遵循语义化版本。2.x 内保持公开 API 源兼容；弃用接口至少保留一个 2.x 过渡周期，破坏性修改只能进入新的主版本。

## 未发布

- 为 `DuiSpreadsheet` 增加源兼容的查找替换、模型排序筛选、右键菜单和自动行列尺寸能力。
- 为 `DuiSpreadsheet` 增加完整内容边框和模型驱动的视口范围预取，避免大表首次加载全部数据。

- 建立严格编译、静态分析、ASan/UBSan、公开头和安装消费者门禁。
- 将回归测试拆分为独立模块目标，并补充生命周期测试。
- 为 XML 构建增加结构化错误，为 Theme、Skin、Menu 增加 RAII 订阅句柄。
- 为 Win32 UIA 增加 Invoke、Value 和 ExpandCollapse 操作模式。
- 将可选 `resource` 模块纳入安装导出，附带 minizip-ng/zlib-ng CMake 依赖、打包器和许可证文件。
- 增加资源安装消费者、libsodium 加密 CI，以及缺少项目许可证时阻断标签发布的工作流。
- 明确 Font Awesome Free 仅为 Gallery 外部运行时资源，不属于 SDK 内置或安装内容。
- 修正迁移状态中的 resource 发布门槛描述，并补充静态包发布清单。
- 拆分 `DuiSpreadsheet` 的编辑、指针、绘制、可访问性实现及生命周期测试职责。
- 为工作簿模型增加源兼容的稀疏行列尺寸快照，消除大表绑定、尺寸拖拽和历史操作的全量扫描。

## 2.0.0 - 2026-07-29

- 从 BalloonUI `d1ab3f3` 基线迁移为 C++20 静态库。
- 建立平台无关的 core、render、ui、controls 模块与私有 Win32 后端。
- 提供 Windows Gallery、64 页面视觉回归和 CMake 安装包。
