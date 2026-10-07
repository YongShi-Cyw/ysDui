#include "ysDui/controls/graph/DuiNodeEditor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "DuiNodeEditorInternal.hpp"
#include "ysDui/controls/input/DuiDoubleSpinBox.hpp"
#include "ysDui/controls/input/DuiEditHost.hpp"
#include "ysDui/controls/input/DuiSpinBox.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::graph {
using namespace detail;
namespace {

/** 节点右键菜单命令标识。 */
enum class ContextCommand : std::uint32_t
{
    Cut = 1,        // 剪切
    Copy = 2,       // 复制
    Paste = 3,      // 粘贴
    Delete = 4,     // 删除
    Disconnect = 5, // 断开连线
};

void OffsetRect(core::Rect& rect, core::Point delta)
{
    if (rect.Empty())
        return;
    rect.left += delta.x;
    rect.top += delta.y;
    rect.right += delta.x;
    rect.bottom += delta.y;
}

template <typename T, typename Id>
void PushUniqueById(std::vector<T>& list, const T& item, Id T::* id)
{
    for (const T& existing : list)
    {
        if (existing.*id == item.*id)
            return;
    }
    list.push_back(item);
}

void CaptureNodeRemoval(const DuiNodeGraph& graph, DuiNodeId node, StructureDelta& delta)
{
    const DuiGraphNode* entry = graph.FindNode(node);
    if (entry == nullptr)
        return;
    PushUniqueById(delta.removedNodes, *entry, &DuiGraphNode::id);
    for (const DuiPinId pinId : graph.PinsOf(node))
    {
        const DuiGraphPin* pin = graph.FindPin(pinId);
        if (pin != nullptr)
            PushUniqueById(delta.removedPins, *pin, &DuiGraphPin::id);
    }
    for (const DuiLinkId linkId : graph.LinksOfNode(node))
    {
        const DuiGraphLink* link = graph.FindLink(linkId);
        if (link != nullptr)
            PushUniqueById(delta.removedLinks, *link, &DuiGraphLink::id);
    }
}

void CaptureExclusiveLinks(const DuiNodeGraph& graph, DuiPinId start, DuiPinId end,
                           std::vector<DuiGraphLink>& out)
{
    const auto take = [&](DuiPinId pin)
    {
        const DuiGraphPin* entry = graph.FindPin(pin);
        if (entry == nullptr || entry->multipleConnections)
            return;
        for (const DuiLinkId id : graph.LinksOfPin(pin))
        {
            const DuiGraphLink* link = graph.FindLink(id);
            if (link != nullptr)
                PushUniqueById(out, *link, &DuiGraphLink::id);
        }
    };
    take(start);
    take(end);
}

bool StructureDeltaEmpty(const StructureDelta& delta)
{
    return delta.addedNodes.empty() && delta.addedPins.empty() && delta.addedLinks.empty()
        && delta.removedNodes.empty() && delta.removedPins.empty() && delta.removedLinks.empty()
        && delta.positionsBefore.empty() && delta.sizesBefore.empty();
}

StructureDelta SwapStructure(StructureDelta delta)
{
    using std::swap;
    swap(delta.addedNodes, delta.removedNodes);
    swap(delta.addedPins, delta.removedPins);
    swap(delta.addedLinks, delta.removedLinks);
    swap(delta.positionsBefore, delta.positionsAfter);
    swap(delta.sizesBefore, delta.sizesAfter);
    return delta;
}

/**
 * 同步内嵌控件的平台代理（如 SpinBox 的 Win32 TextInput）。
 * 仅 SetBounds/SetVisible 不会更新原生子窗口，会导致文本框停在旧位置或
 * 在控件已隐藏后仍浮在节点之上/之下，表现为「控件被节点挡住」。
 */
void SyncEmbeddedControl(core::Control& control)
{
    const core::Rect bounds = control.Bounds();
    if (auto* spin = dynamic_cast<controls::input::DuiSpinBox*>(&control))
    {
        spin->Layout(bounds);
        return;
    }
    if (auto* spin = dynamic_cast<controls::input::DuiDoubleSpinBox*>(&control))
    {
        spin->Layout(bounds);
        return;
    }
    if (auto* edit = dynamic_cast<controls::input::DuiEditHost*>(&control))
        edit->Layout(bounds);
}

/** 被上层节点挡住时把原生输入框移出屏幕，避免 HWND 永远盖住其它节点。 */
void ParkNativeInput(core::Control& control, bool park)
{
    const core::Rect parked{};
    if (auto* spin = dynamic_cast<controls::input::DuiSpinBox*>(&control))
    {
        if (park && spin->TextInput() != nullptr)
            spin->TextInput()->SetBounds(parked);
        return;
    }
    if (auto* spin = dynamic_cast<controls::input::DuiDoubleSpinBox*>(&control))
    {
        if (park && spin->TextInput() != nullptr)
            spin->TextInput()->SetBounds(parked);
        return;
    }
    if (auto* edit = dynamic_cast<controls::input::DuiEditHost*>(&control))
    {
        if (park)
            edit->Layout(parked);
    }
}

} // namespace

DuiNodeEditor::DuiNodeEditor() : editor_(std::make_unique<Impl>())
{
    editor_->owner = this;
    editor_->contextMenu = std::make_unique<list::DuiMenu>();
    editor_->contextMenu->SetItemInvokedHandler([this](std::uint32_t command)
    {
        switch (static_cast<ContextCommand>(command))
        {
        case ContextCommand::Cut:
            (void)CutSelection();
            break;
        case ContextCommand::Copy:
            (void)CopySelection();
            break;
        case ContextCommand::Delete:
            (void)DeleteSelection();
            break;
        case ContextCommand::Paste:
            (void)PasteClipboard({});
            break;
        case ContextCommand::Disconnect:
            (void)DisconnectSelection();
            break;
        }
    });
}

DuiNodeEditor::~DuiNodeEditor()
{
    if (editor_)
    {
        editor_->FinishRename(false);
        if (editor_->contextMenu)
        {
            editor_->contextMenu->SetItemInvokedHandler({});
            editor_->contextMenu->Hide();
        }
    }
}

// ---------------------------------------------------------------- 模型

DuiNodeGraph& DuiNodeEditor::Graph() { return editor_->graph; }
const DuiNodeGraph& DuiNodeEditor::Graph() const { return editor_->graph; }

void DuiNodeEditor::SetGraph(DuiNodeGraph graph)
{
    editor_->graph = std::move(graph);
    editor_->ClearSelection();
    editor_->undoStack.clear();
    editor_->redoStack.clear();
    editor_->NotifyGraphChanged();
}

void DuiNodeEditor::NotifyGraphChanged() { editor_->NotifyGraphChanged(); }

void DuiNodeEditor::Impl::NotifyGraphChanged()
{
    graph.RefreshGroupMembership();
    SyncContents();
    LayoutGraph();
    if (graphChanged)
        graphChanged();
}

// ---------------------------------------------------------------- 视图

void DuiNodeEditor::SetStyle(DuiNodeEditorStyle style)
{
    style.nodePadding = (std::max)(0, style.nodePadding);
    style.nodeRounding = (std::max)(0, style.nodeRounding);
    style.headerHeight = (std::max)(0, style.headerHeight);
    style.headerPaddingX = (std::max)(0, style.headerPaddingX);
    style.headerPaddingY = (std::max)(0, style.headerPaddingY);
    style.headerIconSlot = (std::max)(0, style.headerIconSlot);
    style.pinRowHeight = (std::max)(1, style.pinRowHeight);
    style.pinRadius = (std::max)(1, style.pinRadius);
    style.minNodeWidth = (std::max)(1, style.minNodeWidth);
    style.maxNodeWidth = (std::max)(style.minNodeWidth, style.maxNodeWidth);
    style.linkStrength = (std::clamp)(style.linkStrength, 0.0, 1.0);
    style.linkThickness = (std::max)(1, style.linkThickness);
    style.gridSize = (std::max)(4, style.gridSize);
    style.minZoom = (std::clamp)(style.minZoom, 0.05, 1.0);
    style.maxZoom = (std::max)(style.minZoom, style.maxZoom);
    style.zoomStep = (std::clamp)(style.zoomStep, 1.001, 4.0);
    style.embeddedZoomTolerance = (std::max)(0.0, style.embeddedZoomTolerance);
    editor_->style = style;
    editor_->transform.scale = (std::clamp)(editor_->transform.scale, style.minZoom, style.maxZoom);
    editor_->LayoutGraph();
}

const DuiNodeEditorStyle& DuiNodeEditor::Style() const { return editor_->style; }

void DuiNodeEditor::SetTextStyle(render::DuiTextStyle style)
{
    editor_->textStyle = std::move(style);
    editor_->LayoutGraph();
}
void DuiNodeEditor::SetTextMeasurer(render::DuiTextMeasurer* measurer)
{
    editor_->measurer = measurer;
    editor_->LayoutGraph();
}

double DuiNodeEditor::Zoom() const { return editor_->transform.scale; }

void DuiNodeEditor::SetZoom(double scale, core::Point anchorScreen)
{
    const double next = (std::clamp)(scale, editor_->style.minZoom, editor_->style.maxZoom);
    if (std::abs(next - editor_->transform.scale) < 1e-9)
        return;
    // 用户一旦手动缩放，即退出自动适配
    editor_->autoFit = false;
    const core::Rect bounds = Bounds();
    if (anchorScreen.x == 0 && anchorScreen.y == 0)
        anchorScreen = {bounds.left + bounds.Width() / 2, bounds.top + bounds.Height() / 2};
    // 保持锚点处的画布坐标不变：先取锚点画布坐标，改缩放后反解偏移
    const core::Point anchorCanvas = editor_->transform.ToCanvas(anchorScreen);
    editor_->transform.scale = next;
    editor_->transform.offset = {anchorScreen.x - static_cast<int>(std::lround(anchorCanvas.x * next)),
                                 anchorScreen.y - static_cast<int>(std::lround(anchorCanvas.y * next))};
    editor_->PlaceEmbeddedControls();
}

