# DuiSpreadsheet 企业数据操作契约

## 职责边界

`DuiSpreadsheet` 负责操作入口、选择定位、状态标记和右键菜单，不负责保存索引、排序数据或维护筛选后的行映射。
查找、替换、排序和筛选均由 `IDuiWorkbookModel` 执行；默认实现返回 `Unsupported`，不会在 UI 线程扫描
`100000 x 1000` 逻辑单元格。

模型通过 `Capabilities()` 明确声明能力，并实现对应方法：

- `FindCell()`：从 `startAfter` 之后开始，遵守方向、回绕、大小写和全单元格匹配选项。
- `ReplaceText()`：返回替换数量和首个修改位置；批量操作的事务性由模型保证。
- `ApplySort()`：在指定闭区间内执行一个或多个列关键字排序。
- `SetFilters()`：设置当前 Sheet 的列筛选；空数组表示清除筛选。

成功的替换、排序和筛选会触发控件重新加载视口。控件内部的单元格撤销历史不会伪造模型级操作撤销；需要撤销这些操作的宿主，必须在模型中提供事务或命令历史。

## 自动尺寸

大表模型应实现 `PreferredRowHeight()` 和 `PreferredColumnWidth()`，直接返回完整数据范围的预计算像素值。返回
`std::nullopt` 时，控件仅在用户显式调用 `AutoFitRow()`、`AutoFitColumn()` 或双击标题分隔线时执行兼容遍历：

- 设置了 `DuiTextMeasurer` 时使用真实文本度量。
- 未设置测量器时列宽采用保守字符宽度估算，行高保持默认单行高度。
- 最终尺寸仍由模型写入并回读，限制在 16 到 1000 像素。

兼容遍历与目标行或列的逻辑长度线性相关，不应作为超大表模型的生产路径。

## 宿主交互

- `SetPopupContext()` 启用标准右键菜单，包含剪贴板、清空、查找/替换请求、排序、筛选和自动尺寸。
- `SetFindRequestedHandler()` 与 `SetReplaceRequestedHandler()` 由宿主打开自己的查找面板；`Ctrl+F`、`Ctrl+H` 和菜单使用同一入口。
- `FindNext()`、`ReplaceText()`、`ApplySort()` 和 `SetFilters()` 返回结构化结果，菜单执行失败还可通过
  `SetWorkbookOperationErrorHandler()` 统一显示诊断。

控件不创建公式栏、文件选择器或 XLSX 对象；这些能力继续由宿主和独立适配器提供。
