/**
 * 文件名：DuiNodeEditor.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明蓝图节点编辑器控件——网格画布上的节点/引脚/连线编辑。
 *
 * 与参考项目 imgui-node-editor 的差异：对方是即时模式（Begin/BeginNode/... 且由调用方
 * 自行增删查询），本控件是保持模式 + 数据模型驱动：编辑器持有 DuiNodeGraph，
 * 调用方通过 AddNode/AddLink 等修改模型，编辑器负责摆放、交互、绘制与撤销。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/controls/graph/DuiNodeGraph.hpp"
#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"
#include "ysDui/ui/DuiClipboard.hpp"
#include "ysDui/ui/DuiHostRef.hpp"

namespace ysDui::ui {
class IUiHostFactory;
class DuiTextInput;
}

namespace ysDui::controls::graph {

/** 引脚形状。 */
enum class DuiPinShape
{
    Circle, // 圆形（默认）
    Square, // 方形：适合表格型节点
    Arrow,  // 箭头：强调数据流向
};

/** 连线路由样式。 */
enum class DuiLinkRouting
{
    Bezier,     // 三次贝塞尔（默认）
    Orthogonal, // 正交折线（水平/竖直）
};

/**
 * 编辑器样式。
 * 不新增主题槽：颜色一律从 core::DuiTheme 现有槽取用，故四套主题预设自动适配。
 */
struct DuiNodeEditorStyle final
{
    int nodePadding{12};      // 节点内容内边距（画布单位）
    int nodeRounding{8};      // 节点圆角
    int headerHeight{32};     // 标题栏高度（含上下留白）
    int headerPaddingX{10};   // 标题栏左右内边距（画布单位）
    int headerPaddingY{6};    // 标题栏上下内边距（画布单位）
    /**
     * 标题栏左侧图标槽宽（画布单位）；有图标时文本从槽右侧起排。
     * 默认 0：当前未绘制图标，不预留空位。
     */
    int headerIconSlot{0};
    int pinRowHeight{24};     // 单个引脚行高
    int pinRadius{5};         // 引脚半径
    DuiPinShape pinShape{DuiPinShape::Circle}; // 引脚形状
    int minNodeWidth{160};    // 节点最小宽度
    int maxNodeWidth{320};    // 节点最大宽度
    /** 引脚内联控件的最大宽度；实际取 DesiredSize 宽度与该值的较小者。 */
    int pinWidgetMaxWidth{96};
    double linkStrength{0.5}; // 贝塞尔控制点距离系数（相对端点水平间距）
    DuiLinkRouting linkRouting{DuiLinkRouting::Bezier}; // 连线路由
    int linkThickness{2};     // 连线粗细
    int gridSize{24};         // 网格间距
    /** 连线流动标记的移动速度（画布单位/秒）。 */
    double flowSpeed{48.0};
    /** 流动标记的间距（画布单位）。 */
    int flowSpacing{32};
    /** 流动标记半径。 */
    int flowRadius{2};
    double minZoom{0.25};     // 缩放下限
    double maxZoom{4.0};      // 缩放上限
    double zoomStep{1.1};     // 单次滚轮缩放倍率
    /**
     * 历史字段：曾用于「偏离 1:1 时隐藏内嵌控件」。
     * 现已改为任意缩放都显示控件，保留该字段以免破坏已有 SetStyle 赋值。
     */
    double embeddedZoomTolerance{0.01};
};

/**
 * 蓝图节点编辑器。
 *
 * 交互约定：
 * - 左键：选择 / 框选 / 拖动节点 / 从引脚拉出连线（引脚有屏幕最小命中区；拉线靠近目标会吸附；
 *   可连接目标引脚会预亮）
 * - `Shift`+左键：加选节点；`Shift`+框选：在已有选择上追加
 * - `Ctrl`+左键：切换选中；`Ctrl`+拖动：复制选中节点并拖走副本；`Ctrl+D`：就地复制并偏移一格
 * - 从引脚拉出后按住 `Alt` 在空白处松手：断开该引脚上的连线
 * - 双击标题栏：重命名；双击节点其它区域：适配选中；双击空白：适配全部
 * - 右键点击节点：上下文菜单（剪切 / 复制 / 粘贴 / 删除 / 断开；需先 `SetPopupContext`）
 * - 中键拖动、空白处右键拖动：平移视图
 * - 滚轮：以光标为锚点缩放（光标在内嵌控件上时由控件处理，不缩放画布）
 * - `F`：适配全部内容；`F2` / 双击标题：重命名（注入 DuiTextInput 时走 IME）；`Ctrl+0` 复位为 1:1
 * - `Delete`：删除选中；`Ctrl+X` / `Ctrl+C` / `Ctrl+V`：剪切 / 复制 / 粘贴；`Ctrl+Z` / `Ctrl+Y`：撤销重做
 * - 右上角小地图：点击或拖动以平移视图
 * - `Ctrl+F`：按上次搜索词定位下一匹配；业务侧也可用 `FindNodesByTitle` / `FocusNode`
 * - 在 `DuiHost` 中点击画布会获得键盘焦点（需 `keyboardFocusable`）
 *
 * 布局契约：节点宽高在**画布坐标**下用未缩放的文本样式测量；绘制时再按当前缩放
 * 放大字号。缩放变化不会改变节点的画布尺寸，只改变屏幕投影。
 *
 * 内嵌控件按屏幕矩形摆放，任意缩放下都显示并可交互。SpinBox 等按 Bounds 高度等比
 * 缩放字号与按钮条，与节点标题/引脚标签保持同一视觉比例。
 */