core::Point DuiNodeEditor::PanOffset() const { return editor_->transform.offset; }
void DuiNodeEditor::SetPanOffset(core::Point offset)
{
    editor_->autoFit = false;
    editor_->transform.offset = offset;
    editor_->PlaceEmbeddedControls();
}

core::Point DuiNodeEditor::ScreenToCanvas(core::Point point) const
{
    return editor_->transform.ToCanvas(point);
}
core::Point DuiNodeEditor::CanvasToScreen(core::Point point) const
{
    return editor_->transform.ToScreen(point);
}

namespace {
bool AsciiContainsIgnoreCase(std::string_view haystack, std::string_view needle)
{
    if (needle.empty())
        return true;
    if (needle.size() > haystack.size())
        return false;
    const auto lower = [](unsigned char ch) -> char
    {
        return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : static_cast<char>(ch);
    };
    for (std::size_t index = 0; index + needle.size() <= haystack.size(); ++index)
    {
        bool match = true;
        for (std::size_t offset = 0; offset < needle.size(); ++offset)
        {
            if (lower(static_cast<unsigned char>(haystack[index + offset]))
                != lower(static_cast<unsigned char>(needle[offset])))
            {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}
} // namespace

std::vector<DuiNodeId> DuiNodeEditor::FindNodesByTitle(std::string_view query,
                                                      bool caseInsensitive) const
{
    std::vector<DuiNodeId> matches;
    for (const auto& node : editor_->graph.Nodes())
    {
        if (query.empty())
        {
            matches.push_back(node.id);
            continue;
        }
        const bool hit = caseInsensitive ? AsciiContainsIgnoreCase(node.title, query)
                                         : node.title.find(query) != std::string::npos;
        if (hit)
            matches.push_back(node.id);
    }
    return matches;
}

bool DuiNodeEditor::FocusNode(DuiNodeId node)
{
    if (editor_->graph.FindNode(node) == nullptr)
        return false;
    editor_->SelectNode(node, false);
    if (editor_->selectionChanged)
        editor_->selectionChanged();
    FrameSelection();
    return true;
}

bool DuiNodeEditor::FindAndFocus(std::string_view query)
{
    if (!query.empty())
    {
        editor_->searchQuery.assign(query.data(), query.size());
        editor_->searchCursor = -1;
    }
    if (editor_->searchQuery.empty())
        return false;
    const std::vector<DuiNodeId> matches = FindNodesByTitle(editor_->searchQuery, true);
    if (matches.empty())
        return false;
    editor_->searchCursor = (editor_->searchCursor + 1) % static_cast<int>(matches.size());
    return FocusNode(matches[static_cast<std::size_t>(editor_->searchCursor)]);
}

void DuiNodeEditor::FrameAll()
{
    editor_->autoFit = true;
    const core::Rect content = editor_->graph.ContentBounds();
    const core::Rect bounds = Bounds();
    if (content.Empty() || bounds.Empty())
    {
        editor_->transform.scale = 1.0;
        editor_->transform.offset = {bounds.left + 20, bounds.top + 20};
        editor_->PlaceEmbeddedControls();
        return;
    }
    // 用两轴中更受限的一轴确定缩放，留出边距
    const double margin = 40.0;
    const double scaleX = (bounds.Width() - margin * 2.0) / (std::max)(1, content.Width());
    const double scaleY = (bounds.Height() - margin * 2.0) / (std::max)(1, content.Height());
    const double scale = (std::clamp)((std::min)(scaleX, scaleY),
                                      editor_->style.minZoom, editor_->style.maxZoom);
    editor_->transform.scale = scale;
    // 让内容包围盒中心对准控件中心
    const core::Point center{content.left + content.Width() / 2, content.top + content.Height() / 2};
    editor_->transform.offset = {
        bounds.left + bounds.Width() / 2 - static_cast<int>(std::lround(center.x * scale)),
        bounds.top + bounds.Height() / 2 - static_cast<int>(std::lround(center.y * scale))};
    editor_->PlaceEmbeddedControls();
}

void DuiNodeEditor::FrameSelection()
{
    if (editor_->selectedNodes.empty())
    {
        FrameAll();
        return;
    }
    core::Rect content{};
    bool first = true;
    for (const DuiNodeId node : editor_->selectedNodes)
    {
        const DuiGraphNode* entry = editor_->graph.FindNode(node);
        if (entry == nullptr)
            continue;
        const core::Rect nodeBounds{entry->position.x, entry->position.y,
                                    entry->position.x + entry->size.width,
                                    entry->position.y + entry->size.height};
        if (first)
        {
            content = nodeBounds;
            first = false;
            continue;
        }
        content.left = (std::min)(content.left, nodeBounds.left);
        content.top = (std::min)(content.top, nodeBounds.top);
        content.right = (std::max)(content.right, nodeBounds.right);
        content.bottom = (std::max)(content.bottom, nodeBounds.bottom);
    }
    const core::Rect bounds = Bounds();
    if (content.Empty() || bounds.Empty())
        return;
    const double scaleX = (bounds.Width() - 80.0) / (std::max)(1, content.Width());
    const double scaleY = (bounds.Height() - 80.0) / (std::max)(1, content.Height());
    const double scale = (std::clamp)((std::min)(scaleX, scaleY),
                                      editor_->style.minZoom, editor_->style.maxZoom);
    editor_->transform.scale = scale;
    const core::Point center{content.left + content.Width() / 2, content.top + content.Height() / 2};
    editor_->transform.offset = {
        bounds.left + bounds.Width() / 2 - static_cast<int>(std::lround(center.x * scale)),
        bounds.top + bounds.Height() / 2 - static_cast<int>(std::lround(center.y * scale))};
    editor_->PlaceEmbeddedControls();
}

void DuiNodeEditor::ResetView()
{
    editor_->transform.scale = 1.0;
    FrameAll();
}

// ---------------------------------------------------------------- 布局

void DuiNodeEditor::Impl::LayoutGraph()
{
    // 布局在画布坐标下用未缩放的 textStyle 测量；缩放只影响绘制字号与屏幕投影。
    layouts.clear();
    for (auto& node : graph.Nodes())
    {
        NodeLayout layout;
        if (node.kind == DuiNodeKind::Group || node.kind == DuiNodeKind::Comment)
        {
            // 分组/注释的尺寸由模型持有；布局只同步 bounds/header
            if (node.size.width <= 0 || node.size.height <= 0)
                node.size = {style.minNodeWidth, style.headerHeight + style.nodePadding * 2};
            layout.bounds = {node.position.x, node.position.y,
                             node.position.x + node.size.width,
                             node.position.y + node.size.height};
            layout.header = {layout.bounds.left, layout.bounds.top, layout.bounds.right,
                             (std::min)(layout.bounds.bottom, layout.bounds.top + style.headerHeight)};
            layouts.push_back({node.id, layout});
            continue;
        }

        // 收集本节点的引脚（保持添加顺序），并记录次序供绘制与命中复用
        std::vector<DuiPinId> inputs;
        std::vector<DuiPinId> outputs;
        for (const auto& pin : graph.Pins())
        {
            if (pin.node != node.id)
                continue;
            (pin.kind == DuiPinKind::Input ? inputs : outputs).push_back(pin.id);
        }
        layout.inputOrder = inputs;
        layout.outputOrder = outputs;
        const int rows = MaxPinRows(static_cast<int>(inputs.size()), static_cast<int>(outputs.size()));

        // 宽度：标题、两侧引脚标签、引脚内联控件、内嵌内容四者取大，再钳制到样式上下限
        int widestInput{};
        int widestOutput{};
        int widestInputWidget{};
        int widestOutputWidget{};
        for (const DuiPinId id : inputs)
        {
            const DuiGraphPin* pin = graph.FindPin(id);
            if (pin == nullptr)
                continue;
            widestInput = (std::max)(widestInput, MeasureWidth(measurer, pin->name, textStyle));
            const core::Control* widget = PinContentOf(id);
            if (widget != nullptr)
            {
                const int preferred = widget->DesiredSize().width;
                const int wanted = preferred > 0 ? preferred : widget->Bounds().Width();
                widestInputWidget = (std::max)(widestInputWidget,
                    (std::min)(style.pinWidgetMaxWidth, (std::max)(0, wanted)));
            }
        }
        for (const DuiPinId id : outputs)
        {
            const DuiGraphPin* pin = graph.FindPin(id);
            if (pin == nullptr)
                continue;
            widestOutput = (std::max)(widestOutput, MeasureWidth(measurer, pin->name, textStyle));
            const core::Control* widget = PinContentOf(id);
            if (widget != nullptr)
            {
                const int preferred = widget->DesiredSize().width;
                const int wanted = preferred > 0 ? preferred : widget->Bounds().Width();
                widestOutputWidget = (std::max)(widestOutputWidget,
                    (std::min)(style.pinWidgetMaxWidth, (std::max)(0, wanted)));
            }
        }
        const int titleWidth = MeasureWidth(measurer, node.title, textStyle);
        const core::Control* content = nullptr;
        for (const auto& entry : contents)
        {
            if (entry.first == node.id)
                content = entry.second;
        }
        const core::Size contentSize = content != nullptr ? content->DesiredSize() : core::Size{};
        const int pinsWidth = widestInput + widestOutput + style.pinRadius * 6 + style.nodePadding * 2
            + widestInputWidget + widestOutputWidget
            + (widestInputWidget > 0 || widestOutputWidget > 0 ? style.nodePadding * 2 : 0);
        int width = (std::max)({style.minNodeWidth, titleWidth + style.nodePadding * 2, pinsWidth,
                                contentSize.width + style.nodePadding * 2});
        width = (std::min)(width, style.maxNodeWidth);

        int height = style.headerHeight + rows * style.pinRowHeight + style.nodePadding;
        if (content != nullptr && contentSize.height > 0)
            height += contentSize.height + style.nodePadding;
        height = (std::max)(height, style.headerHeight + style.nodePadding);

        // 尺寸是布局结果，写回模型供外部查询与命中使用
        node.size = {width, height};
        layout.bounds = {node.position.x, node.position.y,
                         node.position.x + width, node.position.y + height};
        layout.header = {layout.bounds.left, layout.bounds.top, layout.bounds.right,
                         layout.bounds.top + style.headerHeight};

        // 引脚圆心落在节点左右边界上，逐行向下排列。
        // 行内布局：输入 = [引脚][标签 ……][控件]；输出 = [控件][标签 ……][引脚]，
        // 与常见蓝图编辑器一致，使标签与控件不互相挤压。
        const auto reserveWidget = [&](DuiPinId id)
        {
            const core::Control* widget = PinContentOf(id);
            if (widget == nullptr)
                return 0;
            // 优先取控件自报的首选宽度；未重写 DesiredSize 的控件（返回 0）退回其当前宽度，
            // 否则内联控件会静默不占位——显式设定过尺寸的控件也能正常工作。
            const int preferred = widget->DesiredSize().width;
            const int wanted = preferred > 0 ? preferred : widget->Bounds().Width();
            if (wanted <= 0)
                return 0;
            return (std::min)(style.pinWidgetMaxWidth, wanted);
        };
        for (int index = 0; index < static_cast<int>(inputs.size()); ++index)
        {
            const int centerY = layout.bounds.top + style.headerHeight
                + index * style.pinRowHeight + style.pinRowHeight / 2;
            const int widgetWidth = reserveWidget(inputs[static_cast<std::size_t>(index)]);
            layout.inputPins.push_back({layout.bounds.left - style.pinRadius,
                                        centerY - style.pinRadius,
                                        layout.bounds.left + style.pinRadius,
                                        centerY + style.pinRadius});
            const int widgetLeft = widgetWidth > 0
                ? layout.bounds.right - style.nodePadding - widgetWidth
                : layout.bounds.right;
            layout.inputWidgets.push_back(widgetWidth > 0
                ? core::Rect{widgetLeft, centerY - style.pinRowHeight / 2 + 2,
                             layout.bounds.right - style.nodePadding, centerY + style.pinRowHeight / 2 - 2}
                : core::Rect{});
            layout.inputLabels.push_back({layout.bounds.left + style.pinRadius * 2 + 4,
                                          centerY - style.pinRowHeight / 2,
                                          widgetWidth > 0 ? widgetLeft - 4 : layout.bounds.right,
                                          centerY + style.pinRowHeight / 2});
        }
        for (int index = 0; index < static_cast<int>(outputs.size()); ++index)
        {
            const int centerY = layout.bounds.top + style.headerHeight
                + index * style.pinRowHeight + style.pinRowHeight / 2;
            const int widgetWidth = reserveWidget(outputs[static_cast<std::size_t>(index)]);
            layout.outputPins.push_back({layout.bounds.right - style.pinRadius,
                                         centerY - style.pinRadius,
                                         layout.bounds.right + style.pinRadius,
                                         centerY + style.pinRadius});
            layout.outputWidgets.push_back(widgetWidth > 0
                ? core::Rect{layout.bounds.left + style.nodePadding,
                             centerY - style.pinRowHeight / 2 + 2,
                             layout.bounds.left + style.nodePadding + widgetWidth,
                             centerY + style.pinRowHeight / 2 - 2}
                : core::Rect{});
            const int labelLeft = widgetWidth > 0
                ? layout.bounds.left + style.nodePadding + widgetWidth + 4
                : layout.bounds.left;
            layout.outputLabels.push_back({labelLeft,
                                           centerY - style.pinRowHeight / 2,
                                           layout.bounds.right - style.pinRadius * 2 - 4,
                                           centerY + style.pinRowHeight / 2});
        }
        const int contentTop = layout.bounds.top + style.headerHeight + rows * style.pinRowHeight;
        if (content != nullptr && contentSize.height > 0)
        {
            layout.content = {layout.bounds.left + style.nodePadding, contentTop,
                              layout.bounds.right - style.nodePadding,
                              contentTop + contentSize.height};
        }
        layouts.push_back({node.id, layout});
    }
    layoutIndex.clear();
    layoutIndex.reserve(layouts.size());
    for (std::size_t index = 0; index < layouts.size(); ++index)
        layoutIndex[layouts[index].first.value] = index;
    PlaceEmbeddedControls();
}

void DuiNodeEditor::Impl::PlaceEmbeddedControls()
{
    const bool embedded = EmbeddedVisible();
    // 同步内嵌控件：位置取内容的屏幕矩形；
    // 必须 Layout 以同步原生 TextInput，否则会停在旧坐标或隐藏后仍显示。
    for (const auto& entry : contents)
    {
        const NodeLayout* layout = LayoutOf(entry.first);
        if (layout == nullptr || entry.second == nullptr)
            continue;
        if (!embedded)
        {
            entry.second->SetVisible(false);
            SyncEmbeddedControl(*entry.second);
            continue;
        }
        entry.second->SetVisible(true);
        entry.second->SetBounds(transform.ToScreenRect(layout->content));
        SyncEmbeddedControl(*entry.second);
    }

    // 引脚内联控件：按行内为它预留的槽位摆放
    for (const auto& entry : pinContents)
    {
        if (entry.second == nullptr)
            continue;
        if (!embedded)
        {
            entry.second->SetVisible(false);
            SyncEmbeddedControl(*entry.second);
            continue;
        }
        const DuiGraphPin* pin = graph.FindPin(entry.first);
        const NodeLayout* layout = pin != nullptr ? LayoutOf(pin->node) : nullptr;
        if (pin == nullptr || layout == nullptr)
        {
            entry.second->SetVisible(false);
            SyncEmbeddedControl(*entry.second);
            continue;
        }
        const bool isInput = pin->kind == DuiPinKind::Input;
        const auto& order = isInput ? layout->inputOrder : layout->outputOrder;
        const auto& slots = isInput ? layout->inputWidgets : layout->outputWidgets;
        core::Rect slot{};
        for (std::size_t index = 0; index < order.size(); ++index)
        {
            if (order[index] == entry.first && index < slots.size())
            {
                slot = slots[index];
                break;
            }
        }
        if (slot.Empty())
        {
            entry.second->SetVisible(false);
            SyncEmbeddedControl(*entry.second);
            continue;
        }
        entry.second->SetVisible(true);
        entry.second->SetBounds(transform.ToScreenRect(slot));
        SyncEmbeddedControl(*entry.second);
    }

    for (const auto& entry : contents)
    {
        if (entry.second == nullptr || !entry.second->Visible())
            continue;
        ParkNativeInput(*entry.second, WidgetOccluded(entry.first, entry.second->Bounds()));
    }
    for (const auto& entry : pinContents)
    {
        if (entry.second == nullptr || !entry.second->Visible())
            continue;
        const DuiGraphPin* pin = graph.FindPin(entry.first);
        if (pin == nullptr)
            continue;
        ParkNativeInput(*entry.second, WidgetOccluded(pin->node, entry.second->Bounds()));
    }
    PlaceRenameInput();
}

void DuiNodeEditor::Impl::TranslateLayouts(const std::vector<DuiNodeId>& nodes, core::Point delta)
{
    if (delta.x == 0 && delta.y == 0)
        return;
    for (const DuiNodeId node : nodes)
    {
        const auto found = layoutIndex.find(node.value);
        if (found == layoutIndex.end() || found->second >= layouts.size())
            continue;
        NodeLayout& layout = layouts[found->second].second;
        OffsetRect(layout.bounds, delta);
        OffsetRect(layout.header, delta);
        OffsetRect(layout.content, delta);
        for (core::Rect& rect : layout.inputPins)
            OffsetRect(rect, delta);
        for (core::Rect& rect : layout.outputPins)
            OffsetRect(rect, delta);
        for (core::Rect& rect : layout.inputLabels)
            OffsetRect(rect, delta);
        for (core::Rect& rect : layout.outputLabels)
            OffsetRect(rect, delta);
        for (core::Rect& rect : layout.inputWidgets)
            OffsetRect(rect, delta);
        for (core::Rect& rect : layout.outputWidgets)
            OffsetRect(rect, delta);
    }
    PlaceEmbeddedControls();
}

const NodeLayout* DuiNodeEditor::Impl::LayoutOf(DuiNodeId node) const
{
    const auto found = layoutIndex.find(node.value);
    if (found == layoutIndex.end() || found->second >= layouts.size())
        return nullptr;
    return &layouts[found->second].second;
}

std::vector<DuiNodeId> DuiNodeEditor::Impl::NodePaintOrder() const
{
    std::vector<DuiNodeId> rest;
    std::vector<DuiNodeId> top;
    for (const auto& node : graph.Nodes())
    {
        if (node.kind != DuiNodeKind::Normal)
            continue;
        if (IsNodeSelected(node.id) || hovered.node == node.id)
            top.push_back(node.id);
        else
            rest.push_back(node.id);
    }
    rest.insert(rest.end(), top.begin(), top.end());
    return rest;
}

bool DuiNodeEditor::Impl::WidgetOccluded(DuiNodeId ownerNode, core::Rect screenBounds) const
{
    if (screenBounds.Empty())
        return false;
    const std::vector<DuiNodeId> order = NodePaintOrder();
    bool seenOwner{};
    for (const DuiNodeId id : order)
    {
        if (id == ownerNode)
        {
            seenOwner = true;
            continue;
        }
        if (!seenOwner)
            continue;
        const NodeLayout* layout = LayoutOf(id);
        if (layout == nullptr)
            continue;
        if (!core::Rect::Intersect(transform.ToScreenRect(layout->bounds), screenBounds).Empty())
            return true;
    }
    return false;
}

bool DuiNodeEditor::Impl::EmbeddedVisible() const
{
    // 始终显示：FrameAll / 滚轮缩放常使比例偏离 1:1，若因此隐藏控件，
    // 用户会觉得「有时突然没了、Reset 1:1 又出来」，无法预期。
    (void)style.embeddedZoomTolerance;
    return true;
}

void DuiNodeEditor::Impl::SyncContents()
{
    if (owner == nullptr)
        return;
    if (!contentFactory)
    {
        // 取消内容工厂：移除已托管的节点内容控件
        for (const auto& entry : contents)
            (void)owner->RemoveChild(entry.second);
        contents.clear();
    }
    else
    {
        // 移除已删节点的内容
        for (std::size_t index = contents.size(); index > 0; --index)
        {
            const auto& entry = contents[index - 1];
            if (graph.FindNode(entry.first) == nullptr)
            {
                (void)owner->RemoveChild(entry.second);
                contents.erase(contents.begin() + static_cast<std::ptrdiff_t>(index - 1));
            }
        }
        // 为新增的普通节点创建内容
        for (const auto& node : graph.Nodes())
        {
            if (node.kind != DuiNodeKind::Normal)
                continue;
            bool exists = false;
            for (const auto& entry : contents)
                exists = exists || entry.first == node.id;
            if (exists)
                continue;
            auto control = contentFactory(node.id, node.title);
            if (!control)
                continue;
            core::Control* raw = control.get();
            owner->AddChild(std::move(control));
            contents.push_back({node.id, raw});
        }
    }

    // 引脚内联控件：与节点内容同一套增删逻辑
    if (!pinContentFactory)
    {
        for (const auto& entry : pinContents)
            (void)owner->RemoveChild(entry.second);
        pinContents.clear();
        return;
    }
    for (std::size_t index = pinContents.size(); index > 0; --index)
    {
        const auto& entry = pinContents[index - 1];
        if (graph.FindPin(entry.first) == nullptr)
        {
            (void)owner->RemoveChild(entry.second);
            pinContents.erase(pinContents.begin() + static_cast<std::ptrdiff_t>(index - 1));
        }
    }
    for (const auto& pin : graph.Pins())
    {
        bool exists = false;
        for (const auto& entry : pinContents)
            exists = exists || entry.first == pin.id;
        if (exists)
            continue;
        auto control = pinContentFactory(pin.id, pin.name);
        if (!control)
            continue;
        core::Control* raw = control.get();
        owner->AddChild(std::move(control));
        pinContents.push_back({pin.id, raw});
    }
}

core::Size DuiNodeEditor::DesiredSize() const { return {640, 420}; }

core::DuiAccessibilityData DuiNodeEditor::CreateAccessibilityData() const
{
    // Host 在 PointerDown / Tab 时按 keyboardFocusable 决定焦点目标；
    // 未声明时按键（Delete、Ctrl+Z 等）会落到上一个焦点控件上。
    return {core::DuiAccessibilityRole::Pane, Name().empty() ? "NodeEditor" : Name(),
            {}, {}, true, {}};
}

// ---------------------------------------------------------------- 命中测试

HitResult DuiNodeEditor::Impl::HitTest(core::Point screen) const
{
    HitResult result;
    if (minimapVisible && !graph.Empty() && MinimapRect().Contains(screen))
    {
        result.onMinimap = true;
        return result;
    }
    const std::vector<DuiNodeId> order = NodePaintOrder();
    // 顶层节点优先：内嵌控件 > 引脚（含屏幕命中放大）> 节点本体
    for (auto it = order.rbegin(); it != order.rend(); ++it)
    {
        const NodeLayout* layout = LayoutOf(*it);
        if (layout == nullptr)
            continue;
        const auto hitsWidget = [&](const core::Rect& canvas) -> bool
        {
            return !canvas.Empty() && transform.ToScreenRect(canvas).Contains(screen);
        };
        bool onWidget = hitsWidget(layout->content);
        if (!onWidget)
        {
            for (const core::Rect& slot : layout->inputWidgets)
            {
                if (hitsWidget(slot))
                {
                    onWidget = true;
                    break;
                }
            }
        }
        if (!onWidget)
        {
            for (const core::Rect& slot : layout->outputWidgets)
            {
                if (hitsWidget(slot))
                {
                    onWidget = true;
                    break;
                }
            }
        }
        if (onWidget)
        {
            result.node = *it;
            return result;
        }
        for (std::size_t index = 0; index < layout->inputOrder.size(); ++index)
        {
            if (PinHitScreenRect(transform, layout->inputPins[index], true).Contains(screen))
            {
                result.pin = layout->inputOrder[index];
                result.node = *it;
                return result;
            }
        }
        for (std::size_t index = 0; index < layout->outputOrder.size(); ++index)
        {
            if (PinHitScreenRect(transform, layout->outputPins[index], false).Contains(screen))
            {
                result.pin = layout->outputOrder[index];
                result.node = *it;
                return result;
            }
        }
        if (transform.ToScreenRect(layout->bounds).Contains(screen))
        {
            result.node = *it;
            return result;
        }
    }
    // 分组在节点之下
    for (auto entry = layouts.rbegin(); entry != layouts.rend(); ++entry)
    {
        const NodeLayout& layout = entry->second;
        if (!transform.ToScreenRect(layout.bounds).Contains(screen))
            continue;
        const DuiGraphNode* node = graph.FindNode(entry->first);
        if (node == nullptr
            || (node->kind != DuiNodeKind::Group && node->kind != DuiNodeKind::Comment))
            continue;
        result.group = node->id;
        result.node = node->kind == DuiNodeKind::Comment ? node->id : DuiNodeId{};
        const core::Rect corner = transform.ToScreenRect(
            {layout.bounds.right - 12, layout.bounds.bottom - 12,
             layout.bounds.right, layout.bounds.bottom});
        result.onGroupResize = corner.Contains(screen);
        return result;
    }
    // 连线：先按折线包围盒剔除，再算距离
    for (const auto& link : graph.Links())
    {
        const core::Point start = transform.ToScreen(PinCenter(link.start));
        const core::Point end = transform.ToScreen(PinCenter(link.end));
        const std::vector<core::Point> points = BuildLinkPolyline(start, end, style);
        if (points.empty())
            continue;
        int left = points.front().x;
        int top = points.front().y;
        int right = left;
        int bottom = top;
        for (const core::Point& point : points)
        {
            left = (std::min)(left, point.x);
            top = (std::min)(top, point.y);
            right = (std::max)(right, point.x);
            bottom = (std::max)(bottom, point.y);
        }
        const int pad = (std::max)(3, static_cast<int>(
            std::lround(style.linkThickness * transform.scale)));
        const core::Rect envelope{left - pad, top - pad, right + pad, bottom + pad};
        if (!envelope.Contains(screen))
            continue;
        if (DistanceToPolyline(points, screen)
            <= (std::max)(3.0, style.linkThickness * transform.scale))
        {
            result.link = link.id;
            return result;
        }
    }
    return result;
}

DuiPinId DuiNodeEditor::Impl::FindLinkMagnetPin(core::Point screen, DuiPinId from) const
{
    if (!from.Valid())
        return {};
    DuiPinId best{};
    int bestDist2 = kLinkMagnetPx * kLinkMagnetPx;
    const auto consider = [&](DuiPinId pin, const core::Rect& canvasPin)
    {
        if (!pin.Valid() || pin == from)
            return;
        if (!CanCreateLink(from, pin) && !CanCreateLink(pin, from))
            return;
        const core::Rect screenPin = transform.ToScreenRect(canvasPin);
        const int dx = (screenPin.left + screenPin.right) / 2 - screen.x;
        const int dy = (screenPin.top + screenPin.bottom) / 2 - screen.y;
        const int dist2 = dx * dx + dy * dy;
        if (dist2 >= bestDist2)
            return;
        bestDist2 = dist2;
        best = pin;
    };
    for (const auto& entry : layouts)
    {
        const NodeLayout& layout = entry.second;
        for (std::size_t index = 0; index < layout.inputOrder.size(); ++index)
            consider(layout.inputOrder[index], layout.inputPins[index]);
        for (std::size_t index = 0; index < layout.outputOrder.size(); ++index)
            consider(layout.outputOrder[index], layout.outputPins[index]);
    }
    return best;
}

DuiPinId DuiNodeEditor::Impl::ResolveLinkTarget(core::Point screen, DuiPinId from) const
{
    if (hovered.pin.Valid() && hovered.pin != from)
        return hovered.pin;
    return FindLinkMagnetPin(screen, from);
}

core::Point DuiNodeEditor::Impl::PinCenter(DuiPinId pin) const
{
    const DuiGraphPin* entry = graph.FindPin(pin);
    if (entry == nullptr)
        return {};
    const NodeLayout* layout = LayoutOf(entry->node);
    if (layout == nullptr)
        return {};
    const auto& order = entry->kind == DuiPinKind::Input ? layout->inputOrder : layout->outputOrder;
    const auto& rects = entry->kind == DuiPinKind::Input ? layout->inputPins : layout->outputPins;
    for (std::size_t index = 0; index < order.size(); ++index)
    {
        if (order[index] != pin || index >= rects.size())
            continue;
        const core::Rect rect = rects[index];
        return {(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
    }
    return {};
}

// ---------------------------------------------------------------- 选择

bool DuiNodeEditor::Impl::IsNodeSelected(DuiNodeId node) const
{
    return std::find(selectedNodes.begin(), selectedNodes.end(), node) != selectedNodes.end();
}
bool DuiNodeEditor::Impl::IsLinkSelected(DuiLinkId link) const
{
    return std::find(selectedLinks.begin(), selectedLinks.end(), link) != selectedLinks.end();
}

void DuiNodeEditor::Impl::ClearSelection()
{
    if (selectedNodes.empty() && selectedLinks.empty())
        return;
    selectedNodes.clear();
    selectedLinks.clear();
    selectionDirty = true;
    PlaceEmbeddedControls();
}

void DuiNodeEditor::Impl::SelectNode(DuiNodeId node, bool append)
{
    if (!node.Valid())
        return;
    if (!append)
    {
        selectedNodes.clear();
        selectedLinks.clear();
    }
    if (!IsNodeSelected(node))
        selectedNodes.push_back(node);
    selectionDirty = true;
    PlaceEmbeddedControls();
}

void DuiNodeEditor::Impl::SelectLink(DuiLinkId link, bool append)
{
    if (!link.Valid())
        return;
    if (!append)
    {
        selectedNodes.clear();
        selectedLinks.clear();
    }
    if (!IsLinkSelected(link))
        selectedLinks.push_back(link);
    selectionDirty = true;
}

void DuiNodeEditor::ClearSelection()
{
    editor_->ClearSelection();
    if (editor_->selectionChanged)
        editor_->selectionChanged();
}
std::vector<DuiNodeId> DuiNodeEditor::SelectedNodes() const { return editor_->selectedNodes; }
std::vector<DuiLinkId> DuiNodeEditor::SelectedLinks() const { return editor_->selectedLinks; }
bool DuiNodeEditor::IsNodeSelected(DuiNodeId node) const { return editor_->IsNodeSelected(node); }
bool DuiNodeEditor::IsLinkSelected(DuiLinkId link) const { return editor_->IsLinkSelected(link); }
void DuiNodeEditor::SelectNode(DuiNodeId node, bool append)
{
    editor_->SelectNode(node, append);
    if (editor_->selectionChanged)
        editor_->selectionChanged();
}
void DuiNodeEditor::SelectLink(DuiLinkId link, bool append)
{
    editor_->SelectLink(link, append);
    if (editor_->selectionChanged)
        editor_->selectionChanged();
}

// ---------------------------------------------------------------- 历史

void DuiNodeEditor::Impl::PushHistory(HistoryRecord record)
{
    undoStack.push_back(std::move(record));
    if (undoLimit > 0 && static_cast<int>(undoStack.size()) > undoLimit)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void DuiNodeEditor::Impl::CommitStructure(StructureDelta delta)
{
    if (StructureDeltaEmpty(delta))
        return;
    HistoryRecord record;
    record.kind = HistoryKind::Structure;
    record.structure = std::move(delta);
    PushHistory(std::move(record));
}

bool DuiNodeEditor::Impl::CommitNewLink(DuiPinId start, DuiPinId end)
{
    StructureDelta delta;
    CaptureExclusiveLinks(graph, start, end, delta.removedLinks);
    const DuiLinkId created = graph.AddLink(start, end);
    if (!created.Valid())
        return false;
    if (const DuiGraphLink* added = graph.FindLink(created))
        delta.addedLinks.push_back(*added);
    CommitStructure(std::move(delta));
    NotifyGraphChanged();
    return true;
}

bool DuiNodeEditor::Impl::CommitBreakPinLinks(DuiPinId pin)
{
    StructureDelta delta;
    for (const DuiLinkId id : graph.LinksOfPin(pin))
    {
        const DuiGraphLink* link = graph.FindLink(id);
        if (link != nullptr)
            delta.removedLinks.push_back(*link);
    }
    if (delta.removedLinks.empty())
        return false;
    for (const DuiGraphLink& link : delta.removedLinks)
        (void)graph.RemoveLink(link.id);
    CommitStructure(std::move(delta));
    NotifyGraphChanged();
    return true;
}

void DuiNodeEditor::Impl::ApplyStructure(const StructureDelta& delta)
{
    for (const DuiGraphLink& link : delta.addedLinks)
        (void)graph.RemoveLink(link.id);
    for (const DuiGraphPin& pin : delta.addedPins)
        (void)graph.RemovePin(pin.id);
    for (const DuiGraphNode& node : delta.addedNodes)
        (void)graph.RemoveNode(node.id);
    for (const DuiGraphNode& node : delta.removedNodes)
        (void)graph.RestoreNode(node);
    for (const DuiGraphPin& pin : delta.removedPins)
        (void)graph.RestorePin(pin);
    for (const DuiGraphLink& link : delta.removedLinks)
        (void)graph.RestoreLink(link);
    for (const auto& entry : delta.positionsBefore)
    {
        DuiGraphNode* node = graph.FindNode(entry.first);
        if (node != nullptr)
            node->position = entry.second;
    }
    for (const auto& entry : delta.sizesBefore)
    {
        DuiGraphNode* node = graph.FindNode(entry.first);
        if (node != nullptr)
            node->size = entry.second;
    }
    NotifyGraphChanged();
}

std::vector<std::pair<DuiNodeId, core::Point>> DuiNodeEditor::Impl::CapturePositions(
    const std::vector<DuiNodeId>& nodes) const
{
    std::vector<std::pair<DuiNodeId, core::Point>> captured;
    const auto take = [this, &captured](DuiNodeId id)
    {
        const DuiGraphNode* node = graph.FindNode(id);
        if (node == nullptr)
            return;
        for (const auto& existing : captured)
        {
            if (existing.first == id)
                return;
        }
        captured.emplace_back(id, node->position);
    };
    for (const DuiNodeId id : nodes)
    {
        take(id);
        const DuiGraphNode* node = graph.FindNode(id);
        if (node == nullptr || node->kind != DuiNodeKind::Group)
            continue;
        for (const auto& candidate : graph.Nodes())
        {
            if (graph.NodeBelongsTo(candidate.id, id))
                take(candidate.id);
        }
    }
    return captured;
}

void DuiNodeEditor::Impl::ApplyPositions(const std::vector<std::pair<DuiNodeId, core::Point>>& positions)
{
    for (const auto& entry : positions)
    {
        DuiGraphNode* node = graph.FindNode(entry.first);
        if (node != nullptr)
            node->position = entry.second;
    }
    NotifyGraphChanged();
}

void DuiNodeEditor::Impl::CommitPositions(std::vector<std::pair<DuiNodeId, core::Point>> before)
{
    HistoryRecord record;
    record.kind = HistoryKind::Positions;
    record.positionsBefore = std::move(before);
    std::vector<DuiNodeId> ids;
    ids.reserve(record.positionsBefore.size());
    for (const auto& entry : record.positionsBefore)
        ids.push_back(entry.first);
    record.positionsAfter = CapturePositions(ids);
    PushHistory(std::move(record));
}

void DuiNodeEditor::Impl::CommitTitle(DuiNodeId node, std::string before, std::string after)
{
    if (before == after)
        return;
    HistoryRecord record;
    record.kind = HistoryKind::Title;
    record.titleNode = node;
    record.titleBefore = std::move(before);
    record.titleAfter = std::move(after);
    PushHistory(std::move(record));
}

bool DuiNodeEditor::Undo()
{
    if (editor_->readOnly || editor_->undoStack.empty())
        return false;
    Impl::HistoryRecord record = std::move(editor_->undoStack.back());
    editor_->undoStack.pop_back();
    Impl::HistoryRecord inverse;
    inverse.kind = record.kind;
    switch (record.kind)
    {
    case Impl::HistoryKind::Structure:
        inverse.structure = SwapStructure(record.structure);
        editor_->ApplyStructure(record.structure);
        break;
    case Impl::HistoryKind::Positions:
        inverse.positionsBefore = record.positionsAfter;
        inverse.positionsAfter = record.positionsBefore;
        editor_->ApplyPositions(record.positionsBefore);
        break;
    case Impl::HistoryKind::Title:
        inverse.titleNode = record.titleNode;
        inverse.titleBefore = record.titleAfter;
        inverse.titleAfter = record.titleBefore;
        if (DuiGraphNode* node = editor_->graph.FindNode(record.titleNode))
        {
            node->title = record.titleBefore;
            editor_->NotifyGraphChanged();
        }
        break;
    }
    editor_->redoStack.push_back(std::move(inverse));
    editor_->selectedNodes.erase(
        std::remove_if(editor_->selectedNodes.begin(), editor_->selectedNodes.end(),
                       [this](DuiNodeId node) { return editor_->graph.FindNode(node) == nullptr; }),
        editor_->selectedNodes.end());
    editor_->selectedLinks.erase(
        std::remove_if(editor_->selectedLinks.begin(), editor_->selectedLinks.end(),
                       [this](DuiLinkId link) { return editor_->graph.FindLink(link) == nullptr; }),
        editor_->selectedLinks.end());
    if (editor_->selectionChanged)
        editor_->selectionChanged();
    return true;
}

bool DuiNodeEditor::Redo()
{
    if (editor_->readOnly || editor_->redoStack.empty())
        return false;
    Impl::HistoryRecord record = std::move(editor_->redoStack.back());
    editor_->redoStack.pop_back();
    Impl::HistoryRecord inverse;
    inverse.kind = record.kind;
    switch (record.kind)
    {
    case Impl::HistoryKind::Structure:
        inverse.structure = SwapStructure(record.structure);
        editor_->ApplyStructure(record.structure);
        break;
    case Impl::HistoryKind::Positions:
        inverse.positionsBefore = record.positionsAfter;
        inverse.positionsAfter = record.positionsBefore;
        editor_->ApplyPositions(record.positionsBefore);
        break;
    case Impl::HistoryKind::Title:
        inverse.titleNode = record.titleNode;
        inverse.titleBefore = record.titleAfter;
        inverse.titleAfter = record.titleBefore;
        if (DuiGraphNode* node = editor_->graph.FindNode(record.titleNode))
        {
            node->title = record.titleBefore;
            editor_->NotifyGraphChanged();
        }
        break;
    }
    editor_->undoStack.push_back(std::move(inverse));
    if (editor_->selectionChanged)
        editor_->selectionChanged();
    return true;
}

bool DuiNodeEditor::CanUndo() const { return !editor_->undoStack.empty(); }
bool DuiNodeEditor::CanRedo() const { return !editor_->redoStack.empty(); }
void DuiNodeEditor::SetUndoLimit(int limit) { editor_->undoLimit = (std::max)(1, limit); }
void DuiNodeEditor::ClearHistory()
{
    editor_->undoStack.clear();
    editor_->redoStack.clear();
}

// ---------------------------------------------------------------- 编辑

bool DuiNodeEditor::Impl::DeleteSelection()
{
    if (readOnly || (selectedNodes.empty() && selectedLinks.empty()))
        return false;
    StructureDelta delta;
    for (const DuiLinkId link : selectedLinks)
    {
        const DuiGraphLink* entry = graph.FindLink(link);
        if (entry != nullptr)
            PushUniqueById(delta.removedLinks, *entry, &DuiGraphLink::id);
    }
    for (const DuiNodeId node : selectedNodes)
        CaptureNodeRemoval(graph, node, delta);
    if (StructureDeltaEmpty(delta))
        return false;
    for (const DuiLinkId link : selectedLinks)
        (void)graph.RemoveLink(link);
    for (const DuiNodeId node : selectedNodes)
        (void)graph.RemoveNode(node);
    CommitStructure(std::move(delta));
    selectedLinks.clear();
    selectedNodes.clear();
    selectionDirty = true;
    NotifyGraphChanged();
    return true;
}

bool DuiNodeEditor::DeleteSelection() { return editor_->DeleteSelection(); }

bool DuiNodeEditor::Impl::CutSelection()
{
    if (readOnly)
        return false;
    if (!CopySelection())
        return false;
    return DeleteSelection();
}

bool DuiNodeEditor::CutSelection() { return editor_->CutSelection(); }

bool DuiNodeEditor::Impl::CopySelection()
{
    if (selectedNodes.empty())
        return false;
    clipboard.Clear();
    hasClipboard = true;
    // 旧引脚 id -> 剪贴板内新引脚 id，用于重建两端都在选择集内的连线
    std::vector<std::pair<DuiPinId, DuiPinId>> pinMap;
    const auto selectedContains = [this](DuiNodeId node)
    {
        return std::find(selectedNodes.begin(), selectedNodes.end(), node) != selectedNodes.end();
    };
    // 拷贝普通节点（含引脚/内部连线）与注释框；分组不拷贝
    for (const DuiNodeId node : selectedNodes)
    {
        const DuiGraphNode* entry = graph.FindNode(node);
        if (entry == nullptr)
            continue;
        if (entry->kind == DuiNodeKind::Comment)
        {
            const DuiNodeId copy = clipboard.AddComment(entry->title,
                {entry->position.x, entry->position.y,
                 entry->position.x + entry->size.width, entry->position.y + entry->size.height});
            if (DuiGraphNode* target = clipboard.FindNode(copy))
            {
                target->color = entry->color;
                target->hasColor = entry->hasColor;
            }
            continue;
        }
        if (entry->kind != DuiNodeKind::Normal)
            continue;
        const DuiNodeId copy = clipboard.AddNode(entry->title, entry->position);
        DuiGraphNode* target = clipboard.FindNode(copy);
        if (target != nullptr)
        {
            target->color = entry->color;
            target->hasColor = entry->hasColor;
        }
        for (const DuiPinId pinId : graph.PinsOf(node))
        {
            const DuiGraphPin* pin = graph.FindPin(pinId);
            if (pin == nullptr)
                continue;
            const DuiPinId added = clipboard.AddPin(copy, pin->name, pin->kind, pin->multipleConnections,
                                                    pin->typeId);
            pinMap.emplace_back(pinId, added);
            DuiGraphPin* targetPin = clipboard.FindPin(added);
            if (targetPin != nullptr)
            {
                targetPin->color = pin->color;
                targetPin->hasColor = pin->hasColor;
                targetPin->typeId = pin->typeId;
            }
        }
    }
    const auto mapPin = [&pinMap](DuiPinId id)
    {
        for (const auto& entry : pinMap)
        {
            if (entry.first == id)
                return entry.second;
        }
        return DuiPinId{};
    };
    for (const auto& link : graph.Links())
    {
        const DuiGraphPin* start = graph.FindPin(link.start);
        const DuiGraphPin* end = graph.FindPin(link.end);
        if (start == nullptr || end == nullptr)
            continue;
        if (!selectedContains(start->node) || !selectedContains(end->node))
            continue;
        const DuiPinId mappedStart = mapPin(link.start);
        const DuiPinId mappedEnd = mapPin(link.end);
        if (!mappedStart.Valid() || !mappedEnd.Valid())
            continue;
        (void)clipboard.AddLink(mappedStart, mappedEnd);
    }
    if (systemClipboard != nullptr)
        (void)systemClipboard->SetText(SerializeGraphXml(clipboard));
    return true;
}

bool DuiNodeEditor::Impl::CloneSelectedNormals(core::Point offset, StructureDelta& outDelta)
{
    std::vector<DuiNodeId> sources;
    sources.reserve(selectedNodes.size());
    for (const DuiNodeId node : selectedNodes)
    {
        const DuiGraphNode* entry = graph.FindNode(node);
        if (entry != nullptr && entry->kind == DuiNodeKind::Normal)
            sources.push_back(node);
    }
    if (sources.empty())
        return false;

    const auto sourceContains = [&sources](DuiNodeId node)
    {
        return std::find(sources.begin(), sources.end(), node) != sources.end();
    };
    std::vector<std::pair<DuiNodeId, DuiNodeId>> nodeMap;
    std::vector<std::pair<DuiPinId, DuiPinId>> pinMap;
    StructureDelta delta;
    selectedNodes.clear();
    selectedLinks.clear();
    for (const DuiNodeId node : sources)
    {
        const DuiGraphNode* entry = graph.FindNode(node);
        if (entry == nullptr)
            continue;
        const DuiNodeId copy = graph.AddNode(entry->title,
            {entry->position.x + offset.x, entry->position.y + offset.y});
        if (DuiGraphNode* target = graph.FindNode(copy))
        {
            target->color = entry->color;
            target->hasColor = entry->hasColor;
        }
        nodeMap.emplace_back(node, copy);
        SelectNode(copy, true);
        for (const DuiPinId pinId : graph.PinsOf(node))
        {
            const DuiGraphPin* pin = graph.FindPin(pinId);
            if (pin == nullptr)
                continue;
            const DuiPinId added = graph.AddPin(copy, pin->name, pin->kind, pin->multipleConnections,
                                                pin->typeId);
            if (DuiGraphPin* targetPin = graph.FindPin(added))
            {
                targetPin->color = pin->color;
                targetPin->hasColor = pin->hasColor;
            }
            pinMap.emplace_back(pinId, added);
        }
    }
    const auto mapPin = [&pinMap](DuiPinId id)
    {
        for (const auto& mapped : pinMap)
        {
            if (mapped.first == id)
                return mapped.second;
        }
        return DuiPinId{};
    };
    std::vector<std::pair<DuiPinId, DuiPinId>> toLink;
    for (const auto& link : graph.Links())
    {
        const DuiGraphPin* start = graph.FindPin(link.start);
        const DuiGraphPin* end = graph.FindPin(link.end);
        if (start == nullptr || end == nullptr)
            continue;
        if (!sourceContains(start->node) || !sourceContains(end->node))
            continue;
        const DuiPinId mappedStart = mapPin(link.start);
        const DuiPinId mappedEnd = mapPin(link.end);
        if (mappedStart.Valid() && mappedEnd.Valid())
            toLink.emplace_back(mappedStart, mappedEnd);
    }
    for (const auto& pair : toLink)
        (void)graph.AddLink(pair.first, pair.second);
    for (const auto& mapped : nodeMap)
    {
        if (const DuiGraphNode* added = graph.FindNode(mapped.second))
            delta.addedNodes.push_back(*added);
    }
    for (const auto& mapped : pinMap)
    {
        if (const DuiGraphPin* added = graph.FindPin(mapped.second))
            delta.addedPins.push_back(*added);
    }
    for (const auto& link : graph.Links())
    {
        for (const auto& mapped : pinMap)
        {
            if (mapped.second == link.start || mapped.second == link.end)
            {
                PushUniqueById(delta.addedLinks, link, &DuiGraphLink::id);
                break;
            }
        }
    }
    outDelta = std::move(delta);
    NotifyGraphChanged();
    return true;
}

bool DuiNodeEditor::Impl::DuplicateSelectionForDrag()
{
    StructureDelta delta;
    if (!CloneSelectedNormals({}, delta))
        return false;
    dragDuplicateDelta = std::move(delta);
    return true;
}

bool DuiNodeEditor::Impl::DuplicateSelection()
{
    if (readOnly)
        return false;
    StructureDelta delta;
    const int step = (std::max)(1, style.gridSize);
    if (!CloneSelectedNormals({step, step}, delta))
        return false;
    CommitStructure(std::move(delta));
    return true;
}

bool DuiNodeEditor::DuplicateSelection() { return editor_->DuplicateSelection(); }

bool DuiNodeEditor::Impl::PasteClipboard(core::Point canvasPosition)
{
    DuiNodeGraph source = clipboard;
    bool ready = hasClipboard && !clipboard.Empty();
    if (systemClipboard != nullptr)
    {
        const std::optional<std::string> text = systemClipboard->GetText();
        DuiNodeGraph parsed;
        if (text && DeserializeGraphXml(*text, parsed) && !parsed.Empty())
        {
            source = std::move(parsed);
            ready = true;
        }
    }
    if (readOnly || !ready)
        return false;
    const core::Rect bounds = source.ContentBounds();
    const bool explicitPosition = canvasPosition.x != 0 || canvasPosition.y != 0;
    const int offsetX = explicitPosition ? canvasPosition.x - bounds.left : 24 * (++pasteGeneration);
    const int offsetY = explicitPosition ? canvasPosition.y - bounds.top : 24 * pasteGeneration;
    if (explicitPosition)
        ++pasteGeneration;

    std::vector<std::pair<DuiNodeId, DuiNodeId>> nodeMap;
    std::vector<std::pair<DuiPinId, DuiPinId>> pinMap;
    selectedNodes.clear();
    selectedLinks.clear();
    for (const auto& node : source.Nodes())
    {
        if (node.kind == DuiNodeKind::Group)
            continue;
        DuiNodeId added{};
        if (node.kind == DuiNodeKind::Comment)
        {
            added = graph.AddComment(node.title,
                {node.position.x + offsetX, node.position.y + offsetY,
                 node.position.x + offsetX + node.size.width,
                 node.position.y + offsetY + node.size.height});
        }
        else
        {
            added = graph.AddNode(node.title,
                {node.position.x + offsetX, node.position.y + offsetY});
        }
        if (DuiGraphNode* target = graph.FindNode(added))
        {
            target->color = node.color;
            target->hasColor = node.hasColor;
        }
        nodeMap.emplace_back(node.id, added);
        SelectNode(added, true);
        if (node.kind != DuiNodeKind::Normal)
            continue;
        for (const auto& pin : source.Pins())
        {
            if (pin.node != node.id)
                continue;
            pinMap.emplace_back(pin.id, graph.AddPin(added, pin.name, pin.kind, pin.multipleConnections,
                                                    pin.typeId));
        }
    }
    const auto mapPin = [&pinMap](DuiPinId id)
    {
        for (const auto& entry : pinMap)
        {
            if (entry.first == id)
                return entry.second;
        }
        return DuiPinId{};
    };
    for (const auto& link : source.Links())
    {
        const DuiPinId start = mapPin(link.start);
        const DuiPinId end = mapPin(link.end);
        if (start.Valid() && end.Valid())
            (void)graph.AddLink(start, end);
    }
    StructureDelta delta;
    for (const auto& mapped : nodeMap)
    {
        if (const DuiGraphNode* added = graph.FindNode(mapped.second))
            delta.addedNodes.push_back(*added);
    }
    for (const auto& mapped : pinMap)
    {
        if (const DuiGraphPin* added = graph.FindPin(mapped.second))
            delta.addedPins.push_back(*added);
    }
    for (const auto& link : graph.Links())
    {
        for (const auto& mapped : pinMap)
        {
            if (mapped.second == link.start || mapped.second == link.end)
            {
                PushUniqueById(delta.addedLinks, link, &DuiGraphLink::id);
                break;
            }
        }
    }
    CommitStructure(std::move(delta));
    selectionDirty = true;
    NotifyGraphChanged();
    return true;
}

bool DuiNodeEditor::CopySelection() { return editor_->CopySelection(); }
bool DuiNodeEditor::PasteClipboard(core::Point canvasPosition)
{
    return editor_->PasteClipboard(canvasPosition);
}
bool DuiNodeEditor::HasClipboardContent() const
{
    if (editor_->hasClipboard && !editor_->clipboard.Empty())
        return true;
    if (editor_->systemClipboard == nullptr)
        return false;
    const std::optional<std::string> text = editor_->systemClipboard->GetText();
    return text && text->find("<nodeGraph") != std::string::npos;
}

void DuiNodeEditor::SetClipboard(ui::DuiClipboard* clipboard)
{
    editor_->systemClipboard = clipboard;
}

void DuiNodeEditor::SetTextInput(ui::DuiTextInput* input)
{
    if (editor_->renameInput == input)
        return;
    const bool renaming = editor_->renamingNode.Valid();
    if (renaming)
        editor_->UnbindRenameInput();
    editor_->renameInput = input;
    if (renaming)
        editor_->BindRenameInput();
    else if (input != nullptr)
        input->SetVisible(false);
}

ui::DuiTextInput* DuiNodeEditor::TextInput() const { return editor_->renameInput; }

void DuiNodeEditor::SetPopupContext(ui::IUiHostFactory& factory, ui::HostRef owner)
{
    editor_->popupFactory = &factory;
    editor_->popupOwner = std::move(owner);
}

bool DuiNodeEditor::Impl::ShowContextMenu(core::Point point)
{
    if (popupFactory == nullptr || !contextMenu)
        return false;
    const bool hasNodes = !selectedNodes.empty();
    const bool hasLinks = !selectedLinks.empty();
    if (!hasNodes && !hasLinks && !hasClipboard)
        return false;
    list::DuiMenu& menu = *contextMenu;
    menu.ClearItems();
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::Cut), "Cut");
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::Copy), "Copy");
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::Paste), "Paste");
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::Delete), "Delete");
    menu.AddItem(static_cast<std::uint32_t>(ContextCommand::Disconnect), "Disconnect");
    menu.SetItemEnabled(static_cast<std::uint32_t>(ContextCommand::Cut), !readOnly && hasNodes);
    menu.SetItemEnabled(static_cast<std::uint32_t>(ContextCommand::Copy), hasNodes);
    menu.SetItemEnabled(static_cast<std::uint32_t>(ContextCommand::Paste), !readOnly && hasClipboard);
    menu.SetItemEnabled(static_cast<std::uint32_t>(ContextCommand::Delete),
                        !readOnly && (hasNodes || hasLinks));
    menu.SetItemEnabled(static_cast<std::uint32_t>(ContextCommand::Disconnect),
                        !readOnly && (hasNodes || hasLinks));
    return menu.Show(*popupFactory, popupOwner, {point.x, point.y, point.x + 1, point.y + 1});
}

