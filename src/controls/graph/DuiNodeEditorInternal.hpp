/**
 * 文件名：DuiNodeEditorInternal.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：DuiNodeEditor 的私有实现细节——视图变换、布局算法与交互状态。
 *       本头位于 src/ 下，不属于公开 API。
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ysDui/controls/graph/DuiNodeEditor.hpp"
#include "ysDui/controls/list/DuiMenu.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiCanvasTransform.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"
#include "ysDui/ui/DuiClipboard.hpp"
#include "ysDui/ui/DuiHostRef.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"
#include "ysDui/ui/DuiTextInput.hpp"

namespace ysDui::ui {
class IUiHostFactory;
}

namespace ysDui::controls::graph::detail {

/**
 * 画布与屏幕坐标之间的变换。
 * 绘制走 Canvas::PushTransform（同一套 DuiCanvasTransform）；命中测试仍显式换算。
 */
class GraphTransform final
{
public:
    double scale{1.0};    // 缩放系数，1.0 表示 1 画布单位 = 1 DIP
    core::Point offset{}; // 画布原点对应的屏幕坐标

    [[nodiscard]] render::DuiCanvasTransform ToCanvasTransform() const { return {scale, offset}; }

    [[nodiscard]] core::Point ToScreen(core::Point canvas) const
    {
        return ToCanvasTransform().MapPoint(canvas);
    }

    [[nodiscard]] core::Point ToCanvas(core::Point screen) const
    {
        return ToCanvasTransform().UnmapPoint(screen);
    }

    [[nodiscard]] core::Rect ToScreenRect(core::Rect canvas) const
    {
        return ToCanvasTransform().MapRect(canvas);
    }

    /** 长度换算：用于引脚半径、连线粗细等标量。 */
    [[nodiscard]] int ScaleLength(int canvasLength) const
    {
        return ToCanvasTransform().MapLength(canvasLength);
    }
};

/** 引脚在屏幕上的最小命中边长：缩小时圆点只有几像素，不放大则几乎点不中。 */
constexpr int kMinPinHitPx = 16;
/** 向节点外侧再扩一圈，便于从空白处抓住引脚。 */
constexpr int kPinHitOutwardPx = 6;
/** 拉线时对可连接引脚的吸附半径（屏幕像素）。 */
constexpr int kLinkMagnetPx = 22;

/**
 * 引脚命中矩形：视觉圆点仍按布局绘制，命中区保证不小于 kMinPinHitPx，并向外扩。
 * @param input true 为输入引脚（向外即向左），false 为输出引脚（向右）。
 */
[[nodiscard]] inline core::Rect PinHitScreenRect(const GraphTransform& transform,
                                                 core::Rect canvasPin, bool input)
{
    const core::Rect screen = transform.ToScreenRect(canvasPin);
    const int centerX = (screen.left + screen.right) / 2;
    const int centerY = (screen.top + screen.bottom) / 2;
    const int halfX = (std::max)(screen.Width(), kMinPinHitPx) / 2;
    const int halfY = (std::max)(screen.Height(), kMinPinHitPx) / 2;
    core::Rect hit{centerX - halfX, centerY - halfY, centerX + halfX, centerY + halfY};
    if (input)
        hit.left -= kPinHitOutwardPx;
    else
        hit.right += kPinHitOutwardPx;
    return hit;
}

/** 节点布局结果：画布坐标下的各部件矩形。 */
struct NodeLayout final
{
    core::Rect bounds{};                    // 整个节点
    core::Rect header{};                    // 标题栏
    std::vector<DuiPinId> inputOrder;       // 本节点输入引脚（按行自上而下）
    std::vector<DuiPinId> outputOrder;      // 本节点输出引脚
    std::vector<core::Rect> inputPins;      // 与 inputOrder 一一对应
    std::vector<core::Rect> outputPins;     // 与 outputOrder 一一对应
    std::vector<core::Rect> inputLabels;    // 引脚标签文本区
    std::vector<core::Rect> outputLabels;
    std::vector<core::Rect> inputWidgets;   // 引脚内联控件区（无控件时为空矩形）
    std::vector<core::Rect> outputWidgets;
    core::Rect content{};                   // 内嵌内容区（无内容时为空）
};

