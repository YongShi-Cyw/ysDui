# DuiSpreadsheet 文件互操作边界

## 目标

`DuiSpreadsheet` 只负责工作表视口、输入、选择和模型写入，不解析文件格式，也不持有文件句柄。
CSV、TSV 和 XLSX 均由宿主显式调用的独立适配器完成，适配器通过
`IDuiWorkbookModel` 读取或写入数据后，再由宿主调用 `ReloadFromModel()`。

## 视口数据请求

`DuiSpreadsheet` 通过 `IDuiWorkbookModel::RequestCellRanges()` 通知宿主准备当前视口数据。请求只包含
当前可见行、当前可见列及右侧额外两列；冻结区与滚动区保持为独立矩形，避免滚动到远端后请求
中间不可见的大段单元格。重复绘制或未跨越逻辑行列的像素滚动不会重复请求。

模型可以同步准备数据，也可以复制请求范围后异步加载。异步完成后必须在 UI 线程通过既有模型
变更通知触发控件重载和宿主重绘。未准备的数据继续由 `CellPresentation()` 返回当前值或空值；控件
不会缓存业务数据，也不会替模型管理异步任务生命周期。

这样可避免文件 I/O、压缩包、公式和第三方 XLSX 类型进入控件的绘制或事件路径。

## 分层与所有权

```text
业务文件/流
    -> CSV/TSV/XLSX 适配器
    -> IDuiWorkbookModel（宿主拥有）
    -> DuiSpreadsheet（仅注入并显示模型）
```

- 适配器不得依赖 `DuiSpreadsheet`、Canvas、Host 或 Win32。
- `DuiSpreadsheet` 不提供“打开文件”或“保存文件”成员函数。
- 宿主决定流、编码、路径、线程、取消和错误显示；完成模型修改后在 UI 线程调用
  `ReloadFromModel()`。
- 适配器写入失败必须返回结构化结果，不得只返回空字符串、空工作表或 `false`。

## 稀疏行列尺寸

企业数据操作、右键菜单和自动尺寸的模型契约见 `DUI_SPREADSHEET_OPERATIONS.md`。

大工作表模型应实现 `SparseRowSizes()` 和 `SparseColumnSizes()`，一次返回默认像素值与少量
覆盖项。覆盖项索引从 0 开始；越界项会被忽略，重复索引以后出现的值为准，所有尺寸仍会
限制在 16 到 1000 像素。控件只按覆盖项数量分配尺寸缓存，并通过二分查找完成偏移和命中
定位，不按逻辑行列总数创建偏移数组。

返回 `std::nullopt` 时，控件继续逐项调用既有 `RowHeight()` 或 `ColumnWidth()`，因此已有模型
无需修改即可重新编译。这个回退路径与逻辑行列数量线性相关，仅用于兼容；`100000 x 1000`
等大范围模型应提供稀疏快照。宿主直接修改默认尺寸或覆盖集合后，仍需触发模型变更订阅或在
UI 线程调用 `ReloadFromModel()`。

拖拽、撤销和重做成功后，控件只回读被修改的目标尺寸一次，并以模型最终值更新内部缓存。
冻结到大索引不会遍历全部冻结行列，单帧绘制和模型单元格读取量仍由视口内可见单元格决定。

## 统一适配器契约

后续公共适配器采用下列语义，而不是让格式实现直接暴露第三方对象：

```cpp
enum class DuiWorkbookTransferErrorCode
{
    None,
    InvalidEncoding,
    ParseError,
    UnsupportedFeature,
    WorksheetNotFound,
    ModelWriteRejected,
    IoError,
};

struct DuiWorkbookTransferError
{
    DuiWorkbookTransferErrorCode code;
    std::string message;      // UTF-8
    DuiWorksheetId worksheet;
    DuiCellAddress cell;
    std::size_t byteOffset;
    int line;
    int column;
};
```

导出结果携带 UTF-8 文本或由宿主提供的字节流；导入结果携带实际写入的范围。任何
`ModelWriteRejected` 必须保留模型提供的 `DuiWorkbookWriteErrorCode` 和 UTF-8 消息。
当前 `IDuiWorkbookModel` 没有事务接口，因此导入遇到模型拒绝时不能声称自动回滚；
适配器必须报告已经写入的范围。需要原子导入的宿主应在自己的可事务模型中执行。

## CSV 与 TSV

首个实现可只支持一个指定 Sheet 的 UTF-8 文本导入导出：

- CSV 使用 RFC 4180 风格双引号转义，支持字段内逗号、引号和换行。
- TSV 使用制表符分隔；字段包含制表符、引号或换行时沿用双引号转义。
- 导出读取 `CellText()`，不写入展示格式、冻结状态、选择状态或 Sheet UI 状态。
- 导入写入 `SetCellTextWithResult()`，以模型文本作为唯一可移植值类型。
- 空字段映射为空 UTF-8 文本；空行必须保留其行位置。
- 首版不猜测区域性数字、日期、公式或编码；非 UTF-8 输入返回 `InvalidEncoding`。

CSV/TSV 适配器属于可选的独立静态库目标，例如 `ysdui_workbook_text`，只链接
`ysdui_core`。它不应成为 `ysdui_controls` 或安装消费者的隐式依赖。

## XLSX

XLSX 必须保持为单独目标，例如 `ysdui_workbook_xlsx`，并满足以下准入条件后才可加入：

- 依赖有固定版本、来源、许可证、SHA-256 与更新流程。
- 适配器公开头不泄漏 ZIP、XML、日期或第三方 XLSX 类型。
- 工作簿、公式、样式和共享字符串属于适配器或宿主模型能力，不能进入
  `DuiSpreadsheet`。
- 首版允许只读写纯文本 Sheet；遇到公式、合并单元格、富文本或不支持样式时返回
  `UnsupportedFeature`，不得静默丢失后报告成功。
- 大文件解析在后台线程完成，最终模型更新和 `ReloadFromModel()` 回到 UI 线程执行。

## 验证门禁

CSV/TSV 实现提交前至少覆盖：

- 中文、阿拉伯文、组合字符和 emoji 的 UTF-8 往返。
- 逗号、制表符、引号、空字段、空行和字段内换行。
- 非法引号、截断输入和错误位置。
- 模型拒绝写入时的错误枚举、消息、Sheet、单元格和已写入范围。
- `100000 x 1000` 逻辑范围的流式导出，不按总单元格分配内存。

XLSX 适配器还必须增加篡改包、缺失工作表、错误编码和安装消费者回归。