bool DuiNodeEditor::Impl::DisconnectSelection()
{
    if (readOnly)
        return false;
    StructureDelta delta;
    for (const DuiLinkId link : selectedLinks)
    {
        const DuiGraphLink* entry = graph.FindLink(link);
        if (entry != nullptr)
            PushUniqueById(delta.removedLinks, *entry, &DuiGraphLink::id);
    }
    for (const DuiNodeId node : selectedNodes)
    {
        for (const DuiLinkId link : graph.LinksOfNode(node))
        {
            const DuiGraphLink* entry = graph.FindLink(link);
            if (entry != nullptr)
                PushUniqueById(delta.removedLinks, *entry, &DuiGraphLink::id);
        }
    }
    if (delta.removedLinks.empty())
        return false;
    for (const DuiGraphLink& link : delta.removedLinks)
        (void)graph.RemoveLink(link.id);
    CommitStructure(std::move(delta));
    selectedLinks.clear();
    selectionDirty = true;
    NotifyGraphChanged();
    return true;
}

bool DuiNodeEditor::DisconnectSelection() { return editor_->DisconnectSelection(); }

bool DuiNodeEditor::Impl::CanCreateLink(DuiPinId start, DuiPinId end) const
{
    if (!graph.CanLink(start, end))
        return false;
    return !canLinkFilter || canLinkFilter(start, end);
}