/** 交互状态机的当前阶段。 */
enum class Interaction
{
    None,
    Panning,        // 平移视图
    BoxSelecting,   // 框选
    DraggingNodes,  // 拖动节点
    DraggingGroup,  // 拖动分组
    CreatingLink,   // 从引脚拉出连线
    ResizingGroup,  // 拖动分组右下角调整尺寸
    NavigatingMinimap, // 在小地图上拖动以平移主视图
};

/** 悬停/命中的目标类别。 */
struct HitResult final
{
    DuiNodeId node{};
    DuiPinId pin{};
    DuiLinkId link{};
    DuiNodeId group{};
    bool onGroupResize{};
    bool onMinimap{};
    bool empty() const { return !node.Valid() && !pin.Valid() && !link.Valid() && !group.Valid()
        && !onMinimap; }
};

/**
 * 按缩放系数调整文本尺寸。
 * 必要性：渲染层没有画布变换，若不缩放字号，节点框会随缩放变小而文字保持绝对 DIP，
 * 导致小缩放下文字相对框体过大、引脚标签互相挤压（表现为"粗糙"）。
 */
[[nodiscard]] inline render::DuiTextStyle ScaledText(const render::DuiTextStyle& style, double scale)
{
    return render::DuiCanvasTransform{scale, {}}.MapText(style);
}

/** @return 节点内所有引脚中该方向的最大数量，用于计算行数。 */
[[nodiscard]] inline int MaxPinRows(int inputCount, int outputCount)
{
    return (std::max)(inputCount, outputCount);
}

/**
 * 生成连线的折线近似点列。
 * 说明：DuiPath 只有 MoveTo/LineTo/Close，没有弧线或贝塞尔指令，故三次贝塞尔必须折线逼近
 *       （与饼图扇区同一处理方式）。切向取水平方向，控制点距离 = 端点水平间距 × strength，
 *       并保底 minHandle——否则左右相邻很近的节点之间会退化成一条直线，看不出连接方向。
 * @param start 起点（屏幕坐标）
 * @param end 终点（屏幕坐标）
 * @param strength 控制点距离系数
 * @param segments 分段数（越大越平滑）
 * @param minHandle 控制点最小距离（屏幕像素）
 */
/**
 * 连线折线逼近的分段数：命中与流动标记共用。
 * 按端点欧氏距离估算，钳制到 [32, 96]。
 */
[[nodiscard]] inline int LinkSegmentCount(core::Point start, core::Point end)
{
    const double span = std::hypot(static_cast<double>(end.x - start.x),
                                   static_cast<double>(end.y - start.y));
    return (std::clamp)(static_cast<int>(span / 4.0), 32, 96);
}

/** 水平切向三次贝塞尔的两个控制点。 */
[[nodiscard]] inline double LinkHandleLength(core::Point start, core::Point end, double strength,
                                             double minHandle = 24.0)
{
    return (std::max)(minHandle, std::abs(static_cast<double>(end.x - start.x)) * strength);
}

[[nodiscard]] inline std::vector<core::Point> LinkPolyline(core::Point start, core::Point end,
                                                          double strength, int segments,
                                                          double minHandle = 24.0)
{
    segments = (std::clamp)(segments, 24, 96);
    const double handle = LinkHandleLength(start, end, strength, minHandle);
    const double c1x = start.x + handle;
    const double c2x = end.x - handle;
    std::vector<core::Point> points;
    points.reserve(static_cast<std::size_t>(segments) + 1);
    for (int index = 0; index <= segments; ++index)
    {
        const double t = static_cast<double>(index) / segments;
        const double u = 1.0 - t;
        const double x = u * u * u * start.x + 3.0 * u * u * t * c1x
            + 3.0 * u * t * t * c2x + t * t * t * end.x;
        const double y = u * u * u * start.y + 3.0 * u * u * t * start.y
            + 3.0 * u * t * t * end.y + t * t * t * end.y;
        points.push_back({static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y))});
    }
    return points;
}