class DuiNodeEditor final : public core::Control, public render::DuiRenderable
{
public:
    DuiNodeEditor();
    ~DuiNodeEditor() override;
    DuiNodeEditor(const DuiNodeEditor&) = delete;
    DuiNodeEditor& operator=(const DuiNodeEditor&) = delete;
    DuiNodeEditor(DuiNodeEditor&&) = delete;
    DuiNodeEditor& operator=(DuiNodeEditor&&) = delete;

    // --- 模型 ---
    /** @return 可修改的图模型；直接改动后请调用 NotifyGraphChanged()。 */
    [[nodiscard]] DuiNodeGraph& Graph();
    [[nodiscard]] const DuiNodeGraph& Graph() const;
    /** 整体替换模型并清空选择与撤销历史。 */
    void SetGraph(DuiNodeGraph graph);
    /** 外部直接改动 Graph() 后调用，用于刷新布局缓存与内嵌控件。 */
    void NotifyGraphChanged();

    // --- 视图 ---
    void SetStyle(DuiNodeEditorStyle style);
    [[nodiscard]] const DuiNodeEditorStyle& Style() const;
    void SetTextStyle(render::DuiTextStyle style);
    void SetTextMeasurer(render::DuiTextMeasurer* measurer);
    [[nodiscard]] double Zoom() const;
    /** 设置缩放；anchorScreen 为空时以控件中心为锚点。 */
    void SetZoom(double scale, core::Point anchorScreen = {});
    [[nodiscard]] core::Point PanOffset() const;
    void SetPanOffset(core::Point offset);
    /** 缩放平移以适配全部内容。 */
    void FrameAll();
    /** 缩放平移以适配当前选中内容。 */
    void FrameSelection();
    /** 复位为 1:1 并把内容居中。 */
    void ResetView();
    /** @return 画布坐标 <-> 控件客户区坐标。 */
    [[nodiscard]] core::Point ScreenToCanvas(core::Point point) const;
    [[nodiscard]] core::Point CanvasToScreen(core::Point point) const;
    /**
     * 按标题子串查找节点（含分组/注释）。
     * @param query 空串返回全部节点 id。
     * @param caseInsensitive 仅对 ASCII 字母忽略大小写。
     */
    [[nodiscard]] std::vector<DuiNodeId> FindNodesByTitle(std::string_view query,
                                                         bool caseInsensitive = true) const;
    /** 选中节点并适配到视图；节点不存在时返回 false。 */
    bool FocusNode(DuiNodeId node);
    /**
     * 查找并聚焦：空 query 复用上次搜索词；循环定位下一匹配。
     * 无匹配时返回 false。
     */
    bool FindAndFocus(std::string_view query = {});

    // --- 选择 ---
    void ClearSelection();
    [[nodiscard]] std::vector<DuiNodeId> SelectedNodes() const;
    [[nodiscard]] std::vector<DuiLinkId> SelectedLinks() const;
    [[nodiscard]] bool IsNodeSelected(DuiNodeId node) const;
    [[nodiscard]] bool IsLinkSelected(DuiLinkId link) const;
    void SelectNode(DuiNodeId node, bool append = false);
    void SelectLink(DuiLinkId link, bool append = false);