void DuiNodeEditor::Impl::RefreshCompatiblePins()
{
    compatiblePins.clear();
    if (!linkFrom.Valid())
        return;
    for (const auto& pin : graph.Pins())
    {
        if (pin.id == linkFrom)
            continue;
        if (CanCreateLink(linkFrom, pin.id) || CanCreateLink(pin.id, linkFrom))
            compatiblePins.insert(pin.id.value);
    }
}

bool DuiNodeEditor::Impl::IsCompatibleLinkPin(DuiPinId pin) const
{
    return pin.Valid() && compatiblePins.find(pin.value) != compatiblePins.end();
}

// ---------------------------------------------------------------- 配置

void DuiNodeEditor::SetNodeContentFactory(
    std::function<std::unique_ptr<core::Control>(DuiNodeId, std::string_view)> factory)
{
    editor_->contentFactory = std::move(factory);
    editor_->NotifyGraphChanged();
}

void DuiNodeEditor::SetPinContentFactory(
    std::function<std::unique_ptr<core::Control>(DuiPinId, std::string_view)> factory)
{
    editor_->pinContentFactory = std::move(factory);
    editor_->NotifyGraphChanged();
}

core::Control* DuiNodeEditor::PinContent(DuiPinId pin) const
{
    return editor_->PinContentOf(pin);
}