/**
 * 正交折线：正向用水平中点 H-V-H；反向（终点在左）用带 stub 的绕行，避免线穿过节点。
 */
[[nodiscard]] inline std::vector<core::Point> OrthogonalPolyline(core::Point start, core::Point end,
                                                                 int minStub = 24)
{
    std::vector<core::Point> points;
    const int stub = (std::max)(minStub, 8);
    if (end.x >= start.x + stub)
    {
        const int midX = (start.x + end.x) / 2;
        points = {start, {midX, start.y}, {midX, end.y}, end};
    }
    else
    {
        const int midY = (start.y + end.y) / 2;
        points = {start,
                  {start.x + stub, start.y},
                  {start.x + stub, midY},
                  {end.x - stub, midY},
                  {end.x - stub, end.y},
                  end};
    }
    return points;
}

/** 按样式生成命中/流动/正交绘制共用的折线点列。 */
[[nodiscard]] inline std::vector<core::Point> BuildLinkPolyline(core::Point start, core::Point end,
                                                               const DuiNodeEditorStyle& style)
{
    if (style.linkRouting == DuiLinkRouting::Orthogonal)
        return OrthogonalPolyline(start, end);
    return LinkPolyline(start, end, style.linkStrength, LinkSegmentCount(start, end));
}

/** @return 点到线段的最短距离。 */
[[nodiscard]] inline double DistanceToSegment(core::Point point, core::Point a, core::Point b)
{
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.0)
        return std::hypot(static_cast<double>(point.x - a.x), static_cast<double>(point.y - a.y));
    double t = ((point.x - a.x) * dx + (point.y - a.y) * dy) / lengthSquared;
    t = (std::clamp)(t, 0.0, 1.0);
    const double projX = a.x + t * dx;
    const double projY = a.y + t * dy;
    return std::hypot(static_cast<double>(point.x) - projX, static_cast<double>(point.y) - projY);
}

/** @return 点到折线的最短距离。 */
[[nodiscard]] inline double DistanceToPolyline(const std::vector<core::Point>& points,
                                               core::Point point)
{
    if (points.empty())
        return 1e9;
    if (points.size() == 1)
        return std::hypot(static_cast<double>(point.x - points[0].x),
                          static_cast<double>(point.y - points[0].y));
    double best = 1e9;
    for (std::size_t index = 1; index < points.size(); ++index)
        best = (std::min)(best, DistanceToSegment(point, points[index - 1], points[index]));
    return best;
}

/**
 * 文本宽度测量：优先使用注入的测量器；未注入时按字号近似。
 * 与 DuiToast 的既有做法一致，保证控件在没有测量器时也能布局。
 */
[[nodiscard]] inline int MeasureWidth(render::DuiTextMeasurer* measurer, std::string_view text,
                                      const render::DuiTextStyle& style)
{
    if (text.empty())
        return 0;
    if (measurer != nullptr)
        return measurer->MeasureText(text, style, {}).size.width;
    return static_cast<int>(text.size()) * (std::max)(1, style.pointSize);
}

} // namespace ysDui::controls::graph::detail