    // --- 编辑 ---
    /** 删除选中的节点与连线（删除节点会连带其引脚与相关连线）。只读模式下不做任何事。 */
    bool DeleteSelection();
    bool CopySelection();
    /**
     * 就地复制选中的普通节点（含两端都在选择集内的连线），副本偏移一格网格。
     * 等同 `Ctrl+D`。只读或无可复制节点时返回 false。
     */
    bool DuplicateSelection();
    /** 剪切选中内容：先复制到内部剪贴板再删除。只读模式下不做任何事。 */
    bool CutSelection();
    /** 断开选中节点上的连线，或删除选中连线。 */
    bool DisconnectSelection();
    /** 粘贴剪贴板内容；位置为画布坐标，为空时沿用原坐标并偏移。 */
    bool PasteClipboard(core::Point canvasPosition = {});
    [[nodiscard]] bool HasClipboardContent() const;
    /**
     * 注入系统剪贴板。复制时写入图 XML，粘贴时优先读取系统剪贴板中的图 XML。
     * 未注入时仍使用编辑器内部剪贴板。
     */
    void SetClipboard(ui::DuiClipboard* clipboard);
    /**
     * 注入平台文本输入（含 IME）。F2 / 双击标题时把原生输入叠在标题栏上。
     * 未注入时仍用按键缓冲（仅 ASCII），便于无宿主的单测。
     */
    void SetTextInput(ui::DuiTextInput* input);
    [[nodiscard]] ui::DuiTextInput* TextInput() const;
    /** 开始重命名当前选中的普通节点（等同 F2）。 */
    bool BeginRenameSelected();
    /** 显示或隐藏右上角小地图。 */
    void SetMinimapVisible(bool visible);
    [[nodiscard]] bool MinimapVisible() const;
    /**
     * 设置右键菜单所需的宿主工厂与所有者引用。
     * 未设置时右键节点不会弹出菜单（空白处仍可拖动画布）。
     */
    void SetPopupContext(ui::IUiHostFactory& factory, ui::HostRef owner);
    bool Undo();
    bool Redo();
    [[nodiscard]] bool CanUndo() const;
    [[nodiscard]] bool CanRedo() const;
    /** 撤销栈上限，超出后丢弃最早记录。 */
    void SetUndoLimit(int limit);
    /** 清空撤销与重做历史。 */
    void ClearHistory();

    /** 在方向与类型校验之后追加自定义连线约束；返回 false 则拒绝。 */
    void SetCanLinkFilter(std::function<bool(DuiPinId, DuiPinId)> filter);

    // --- 内嵌内容 ---
    /**
     * 设置节点内容工厂。
     * 编辑器为每个节点调用一次以创建内容控件，并作为自己的子节点托管（自行裁剪与路由事件）。
     * @param factory 返回新控件的函数；传入空函数表示取消内嵌内容。
     */
    void SetNodeContentFactory(std::function<std::unique_ptr<core::Control>(DuiNodeId, std::string_view)> factory);
    /**
     * 设置引脚内联控件工厂。
     * 用于把参数输入框放进引脚行（蓝图编辑器的典型形态）：输入引脚的控件右对齐、
     * 输出引脚的控件左对齐，标签占用剩余空间。返回空指针表示该引脚无内联控件。
     */
    void SetPinContentFactory(std::function<std::unique_ptr<core::Control>(DuiPinId, std::string_view)> factory);
    /** @return 指定引脚的内联控件；无则返回 nullptr。 */
    [[nodiscard]] core::Control* PinContent(DuiPinId pin) const;

    // --- 连线流动动画 ---
    /**
     * 注入动画时钟；未注入时连线不显示流动标记。
     * 流动标记是持续动画，会带来持续重绘，故按连线显式开启（对齐参考项目的 Flow 语义）。
     */
    void SetAnimationClock(core::AnimationClock* clock);
    /** 开启/关闭某条连线的流动标记。 */
    void SetLinkFlow(DuiLinkId link, bool enabled);
    [[nodiscard]] bool LinkFlow(DuiLinkId link) const;

    // --- 分组 ---
    /** 把分组矩形收缩到恰好容纳其内部节点（含内边距）。分组不存在时返回 false。 */
    bool FitGroupToContents(DuiNodeId group);

    // --- 交互开关 ---
    void SetReadOnly(bool readOnly);
    [[nodiscard]] bool ReadOnly() const;
    void SetGridVisible(bool visible);
    [[nodiscard]] bool GridVisible() const;
    /** 拖动节点时按网格吸附。 */
    void SetSnapToGrid(bool snap);
    [[nodiscard]] bool SnapToGrid() const;

    // --- 序列化 ---
    /** @return 模型的 UTF-8 XML 文本。 */
    [[nodiscard]] std::string ToXml() const;
    /** @return 是否解析成功；失败时保留原模型。 */
    bool FromXml(std::string_view xml);
    /** @return 是否写入成功；路径为 UTF-8。 */
    bool SaveToFile(const std::string& utf8Path) const;
    /** @return 是否读取成功；失败时保留原模型。 */
    bool LoadFromFile(const std::string& utf8Path);

    // --- 回调 ---
    void SetSelectionChangedHandler(std::function<void()> handler);
    /** 模型被编辑器修改后触发（含撤销/重做/粘贴后的结果）。 */
    void SetGraphChangedHandler(std::function<void()> handler);
    /** @return 鼠标当前悬停的节点/引脚/连线。 */
    [[nodiscard]] DuiPinId HoveredPin() const;
    [[nodiscard]] DuiNodeId HoveredNode() const;

    // --- 控件协议 ---
    [[nodiscard]] core::Size DesiredSize() const override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    /** 声明为可键盘聚焦的窗格，使 Host 在点击/Tab 时把按键路由到本控件。 */
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    class Impl;
    std::unique_ptr<Impl> editor_;
};

} // namespace ysDui::controls::graph