core::Control* DuiNodeEditor::Impl::PinContentOf(DuiPinId pin) const
{
    for (const auto& entry : pinContents)
    {
        if (entry.first == pin)
            return entry.second;
    }
    return nullptr;
}

void DuiNodeEditor::SetAnimationClock(core::AnimationClock* clock)
{
    if (editor_->clock == clock)
        return;
    editor_->clock = clock;
    // 清掉旧时钟上的任务，避免残留回调落到本控件
    if (editor_->flowTask != 0 && editor_->clock != clock && clock == nullptr)
        editor_->OnFlowTick(0.0);
    editor_->UpdateFlowAnimation();
}

void DuiNodeEditor::SetLinkFlow(DuiLinkId link, bool enabled)
{
    if (!link.Valid() || editor_->graph.FindLink(link) == nullptr)
        return;
    auto& list = editor_->flowingLinks;
    const auto found = std::find(list.begin(), list.end(), link);
    const bool present = found != list.end();
    if (enabled == present)
        return;
    if (enabled)
        list.push_back(link);
    else
        list.erase(found);
    editor_->UpdateFlowAnimation();
}

bool DuiNodeEditor::LinkFlow(DuiLinkId link) const
{
    const auto& list = editor_->flowingLinks;
    return std::find(list.begin(), list.end(), link) != list.end();
}