namespace ysDui::controls::graph {

[[nodiscard]] std::string SerializeGraphXml(const DuiNodeGraph& graph);
[[nodiscard]] bool DeserializeGraphXml(std::string_view xml, DuiNodeGraph& graph);

/** 一次结构变更涉及的实体；撤销时反向应用。 */
struct StructureDelta final
{
    std::vector<DuiGraphNode> addedNodes;
    std::vector<DuiGraphPin> addedPins;
    std::vector<DuiGraphLink> addedLinks;
    std::vector<DuiGraphNode> removedNodes;
    std::vector<DuiGraphPin> removedPins;
    std::vector<DuiGraphLink> removedLinks;
    std::vector<std::pair<DuiNodeId, core::Point>> positionsBefore;
    std::vector<std::pair<DuiNodeId, core::Point>> positionsAfter;
    std::vector<std::pair<DuiNodeId, core::Size>> sizesBefore;
    std::vector<std::pair<DuiNodeId, core::Size>> sizesAfter;
};

/**
 * DuiNodeEditor 的私有实现。
 * 定义在内部头以便拆分到多个实现文件（绘制 / 输入 / 序列化）共享同一份状态。
 */
class DuiNodeEditor::Impl
{
public:
    using ContentEntry = std::pair<DuiNodeId, core::Control*>;
    using PinContentEntry = std::pair<DuiPinId, core::Control*>;
    using LayoutEntry = std::pair<DuiNodeId, detail::NodeLayout>;

    DuiNodeGraph graph;                              // 模型
    DuiNodeEditorStyle style;                        // 视图与绘制样式
    DuiNodeEditor* owner{};                          // 宿主控件：Impl 是非控件类，子节点增删需经它
    render::DuiTextStyle textStyle{{34, 34, 40, 255}, {}, 9, false};
    render::DuiTextMeasurer* measurer{};             // 文本测量器（可空）
    detail::GraphTransform transform;                // 画布 <-> 屏幕

    std::vector<DuiNodeId> selectedNodes;            // 选中的节点（含分组）
    std::vector<DuiLinkId> selectedLinks;            // 选中的连线
    std::vector<LayoutEntry> layouts;                // 布局缓存
    std::unordered_map<std::uint64_t, std::size_t> layoutIndex; // 节点 id -> layouts 下标
    std::vector<ContentEntry> contents;              // 节点内嵌控件（所有权在 Control 子节点表）
    std::vector<PinContentEntry> pinContents;        // 引脚内联控件（同上）

    // 连线流动动画（按连线显式开启，避免无条件持续重绘）
    core::AnimationClock* clock{};
    core::AnimationClock::TaskId flowTask{};
    double flowPhase{};                              // 0..1 循环相位
    std::vector<DuiLinkId> flowingLinks;

    // 交互状态
    detail::Interaction interaction{detail::Interaction::None};
    core::Point dragStartScreen{};                   // 本次拖动的起点（屏幕坐标）
    core::Point dragLastCanvas{};                    // 上次拖动位置（画布坐标）
    core::Point panStartOffset{};
    DuiPinId linkFrom{};                             // 拉线起点引脚
    core::Point linkCursorCanvas{};                  // 拉线时的光标（画布坐标）
    DuiPinId linkCandidate{};                        // 当前可落下的目标引脚
    bool linkCandidateValid{};
    /** 拉线期间可连接的目标引脚（相对 linkFrom），用于预亮。 */
    std::unordered_set<std::uint64_t> compatiblePins;
    core::Rect selectionBox{};                       // 框选矩形（屏幕坐标）
    DuiNodeId resizingGroup{};                       // 正在调整尺寸的分组
    /** 拖动/缩放尚未写入撤销栈；首次产生位移时记录增量。 */
    bool dragHistoryPending{};
    /** 拖动开始时各节点位置，用于增量撤销。 */
    std::vector<std::pair<DuiNodeId, core::Point>> dragPositionsBefore;
    /** Ctrl+拖动：首次位移时复制选中节点，松手再提交结构增量。 */
    bool dragDuplicate{};
    StructureDelta dragDuplicateDelta;
    /** Ctrl+点击已选中节点：无拖动则在抬起时取消选中。 */
    DuiNodeId pendingToggle{};

    detail::HitResult hovered;                       // 当前悬停目标

    /** 撤销记录：只存变更，不整图拷贝。 */
    enum class HistoryKind
    {
        Structure, // 增删节点/引脚/连线，或分组几何
        Positions, // 仅节点位置（拖动）
        Title,     // 仅标题（F2 重命名）
    };
    struct HistoryRecord final
    {
        HistoryKind kind{HistoryKind::Structure};
        StructureDelta structure;
        std::vector<std::pair<DuiNodeId, core::Point>> positionsBefore;
        std::vector<std::pair<DuiNodeId, core::Point>> positionsAfter;
        DuiNodeId titleNode{};
        std::string titleBefore;
        std::string titleAfter;
    };

    std::vector<HistoryRecord> undoStack;
    std::vector<HistoryRecord> redoStack;
    int undoLimit{50};
    DuiNodeGraph clipboard;
    bool hasClipboard{};
    int pasteGeneration{};
    ui::DuiClipboard* systemClipboard{};

    DuiNodeId renamingNode{};
    std::string renameBuffer;
    ui::DuiTextInput* renameInput{};
    bool renameSelecting{};
    bool renameClosing{};
    bool minimapVisible{true};
    std::vector<std::pair<DuiNodeId, core::Size>> dragSizesBefore;
    /** 搜索：上次查询与循环定位下标。 */
    std::string searchQuery;
    int searchCursor{-1};

    // 配置与回调
    std::function<std::unique_ptr<core::Control>(DuiNodeId, std::string_view)> contentFactory;
    std::function<std::unique_ptr<core::Control>(DuiPinId, std::string_view)> pinContentFactory;
    std::function<void()> selectionChanged;
    std::function<void()> graphChanged;
    /** 额外连线约束；返回 false 则拒绝。在方向/类型校验通过后调用。 */
    std::function<bool(DuiPinId, DuiPinId)> canLinkFilter;
    bool readOnly{};
    bool gridVisible{true};
    bool snapToGrid{};
    bool selectionDirty{};                           // 选择变化待通知
    /** 视图是否仍处于自动适配状态；用户一旦手动缩放/平移即置 false。 */
    bool autoFit{true};

    // 右键菜单（需宿主注入 popupFactory）
    ui::IUiHostFactory* popupFactory{};
    ui::HostRef popupOwner;
    std::unique_ptr<list::DuiMenu> contextMenu;

    // --- 由各实现文件定义 ---
    /** 重新计算全部节点布局并把尺寸写回模型，同时同步内嵌控件的位置与可见性。 */
    void LayoutGraph();
    /** 仅按当前变换把内嵌控件摆到屏幕矩形；平移/缩放时调用，避免重测文本。 */
    void PlaceEmbeddedControls();
    /** 把指定节点的画布布局整体平移；用于拖动，避免整图重测。 */
    void TranslateLayouts(const std::vector<DuiNodeId>& nodes, core::Point delta);
    /** @return 指定节点的布局；未命中返回 nullptr。 */
    [[nodiscard]] const detail::NodeLayout* LayoutOf(DuiNodeId node) const;
    /** @return 引脚圆心（画布坐标）；引脚或布局缺失时返回原点。 */
    [[nodiscard]] core::Point PinCenter(DuiPinId pin) const;
    /** 屏幕坐标命中测试；顶层优先。 */
    [[nodiscard]] detail::HitResult HitTest(core::Point screen) const;
    /**
     * 拉线吸附：在屏幕半径内找最近的可连接引脚。
     * 不命中精确引脚时使用，避免必须对准几像素的圆点才能松手。
     */
    [[nodiscard]] DuiPinId FindLinkMagnetPin(core::Point screen, DuiPinId from) const;
    /** 拉线落点：已命中引脚优先，否则走吸附。 */
    [[nodiscard]] DuiPinId ResolveLinkTarget(core::Point screen, DuiPinId from) const;
    /** 重建内嵌控件：为新增节点/引脚创建内容，移除已删节点/引脚的内容。 */
    void SyncContents();
    /** @return 指定引脚的内联控件；无则返回 nullptr。 */
    [[nodiscard]] core::Control* PinContentOf(DuiPinId pin) const;
    /** 内嵌控件当前是否可见。 */
    [[nodiscard]] bool EmbeddedVisible() const;
    /** 普通节点绘制顺序：未选中在下、选中/悬停在上；末项为最顶层。 */
    [[nodiscard]] std::vector<DuiNodeId> NodePaintOrder() const;
    /** 内嵌控件屏幕矩形是否被更上层节点挡住。 */
    [[nodiscard]] bool WidgetOccluded(DuiNodeId ownerNode, core::Rect screenBounds) const;
    /** 方向、类型与可选过滤器均通过时允许连线。 */
    [[nodiscard]] bool CanCreateLink(DuiPinId start, DuiPinId end) const;
    /** 按当前 linkFrom 重建可连接引脚集合。 */
    void RefreshCompatiblePins();
    /** @return 拉线预亮集合是否包含该引脚。 */
    [[nodiscard]] bool IsCompatibleLinkPin(DuiPinId pin) const;
    /** 按 flowPhase 重排流动动画任务；无流动连线时取消任务。 */
    void UpdateFlowAnimation();
    /** 流动动画单帧回调。 */
    void OnFlowTick(double progress);