void DuiNodeEditor::Impl::UpdateFlowAnimation()
{
    if (clock == nullptr)
        return;
    // 无流动连线：停掉任务，避免持续重绘
    if (flowingLinks.empty())
    {
        if (flowTask != 0)
            clock->Cancel(flowTask);
        flowTask = 0;
        flowPhase = 0.0;
        return;
    }
    if (flowTask != 0)
        return; // 已在动
    OnFlowTick(0.0);
}

void DuiNodeEditor::Impl::OnFlowTick(double progress)
{
    flowPhase = progress;
    flowTask = 0;
    if (clock == nullptr || flowingLinks.empty())
        return;
    // 逐帧推进相位；任务完成后立刻重排下一帧，形成连续动画
    flowTask = clock->Schedule(16, [this](double value) { OnFlowTick(value); });
}

bool DuiNodeEditor::FitGroupToContents(DuiNodeId group)
{
    const DuiGraphNode* entry = editor_->graph.FindNode(group);
    if (entry == nullptr || entry->kind != DuiNodeKind::Group)
        return false;
    // 由内部节点的包围盒加内边距推出分组尺寸
    core::Rect content{};
    bool first = true;
    for (const auto& node : editor_->graph.Nodes())
    {
        if (node.kind != DuiNodeKind::Normal)
            continue;
        if (!editor_->graph.NodeBelongsTo(node.id, group))
            continue;
        const core::Rect bounds{node.position.x, node.position.y,
                                node.position.x + node.size.width,
                                node.position.y + node.size.height};
        if (first)
        {
            content = bounds;
            first = false;
            continue;
        }
        content.left = (std::min)(content.left, bounds.left);
        content.top = (std::min)(content.top, bounds.top);
        content.right = (std::max)(content.right, bounds.right);
        content.bottom = (std::max)(content.bottom, bounds.bottom);
    }
    if (first)
        return false; // 组内无节点
    DuiGraphNode* target = editor_->graph.FindNode(group);
    if (target == nullptr)
        return false;
    const int padding = editor_->style.nodePadding;
    const int header = editor_->style.headerHeight;
    StructureDelta delta;
    delta.positionsBefore.emplace_back(group, target->position);
    delta.sizesBefore.emplace_back(group, target->size);
    target->position = {content.left - padding, content.top - padding - header};
    target->size = {content.Width() + padding * 2,
                    content.Height() + padding * 2 + header};
    delta.positionsAfter.emplace_back(group, target->position);
    delta.sizesAfter.emplace_back(group, target->size);
    editor_->CommitStructure(std::move(delta));
    editor_->NotifyGraphChanged();
    return true;
}