    // 历史
    void PushHistory(HistoryRecord record);
    void CommitStructure(StructureDelta delta);
    /** 建立连线并记下被顶替的旧连线，供增量撤销。失败返回 false。 */
    bool CommitNewLink(DuiPinId start, DuiPinId end);
    /** 断开引脚上全部连线并记入撤销。无连线时返回 false。 */
    bool CommitBreakPinLinks(DuiPinId pin);
    void CommitPositions(std::vector<std::pair<DuiNodeId, core::Point>> before);
    void CommitTitle(DuiNodeId node, std::string before, std::string after);
    void ApplyPositions(const std::vector<std::pair<DuiNodeId, core::Point>>& positions);
    void ApplyStructure(const StructureDelta& delta);
    [[nodiscard]] std::vector<std::pair<DuiNodeId, core::Point>> CapturePositions(
        const std::vector<DuiNodeId>& nodes) const;
    void NotifyGraphChanged();                       // 修改完成后调用

    [[nodiscard]] core::Rect MinimapRect() const;
    void PanFromMinimap(core::Point screen);
    [[nodiscard]] core::Rect RenameScreenRect() const;
    void PlaceRenameInput();
    void BindRenameInput();
    void UnbindRenameInput();
    bool BeginRename();
    void FinishRename(bool commit);
    void HandleRenameText(std::string_view text);

    // 编辑
    bool DeleteSelection();
    bool CopySelection();
    /**
     * 复制选中普通节点（含内部连线），副本按 offset 平移；选中集改为副本。
     * @param offset 画布位移；{0,0} 表示与原稿重叠（供 Ctrl+拖动）。
     * @param outDelta 写出新增实体，调用方决定立刻 Commit 或拖完再 Commit。
     */
    bool CloneSelectedNormals(core::Point offset, StructureDelta& outDelta);
    /** 把当前选中的普通节点就地复制一份并改为选中副本；供 Ctrl+拖动。 */
    bool DuplicateSelectionForDrag();
    /** 就地复制并偏移一格，立刻记入撤销；供 Ctrl+D。 */
    bool DuplicateSelection();
    bool CutSelection();
    bool PasteClipboard(core::Point canvasPosition);
    /** 断开选中节点上的连线，或删除选中连线。 */
    bool DisconnectSelection();
    /** 在指定屏幕点弹出上下文菜单。 */
    bool ShowContextMenu(core::Point point);
    void ClearSelection();
    [[nodiscard]] bool IsNodeSelected(DuiNodeId node) const;
    [[nodiscard]] bool IsLinkSelected(DuiLinkId link) const;
    void SelectNode(DuiNodeId node, bool append);
    void SelectLink(DuiLinkId link, bool append);
};

} // namespace ysDui::controls::graph