core::Rect DuiNodeEditor::Impl::MinimapRect() const
{
    if (owner == nullptr)
        return {};
    const core::Rect bounds = owner->Bounds();
    constexpr int kWidth = 168;
    constexpr int kHeight = 112;
    constexpr int kPad = 10;
    return {bounds.right - kPad - kWidth, bounds.top + kPad,
            bounds.right - kPad, bounds.top + kPad + kHeight};
}

void DuiNodeEditor::Impl::PanFromMinimap(core::Point screen)
{
    const core::Rect mini = MinimapRect();
    const core::Rect world = graph.ContentBounds();
    if (mini.Empty() || world.Empty() || owner == nullptr)
        return;
    const double nx = (std::clamp)(static_cast<double>(screen.x - mini.left) / mini.Width(), 0.0, 1.0);
    const double ny = (std::clamp)(static_cast<double>(screen.y - mini.top) / mini.Height(), 0.0, 1.0);
    const core::Point canvas{
        world.left + static_cast<int>(std::lround(nx * world.Width())),
        world.top + static_cast<int>(std::lround(ny * world.Height()))};
    const core::Rect view = owner->Bounds();
    autoFit = false;
    transform.offset = {
        view.left + view.Width() / 2 - static_cast<int>(std::lround(canvas.x * transform.scale)),
        view.top + view.Height() / 2 - static_cast<int>(std::lround(canvas.y * transform.scale))};
    PlaceEmbeddedControls();
}

core::Rect DuiNodeEditor::Impl::RenameScreenRect() const
{
    const NodeLayout* layout = LayoutOf(renamingNode);
    if (layout == nullptr || layout->header.Empty())
        return {};
    const int padX = style.headerPaddingX;
    const int padY = style.headerPaddingY;
    const int iconSlot = (std::max)(0, style.headerIconSlot);
    const core::Rect title{layout->header.left + padX + iconSlot, layout->header.top + padY,
                           layout->header.right - padX, layout->header.bottom - padY};
    if (title.Empty())
        return {};
    return transform.ToScreenRect(title);
}

void DuiNodeEditor::Impl::UnbindRenameInput()
{
    if (renameInput == nullptr)
        return;
    renameInput->SetSubmitHandler({});
    renameInput->SetCancelHandler({});
    renameInput->SetFocusLostHandler({});
    renameInput->SetChangedHandler({});
    renameInput->SetVisible(false);
}

void DuiNodeEditor::Impl::PlaceRenameInput()
{
    if (renameInput == nullptr)
        return;
    if (!renamingNode.Valid())
    {
        renameInput->SetVisible(false);
        return;
    }
    const core::Rect field = RenameScreenRect();
    if (field.Empty())
    {
        renameInput->SetVisible(false);
        return;
    }
    renameInput->SetBounds(field);
    renameInput->SetVisible(true);
}

void DuiNodeEditor::Impl::BindRenameInput()
{
    if (renameInput == nullptr)
        return;
    ui::DuiTextInputOptions options;
    renameInput->SetOptions(options);
    renameInput->SetPlaceholder({});
    renameInput->SetText(renameBuffer);
    renameInput->SetEnabled(true);
    renameInput->SetBorderVisible(false);
    PlaceRenameInput();
    renameInput->SetSubmitHandler([this] { FinishRename(true); });
    renameInput->SetCancelHandler([this] { FinishRename(false); });
    renameInput->SetFocusLostHandler([this] { FinishRename(true); });
    renameInput->Focus();
}

bool DuiNodeEditor::Impl::BeginRename()
{
    if (readOnly)
        return false;
    DuiNodeId target = hovered.node;
    if (!target.Valid() && !selectedNodes.empty())
        target = selectedNodes.back();
    const DuiGraphNode* node = graph.FindNode(target);
    if (node == nullptr
        || (node->kind != DuiNodeKind::Normal && node->kind != DuiNodeKind::Comment))
        return false;
    FinishRename(true);
    node = graph.FindNode(target);
    if (node == nullptr)
        return false;
    renamingNode = target;
    renameBuffer = node->title;
    if (renameInput != nullptr)
        BindRenameInput();
    return true;
}

void DuiNodeEditor::Impl::FinishRename(bool commit)
{
    if (!renamingNode.Valid() || renameClosing)
        return;
    renameClosing = true;
    const std::string next = renameInput != nullptr ? renameInput->Text() : renameBuffer;
    UnbindRenameInput();
    renameSelecting = false;
    const DuiNodeId id = renamingNode;
    DuiGraphNode* node = graph.FindNode(id);
    renamingNode = {};
    renameBuffer.clear();
    renameClosing = false;
    if (!commit || node == nullptr || node->title == next)
        return;
    const std::string previous = node->title;
    node->title = next;
    CommitTitle(id, previous, next);
    NotifyGraphChanged();
}

void DuiNodeEditor::Impl::HandleRenameText(std::string_view text)
{
    if (!renamingNode.Valid() || renameInput != nullptr || text.empty())
        return;
    renameBuffer.append(text.data(), text.size());
}

bool DuiNodeEditor::BeginRenameSelected() { return editor_->BeginRename(); }
void DuiNodeEditor::SetMinimapVisible(bool visible) { editor_->minimapVisible = visible; }
bool DuiNodeEditor::MinimapVisible() const { return editor_->minimapVisible; }

void DuiNodeEditor::SetReadOnly(bool readOnly) { editor_->readOnly = readOnly; }
bool DuiNodeEditor::ReadOnly() const { return editor_->readOnly; }
void DuiNodeEditor::SetGridVisible(bool visible) { editor_->gridVisible = visible; }
bool DuiNodeEditor::GridVisible() const { return editor_->gridVisible; }
void DuiNodeEditor::SetSnapToGrid(bool snap) { editor_->snapToGrid = snap; }
bool DuiNodeEditor::SnapToGrid() const { return editor_->snapToGrid; }
void DuiNodeEditor::SetSelectionChangedHandler(std::function<void()> handler)
{
    editor_->selectionChanged = std::move(handler);
}
void DuiNodeEditor::SetGraphChangedHandler(std::function<void()> handler)
{
    editor_->graphChanged = std::move(handler);
}

void DuiNodeEditor::SetCanLinkFilter(std::function<bool(DuiPinId, DuiPinId)> filter)
{
    editor_->canLinkFilter = std::move(filter);
}
DuiPinId DuiNodeEditor::HoveredPin() const { return editor_->hovered.pin; }
DuiNodeId DuiNodeEditor::HoveredNode() const { return editor_->hovered.node; }

} // namespace ysDui::controls::graph
