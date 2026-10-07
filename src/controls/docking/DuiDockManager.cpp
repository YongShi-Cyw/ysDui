/**
 * 文件名：DuiDockManager.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：停靠管理器实现：模型与 DuiSplitter / DuiTabPage 视图的同步、拖拽停靠、悬浮与自动隐藏。
 */
#include "ysDui/controls/docking/DuiDockManager.hpp"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "ysDui/controls/layout/DuiSplitter.hpp"
#include "ysDui/controls/list/DuiTab.hpp"
#include "ysDui/controls/list/DuiTabPage.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiPaintChildren.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/ui/DuiFrameHost.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"

namespace ysDui::controls::docking {
namespace {
/** 分割条厚度（逻辑像素）。 */
constexpr int kSplitBarThickness = 4;
/** 分割两侧各自的最小可读尺寸。 */
constexpr int kSplitMinimumExtent = 60;
/** 拖动起效的最小位移，避免点击被误判为拖动。 */
constexpr int kDragThreshold = 6;
/** 边缘窄条默认厚度。 */
constexpr int kDefaultStripThickness = 24;
/** 窄条上每个条目的长度（逻辑像素）。 */
constexpr int kStripItemExtent = 26;
/** 悬停窄条时滑出面板的厚度。 */
constexpr int kPeekThickness = 260;
/** 拖动时随指针绘制的幻影尺寸。 */
constexpr core::Size kGhostSize{150, 22};
/** 组边缘视为"切出新组"的带宽比例分母。 */
constexpr int kEdgeBandDivisor = 4;
/** 停靠指示器（十字）每格边长，整体为 3x3 格。 */
constexpr int kGuideCell = 30;
/** 标签宽度下限。 */
constexpr int kDefaultTabWidthMinimum = 64;
/** 标签宽度上限。 */
constexpr int kDefaultTabWidthMaximum = 180;
/** 空布局提示文字。 */
constexpr const char* kEmptyHint = "No panes";
/** 悬浮窗标题栏「停靠」按钮的标识。 */
constexpr int kDockBackCaptionId = 0x4454; // 'DT'

/** 拖放目标类型。 */
enum class DropKind
{
    None,   // 无目标。
    Merge,  // 并入目标组成为标签。
    Left,   // 在目标组左侧切出新组。
    Right,  // 在目标组右侧切出新组。
    Top,    // 在目标组上方切出新组。
    Bottom, // 在目标组下方切出新组。
    Reorder,// 在同一个组内调整标签顺序。
    Float,  // 拖出为独立悬浮窗口。
};

/** 拖放目标。 */
struct DropTarget final
{
    DropKind kind{DropKind::None};
    std::uint64_t groupNodeId{};
    std::string targetPaneId;      // 参照窗格（并入或切分时的锚点）
    int insertionIndex{};          // 仅 Reorder 有效
    core::Rect preview;            // 落点预览矩形
};

/** @return 窄条厚度/条目几何用的槽位顺序。 */
constexpr DuiDockSlot kStripSlots[4] = {DuiDockSlot::Left, DuiDockSlot::Right,
                                        DuiDockSlot::Top, DuiDockSlot::Bottom};

/** @return 槽位是否属于左右（纵向）窄条。 */
bool IsVerticalStrip(DuiDockSlot slot)
{
    return slot == DuiDockSlot::Left || slot == DuiDockSlot::Right;
}

/** @return 槽位在窄条数组中的下标；不是左右上下时返回 -1。 */
int StripIndex(DuiDockSlot slot)
{
    for (int index = 0; index < 4; ++index)
    {
        if (kStripSlots[index] == slot)
            return index;
    }
    return -1;
}

/** @return 拖放目标对应的槽位。 */
DuiDockSlot SlotOf(DropKind kind)
{
    switch (kind)
    {
    case DropKind::Left: return DuiDockSlot::Left;
    case DropKind::Right: return DuiDockSlot::Right;
    case DropKind::Top: return DuiDockSlot::Top;
    case DropKind::Bottom: return DuiDockSlot::Bottom;
    default: return DuiDockSlot::Center;
    }
}

/** @return 落点提示文字，供测试与宿主判断。 */
const char* DropKindName(DropKind kind)
{
    switch (kind)
    {
    case DropKind::Merge: return "merge";
    case DropKind::Left: return "left";
    case DropKind::Right: return "right";
    case DropKind::Top: return "top";
    case DropKind::Bottom: return "bottom";
    case DropKind::Reorder: return "reorder";
    case DropKind::Float: return "float";
    default: return {};
    }
}

/** 纵向窄条上的文字只能竖排，且窄条宽度放不下整串：只取首字，完整标题由调用方提示。 */
void PaintStripLabel(render::Canvas& canvas, std::string_view text, core::Rect bounds,
                     bool vertical, const render::DuiTextStyle& style)
{
    if (text.empty() || bounds.Width() <= 0 || bounds.Height() <= 0)
        return;
    if (!vertical)
    {
        canvas.DrawText(text, bounds, style, render::DuiTextAlignment::Center, false);
        return;
    }
    canvas.DrawText(text.substr(0, 1), bounds, style, render::DuiTextAlignment::Center, false);
}

/** 停靠指示器内某一格的方向符号：外侧画"拟停靠的位置条"，由中心向该侧画箭头。 */
void PaintGuideGlyph(render::Canvas& canvas, core::Rect cell, DropKind kind, core::Color color)
{
    constexpr int kBarThickness = 5; // 位置条厚度
    constexpr int kArrowLength = 9;  // 箭头长度
    constexpr int kArrowHalf = 5;    // 箭头半宽
    const int centerX = (cell.left + cell.right) / 2;
    const int centerY = (cell.top + cell.bottom) / 2;
    if (kind == DropKind::Merge)
    {
        // 中心格：并入目标组，用方块表示"成为其中一个标签"
        canvas.FillRoundedRect({centerX - 7, centerY - 7, centerX + 7, centerY + 7}, 2, color);
        return;
    }
    switch (kind)
    {
    case DropKind::Left:
        canvas.FillRect({cell.left + 5, cell.top + 8, cell.left + 5 + kBarThickness, cell.bottom - 8}, color);
        {
            render::DuiPath arrow;
            arrow.MoveTo({cell.left + 7 + kBarThickness + kArrowLength, centerY - kArrowHalf});
            arrow.LineTo({cell.left + 7 + kBarThickness + kArrowLength, centerY + kArrowHalf});
            arrow.LineTo({cell.left + 7 + kBarThickness, centerY});
            arrow.Close();
            canvas.FillPath(arrow, color);
        }
        break;
    case DropKind::Right:
        canvas.FillRect({cell.right - 5 - kBarThickness, cell.top + 8, cell.right - 5, cell.bottom - 8}, color);
        {
            render::DuiPath arrow;
            arrow.MoveTo({cell.right - 7 - kBarThickness - kArrowLength, centerY - kArrowHalf});
            arrow.LineTo({cell.right - 7 - kBarThickness - kArrowLength, centerY + kArrowHalf});
            arrow.LineTo({cell.right - 7 - kBarThickness, centerY});
            arrow.Close();
            canvas.FillPath(arrow, color);
        }
        break;
    case DropKind::Top:
        canvas.FillRect({cell.left + 8, cell.top + 5, cell.right - 8, cell.top + 5 + kBarThickness}, color);
        {
            render::DuiPath arrow;
            arrow.MoveTo({centerX - kArrowHalf, cell.top + 7 + kBarThickness + kArrowLength});
            arrow.LineTo({centerX + kArrowHalf, cell.top + 7 + kBarThickness + kArrowLength});
            arrow.LineTo({centerX, cell.top + 7 + kBarThickness});
            arrow.Close();
            canvas.FillPath(arrow, color);
        }
        break;
    case DropKind::Bottom:
        canvas.FillRect({cell.left + 8, cell.bottom - 5 - kBarThickness, cell.right - 8, cell.bottom - 5}, color);
        {
            render::DuiPath arrow;
            arrow.MoveTo({centerX - kArrowHalf, cell.bottom - 7 - kBarThickness - kArrowLength});
            arrow.LineTo({centerX + kArrowHalf, cell.bottom - 7 - kBarThickness - kArrowLength});
            arrow.LineTo({centerX, cell.bottom - 7 - kBarThickness});
            arrow.Close();
            canvas.FillPath(arrow, color);
        }
        break;
    default:
        break;
    }
}
} // namespace

class DuiDockManager::Impl
{
public:
    /** 一个标签组对应的视图。 */
    struct GroupView final
    {
        list::DuiTabPage* page{};
        std::vector<std::string> paneIds; // 与标签页的页序一致
    };

    /** 悬浮窗格的登记信息，用于收回时恢复标题与落位锚点。 */
    struct FloatRecord final
    {
        std::string title;
        std::string anchorPaneId;  // 拖出时同组的首个窗格；收回时并入该组，为空则回中央组
        ui::DuiFrameOptions options; // 保留窗口配置，供后续改标题/图标时 SetOptions
    };

    DuiDockTree tree;
    std::map<std::uint64_t, layout::DuiSplitter*> splitterByNode;
    std::map<std::uint64_t, GroupView> groupByNode;
    std::map<std::string, core::Control*> contentByPaneId;
    std::map<std::string, bool> closeableByPaneId;
    std::map<std::string, std::shared_ptr<const render::DuiImage>> iconByPaneId;
    std::map<std::string, std::unique_ptr<core::Control>> detached;
    std::map<std::string, std::unique_ptr<core::Control>> autoHideContents; // 收起窗格的内容常驻
    std::map<std::string, std::unique_ptr<ui::IFrameHost>> floats;
    std::map<std::string, FloatRecord> floatRecords;
    /**
     * 已关闭/已收回、等待安全时机销毁的悬浮窗体。
     * 不能在窗体自身的消息回调里销毁它（会释放正在执行的回调闭包），故一律登记到这里，
     * 由 `FlushRebuild` 在非窗体回调期间释放。
     */
    std::vector<std::unique_ptr<ui::IFrameHost>> retiredFrames;
    /** true 表示当前正处于某个悬浮窗体的消息回调内（此时不得释放 retiredFrames）。 */
    bool insideFrameCallback{};
    std::vector<std::string> pendingClosed;
    std::function<void(std::string)> paneClosed;
    std::function<void(std::string)> activePaneChanged;
    std::function<void(std::string)> paneFloated;
    ui::IUiHostFactory* hostFactory{};
    std::string activePaneId;
    std::string keepAlivePaneId;                     // 正在拖出、内容不可释放的窗格
    std::unique_ptr<core::Control> floatedContent;   // 拖出后的内容，交给悬浮窗口
    bool pendingRebuild{};

    // 布局与外观
    core::Rect contentRect;                          // 扣除窄条后的内容区
    core::Rect stripRects[4];                        // 四条窄条的占位
    int stripThickness{kDefaultStripThickness};
    int tabWidthMinimum{kDefaultTabWidthMinimum};
    int tabWidthMaximum{kDefaultTabWidthMaximum};

    // 拖动
    std::string dragPaneId;
    std::uint64_t dragGroupNodeId{};
    list::DuiTab* dragHeader{};  // 拖动来源的标签控件，需转发抬起/取消以复位其内部状态
    core::Point dragStart;      // 按下点
    core::Point dragPoint;      // 当前点
    bool dragCandidate{};        // 已按下但尚未越过阈值
    bool dragging{};             // 已进入拖动
    DropTarget drop;
    core::Rect guideRect;        // 十字停靠指示器；为空表示不显示

    // 悬停滑出
    std::string peekPaneId;
    core::Rect peekRect;

    // ---------------------------------------------------------------- 内容池

    /** 摘出全部标签页内容，供重建前暂存；重建后未被消费的内容即已从模型中移除。 */
    void DetachContents()
    {
        for (auto& entry : groupByNode)
        {
            GroupView& view = entry.second;
            if (view.page == nullptr)
                continue;
            // 倒序摘出，避免下标在摘出过程中偏移
            for (int index = view.page->PageCount() - 1; index >= 0; --index)
            {
                std::unique_ptr<core::Control> content = view.page->ReleasePage(index);
                if (content == nullptr)
                    continue;
                const std::size_t position = static_cast<std::size_t>(index);
                if (position < view.paneIds.size())
                    detached.emplace(view.paneIds[position], std::move(content));
            }
        }
    }

    /** @return 该窗格的图标；未设置时为空指针。 */
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& IconOf(std::string_view paneId) const
    {
        static const std::shared_ptr<const render::DuiImage> empty;
        const auto found = iconByPaneId.find(std::string(paneId));
        return found == iconByPaneId.end() ? empty : found->second;
    }

    /** @return 该窗格是否显示关闭按钮；未登记时按显示处理。 */
    [[nodiscard]] bool CloseableOf(std::string_view paneId) const
    {
        const auto found = closeableByPaneId.find(std::string(paneId));
        return found == closeableByPaneId.end() ? true : found->second;
    }

    // ---------------------------------------------------------------- 视图搭建

    /** 按模型递归搭建控件树；调用前须先摘出内容。 */
    std::unique_ptr<core::Control> BuildNode(const DuiDockNode& node)
    {
        if (node.kind == DuiDockNodeKind::Group)
            return BuildGroup(node);
        auto splitter = std::make_unique<layout::DuiSplitter>();
        splitter->SetOrientation(node.orientation == DuiDockOrientation::Vertical
            ? layout::DuiSplitterOrientation::Vertical
            : layout::DuiSplitterOrientation::Horizontal);
        splitter->SetBarThickness(kSplitBarThickness);
        splitter->SetMinSizes(kSplitMinimumExtent, kSplitMinimumExtent);
        const std::uint64_t nodeId = node.id;
        splitter->SetValueChangedHandler([this, nodeId](int)
        {
            // 只把比例写回模型：像素值在宿主尺寸变化后会走形
            const auto found = splitterByNode.find(nodeId);
            if (found != splitterByNode.end() && found->second != nullptr)
                tree.SetSplitFraction(nodeId, found->second->SplitFraction());
        });
        // 先设比例：此刻分割还没有尺寸，比例会挂起并在首次布局时生效
        splitter->SetSplitFraction(node.fraction);
        splitterByNode[nodeId] = splitter.get();
        for (int index = 0; index < static_cast<int>(node.children.size()) && index < 2; ++index)
            splitter->SetPane(index, BuildNode(*node.children[static_cast<std::size_t>(index)]));
        return splitter;
    }

    /** 为一个标签组建视图并挂上内容。 */
    std::unique_ptr<core::Control> BuildGroup(const DuiDockNode& node)
    {
        auto page = std::make_unique<list::DuiTabPage>();
        page->SetHeaderHeight(28);
        GroupView view;
        view.page = page.get();
        for (const DuiDockPane& pane : node.panes)
        {
            const auto found = detached.find(pane.id);
            if (found == detached.end() || found->second == nullptr)
                continue; // 无对应内容：跳过，避免出现空标签
            const int index = page->AddPage(pane.title, std::move(found->second), IconOf(pane.id));
            detached.erase(found);
            view.paneIds.push_back(pane.id);
            page->Header().SetCloseable(index, CloseableOf(pane.id));
            contentByPaneId[pane.id] = page->PageAt(index);
        }
        const std::uint64_t nodeId = node.id;
        // 标签条交给本控件统一处理拖动：组内重排与跨组停靠走同一套落点判定
        page->Header().SetMinTabWidth(tabWidthMinimum);
        page->Header().SetMaxTabWidth(tabWidthMaximum);
        page->Header().SetAutoFitTabWidth(true);
        page->Header().SetWheelSelect(true);
        page->Header().SetReorderEnabled(false);
        // 关闭由标签自身回调触发：只改模型并挂起重建立即返回，销毁推迟到绘制前
        page->Header().SetCloseHandler([this, nodeId](int index) { ClosePaneAt(nodeId, index); });
        page->SetSelectionChangedHandler([this, nodeId](int index) { SelectAtIndex(nodeId, index); });
        if (!view.paneIds.empty())
            page->SetSelectedIndex((std::clamp)(node.active, 0, static_cast<int>(view.paneIds.size()) - 1),
                                   false);
        groupByNode[nodeId] = std::move(view);
        return page;
    }

    /** 重建控件树，并释放已从模型中移除的窗格内容。 */
    void Rebuild(DuiDockManager& owner)
    {
        pendingRebuild = false;
        // 收起窗格的内容先并入待分配池，由模型在此次重建后决定去向
        for (auto& entry : autoHideContents)
        {
            if (detached.find(entry.first) == detached.end())
                detached[entry.first] = std::move(entry.second);
        }
        autoHideContents.clear();
        DetachContents();
        while (!owner.Children().empty())
            (void)owner.RemoveChild(owner.Children().front().get());
        splitterByNode.clear();
        groupByNode.clear();
        contentByPaneId.clear();
        if (std::unique_ptr<core::Control> root = BuildNode(tree.Root()))
            owner.AddChild(std::move(root));

        // 回收：仍收起的回内容常驻池，正在拖出的交给 FloatPane，其余随池释放
        for (auto& entry : detached)
        {
            if (tree.IsPaneAutoHidden(entry.first))
            {
                contentByPaneId[entry.first] = entry.second.get();
                autoHideContents[entry.first] = std::move(entry.second);
            }
            else if (entry.first == keepAlivePaneId)
            {
                floatedContent = std::move(entry.second);
            }
        }
        detached.clear();
        keepAlivePaneId.clear();
        for (const auto& entry : autoHideContents)
            contentByPaneId[entry.first] = entry.second.get();

        if (!tree.Contains(activePaneId))
            activePaneId = tree.ActivePane();
        if (peekPaneId.empty() || !tree.IsPaneAutoHidden(peekPaneId))
        {
            peekPaneId.clear();
            peekRect = {};
        }
        if (!owner.Bounds().Empty())
            owner.Layout(owner.Bounds());
    }

    // ---------------------------------------------------------------- 模型动作

    /** 关闭一个标签；只改模型，视图重建交给挂起标记。 */
    void ClosePaneAt(std::uint64_t groupNodeId, int index)
    {
        const auto found = groupByNode.find(groupNodeId);
        if (found == groupByNode.end())
            return;
        const std::size_t position = static_cast<std::size_t>(index);
        if (index < 0 || position >= found->second.paneIds.size())
            return;
        if (!RemovePaneInternal(found->second.paneIds[position]))
            return;
        pendingRebuild = true;
    }

    /** 从模型与登记表移除窗格。 */
    bool RemovePaneInternal(std::string_view paneId)
    {
        if (!tree.RemovePane(paneId))
            return false;
        closeableByPaneId.erase(std::string(paneId));
        pendingClosed.emplace_back(paneId);
        pendingRebuild = true;
        return true;
    }

    /** 标签切换：写回模型并广播活动窗格。 */
    void SelectAtIndex(std::uint64_t groupNodeId, int index)
    {
        const auto found = groupByNode.find(groupNodeId);
        if (found == groupByNode.end())
            return;
        const std::size_t position = static_cast<std::size_t>(index);
        if (index < 0 || position >= found->second.paneIds.size())
            return;
        tree.SetGroupActive(groupNodeId, index);
        SetActivePaneId(found->second.paneIds[position]);
    }

    /** 更新活动窗格并在变化时通知。 */
    void SetActivePaneId(std::string paneId)
    {
        if (activePaneId == paneId)
            return;
        activePaneId = std::move(paneId);
        if (activePaneChanged)
            activePaneChanged(activePaneId);
    }

    // ---------------------------------------------------------------- 命中判定

    /** @return 该窗格在其组内的标签下标；未找到返回 -1。 */
    [[nodiscard]] int TagIndexOf(std::string_view paneId, GroupView*& view)
    {
        for (auto& entry : groupByNode)
        {
            GroupView& candidate = entry.second;
            const auto found = std::find(candidate.paneIds.begin(), candidate.paneIds.end(), paneId);
            if (found != candidate.paneIds.end())
            {
                view = &candidate;
                return static_cast<int>(found - candidate.paneIds.begin());
            }
        }
        view = nullptr;
        return -1;
    }

    /** @return 坐标所在的标签条控件；不在任何标签条上时为空。 */
    list::DuiTab* HitTabStrip(core::Point point, std::uint64_t* nodeId = nullptr)
    {
        for (auto& entry : groupByNode)
        {
            GroupView& view = entry.second;
            if (view.page == nullptr)
                continue;
            if (view.page->HeaderRect().Contains(point))
            {
                if (nodeId != nullptr)
                    *nodeId = entry.first;
                return &view.page->Header();
            }
        }
        return nullptr;
    }

    /** @return 内容区中命中坐标的组视图；不在任何组内时为空。 */
    GroupView* HitGroup(core::Point point, std::uint64_t* nodeId = nullptr)
    {
        for (auto& entry : groupByNode)
        {
            GroupView& view = entry.second;
            if (view.page != nullptr && view.page->Bounds().Contains(point))
            {
                if (nodeId != nullptr)
                    *nodeId = entry.first;
                return &view;
            }
        }
        return nullptr;
    }

    /** @return 该条边上第 index 个窄条条目的矩形。 */
    [[nodiscard]] core::Rect StripItemRect(DuiDockSlot slot, int index) const
    {
        const int strip = StripIndex(slot);
        if (strip < 0)
            return {};
        const core::Rect rect = stripRects[strip];
        if (IsVerticalStrip(slot))
        {
            const int top = rect.top + index * kStripItemExtent;
            return {rect.left, top, rect.right, top + kStripItemExtent};
        }
        const int left = rect.left + index * kStripItemExtent;
        return {left, rect.top, left + kStripItemExtent, rect.bottom};
    }

    /** @return 该槽位上已收起的窗格标识，按加入顺序。 */
    [[nodiscard]] std::vector<std::string> HiddenOn(DuiDockSlot slot) const
    {
        std::vector<std::string> ids;
        for (const DuiDockAutoHideItem& item : tree.AutoHideItems())
        {
            if (item.slot == slot)
                ids.push_back(item.id);
        }
        return ids;
    }

    /** @return 坐标命中的窄条条目；未命中时返回空标识。 */
    std::string HitStripItem(core::Point point, DuiDockSlot* slot = nullptr) const
    {
        for (const DuiDockSlot side : kStripSlots)
        {
            const std::vector<std::string> ids = HiddenOn(side);
            for (int index = 0; index < static_cast<int>(ids.size()); ++index)
            {
                if (StripItemRect(side, index).Contains(point))
                {
                    if (slot != nullptr)
                        *slot = side;
                    return ids[static_cast<std::size_t>(index)];
                }
            }
        }
        return {};
    }

    /** @return 某条边上是否存在收起窗格。 */
    [[nodiscard]] bool HasStrip(DuiDockSlot slot) const { return !HiddenOn(slot).empty(); }

    /** @return 悬停滑出面板的矩形。 */
    [[nodiscard]] core::Rect PeekRectOf(DuiDockSlot slot) const
    {
        switch (slot)
        {
        case DuiDockSlot::Left:
            return {contentRect.left, contentRect.top,
                    (std::min)(contentRect.right, contentRect.left + kPeekThickness), contentRect.bottom};
        case DuiDockSlot::Right:
            return {(std::max)(contentRect.left, contentRect.right - kPeekThickness), contentRect.top,
                    contentRect.right, contentRect.bottom};
        case DuiDockSlot::Top:
            return {contentRect.left, contentRect.top, contentRect.right,
                    (std::min)(contentRect.bottom, contentRect.top + kPeekThickness)};
        case DuiDockSlot::Bottom:
            return {contentRect.left, (std::max)(contentRect.top, contentRect.bottom - kPeekThickness),
                    contentRect.right, contentRect.bottom};
        default:
            return {};
        }
    }

    // ---------------------------------------------------------------- 拖动

    /** @return 指针落在组内标签序列中的插入下标。 */
    [[nodiscard]] int InsertionIndexAt(const GroupView& view, core::Point point) const
    {
        const int count = static_cast<int>(view.paneIds.size());
        for (int index = 0; index < count; ++index)
        {
            const core::Rect tabRect = view.page->Header().TabRect(index);
            if (point.x < (tabRect.left + tabRect.right) / 2)
                return index;
        }
        return count;
    }

    /** @return 组内标签插入位置的提示条（重排时的落点预览）。 */
    [[nodiscard]] static core::Rect InsertionBarRect(const GroupView& view, int insertionIndex)
    {
        if (view.page == nullptr)
            return {};
        const core::Rect header = view.page->HeaderRect();
        const int count = static_cast<int>(view.paneIds.size());
        const int index = (std::clamp)(insertionIndex, 0, count);
        int x = header.left + 2;
        if (index > 0)
        {
            const core::Rect previous = view.page->Header().TabRect(index - 1);
            x = previous.right + 2;
        }
        return {x - 1, header.top + 4, x + 1, header.bottom - 4};
    }

    /** 按当前指针位置计算落点。 */
    DropTarget ComputeDrop(core::Point point)
    {
        DropTarget target;
        guideRect = {};
        for (const auto& entry : groupByNode)
        {
            const GroupView& view = entry.second;
            if (view.page == nullptr || view.paneIds.empty())
                continue;
            const core::Rect rect = view.page->Bounds();
            if (!rect.Contains(point))
                continue;
            target.groupNodeId = entry.first;
            target.targetPaneId = view.paneIds.front();
            // 拖回自己原来的那一组：单窗格组的任何"切分"都是空操作（组里就它一个，
            // 切完还是它一个），此时不出十字停靠提示，只做组内重排。
            if (entry.first == dragGroupNodeId && view.paneIds.size() <= 1)
            {
                target.kind = DropKind::Reorder;
                target.insertionIndex = 0;
                target.preview = InsertionBarRect(view, 0);
                return target;
            }
            // 指示器锚在目标组中心，随目标组切换而移动
            guideRect = GuideRectFor(rect);

            DropKind kind = HitTestGuide(point);
            if (kind == DropKind::None)
            {
                const int bandX = (std::max)(1, rect.Width() / kEdgeBandDivisor);
                const int bandY = (std::max)(1, rect.Height() / kEdgeBandDivisor);
                // 落在标签条上只做融入或重排；切分由指示器或内容区四边触发，避免贴着标签点就把组切了
                const bool overHeader = view.page->HeaderRect().Contains(point);
                if (!overHeader && point.x < rect.left + bandX)
                    kind = DropKind::Left;
                else if (!overHeader && point.x > rect.right - bandX)
                    kind = DropKind::Right;
                else if (!overHeader && point.y < rect.top + bandY)
                    kind = DropKind::Top;
                else if (!overHeader && point.y > rect.bottom - bandY)
                    kind = DropKind::Bottom;
                else
                    kind = DropKind::Merge;
            }
            // 落在原组内既不改结构也不换组时，退化为组内标签重排
            if (entry.first == dragGroupNodeId && kind == DropKind::Merge)
            {
                target.kind = DropKind::Reorder;
                target.insertionIndex = InsertionIndexAt(view, point);
                target.preview = InsertionBarRect(view, target.insertionIndex);
                return target;
            }
            target.kind = kind;
            target.preview = PreviewRectFor(kind, rect);
            return target;
        }
        target.kind = DropKind::Float;
        return target;
    }

    /**
     * 落地拖放动作。
     * @param paneId 被拖动的窗格
     * @param target 落点
     * @return 需要重建视图时返回 true
     */
    bool ApplyDrop(std::string_view paneId, const DropTarget& target)
    {
        if (paneId.empty())
            return false;
        switch (target.kind)
        {
        case DropKind::Merge:
            return tree.MovePane(paneId, target.targetPaneId, DuiDockSlot::Center);
        case DropKind::Left:
        case DropKind::Right:
        case DropKind::Top:
        case DropKind::Bottom:
            return tree.MovePane(paneId, target.targetPaneId, SlotOf(target.kind), 0.5);
        case DropKind::Reorder:
        {
            GroupView* view = nullptr;
            const int from = TagIndexOf(paneId, view);
            if (view == nullptr || from < 0)
                return false;
            int to = target.insertionIndex;
            if (to > from)
                --to; // 插入下标换算为移除后的目标下标
            return tree.ReorderPane(target.groupNodeId, from, to);
        }
        default:
            return false; // Float 由调用方另行处理
        }
    }

    /** 结束拖动并清理状态。 */
    void ResetDrag()
    {
        dragPaneId.clear();
        dragGroupNodeId = 0;
        dragHeader = nullptr;
        dragCandidate = false;
        dragging = false;
        drop = {};
        guideRect = {};
    }

    // ------------------------------------------------------------ 十字停靠指示器

    /** @return 指示器矩形：以目标组中心为锚点，整体 3x3 格。 */
    [[nodiscard]] static core::Rect GuideRectFor(const core::Rect& groupBounds)
    {
        const int extent = kGuideCell * 3;
        const int left = (groupBounds.left + groupBounds.right) / 2 - extent / 2;
        const int top = (groupBounds.top + groupBounds.bottom) / 2 - extent / 2;
        return {left, top, left + extent, top + extent};
    }

    /** @return 指示器内按行列取的格子矩形（列、行均为 0..2）。 */
    [[nodiscard]] core::Rect GuideCellAt(int column, int row) const
    {
        const int left = guideRect.left + column * kGuideCell;
        const int top = guideRect.top + row * kGuideCell;
        return {left, top, left + kGuideCell, top + kGuideCell};
    }

    /** @return 某个落点方向对应的指示器格子；非指示器方向时为空。 */
    [[nodiscard]] core::Rect GuideCellRect(DropKind kind) const
    {
        if (guideRect.Empty())
            return {};
        switch (kind)
        {
        case DropKind::Top: return GuideCellAt(1, 0);
        case DropKind::Left: return GuideCellAt(0, 1);
        case DropKind::Merge: return GuideCellAt(1, 1);
        case DropKind::Right: return GuideCellAt(2, 1);
        case DropKind::Bottom: return GuideCellAt(1, 2);
        default: return {};
        }
    }

    /** @return 指针命中的指示器方向；未命中十字（含四角）时为空。 */
    [[nodiscard]] DropKind HitTestGuide(core::Point point) const
    {
        if (guideRect.Empty() || !guideRect.Contains(point))
            return DropKind::None;
        const int column = (point.x - guideRect.left) / kGuideCell;
        const int row = (point.y - guideRect.top) / kGuideCell;
        if (row == 0 && column == 1) return DropKind::Top;
        if (row == 1 && column == 0) return DropKind::Left;
        if (row == 1 && column == 1) return DropKind::Merge;
        if (row == 1 && column == 2) return DropKind::Right;
        if (row == 2 && column == 1) return DropKind::Bottom;
        return DropKind::None; // 四个角不属于十字
    }

    /** @return 该落点方向对应的结果区域，用于拖动预览。 */
    [[nodiscard]] static core::Rect PreviewRectFor(DropKind kind, const core::Rect& group)
    {
        switch (kind)
        {
        case DropKind::Left:
            return {group.left, group.top, group.left + group.Width() / 2, group.bottom};
        case DropKind::Right:
            return {group.left + group.Width() / 2, group.top, group.right, group.bottom};
        case DropKind::Top:
            return {group.left, group.top, group.right, group.top + group.Height() / 2};
        case DropKind::Bottom:
            return {group.left, group.top + group.Height() / 2, group.right, group.bottom};
        default:
            return group;
        }
    }
};

DuiDockManager::DuiDockManager() : impl_(std::make_unique<Impl>()) {}
DuiDockManager::~DuiDockManager() = default;
DuiDockManager::DuiDockManager(DuiDockManager&&) noexcept = default;
DuiDockManager& DuiDockManager::operator=(DuiDockManager&&) noexcept = default;

bool DuiDockManager::AddPane(std::string id, std::string title, std::unique_ptr<core::Control> content,
                             DuiDockSlot slot, std::shared_ptr<const render::DuiImage> icon)
{
    if (id.empty() || content == nullptr || impl_->tree.Contains(id))
        return false;
    const std::string paneId = id;
    if (!impl_->tree.AddPane(id, std::move(title), slot))
        return false;
    if (icon)
        impl_->iconByPaneId[paneId] = std::move(icon);
    impl_->detached.emplace(paneId, std::move(content));
    impl_->Rebuild(*this);
    return true;
}

bool DuiDockManager::AddPaneBeside(std::string_view anchorPaneId, std::string id, std::string title,
                                   std::unique_ptr<core::Control> content, DuiDockSlot slot,
                                   double fraction)
{
    if (id.empty() || content == nullptr || impl_->tree.Contains(id))
        return false;
    const std::string paneId = id;
    if (!impl_->tree.AddPaneBeside(anchorPaneId, id, std::move(title), slot, fraction))
        return false;
    impl_->detached.emplace(paneId, std::move(content));
    impl_->Rebuild(*this);
    return true;
}

bool DuiDockManager::RemovePane(std::string_view id)
{
    if (!impl_->RemovePaneInternal(id))
        return false;
    FlushRebuild();
    return true;
}

bool DuiDockManager::MovePane(std::string_view id, std::string_view targetId, DuiDockSlot slot,
                              double fraction)
{
    if (!impl_->tree.MovePane(id, targetId, slot, fraction))
        return false;
    impl_->Rebuild(*this);
    return true;
}

bool DuiDockManager::SetPaneTitle(std::string_view id, std::string title)
{
    if (!impl_->tree.SetPaneTitle(id, std::move(title)))
        return false;
    Impl::GroupView* view = nullptr;
    const int index = impl_->TagIndexOf(id, view);
    if (index >= 0 && view != nullptr && view->page != nullptr)
        view->page->SetPageTitle(index, impl_->tree.PaneTitle(id));
    return true;
}

bool DuiDockManager::SetPaneCloseable(std::string_view id, bool closeable)
{
    if (!impl_->tree.Contains(id))
        return false;
    impl_->closeableByPaneId[std::string(id)] = closeable;
    Impl::GroupView* view = nullptr;
    const int index = impl_->TagIndexOf(id, view);
    if (index >= 0 && view != nullptr && view->page != nullptr)
        view->page->Header().SetCloseable(index, closeable);
    return true;
}

bool DuiDockManager::Contains(std::string_view id) const { return impl_->tree.Contains(id); }
int DuiDockManager::PaneCount() const { return impl_->tree.PaneCount(); }
std::vector<std::string> DuiDockManager::PaneIds() const { return impl_->tree.PaneIds(); }

std::string DuiDockManager::ActivePane() const
{
    return impl_->activePaneId.empty() ? impl_->tree.ActivePane() : impl_->activePaneId;
}

core::Control* DuiDockManager::PaneContent(std::string_view id) const
{
    const auto found = impl_->contentByPaneId.find(std::string(id));
    return found == impl_->contentByPaneId.end() ? nullptr : found->second;
}

bool DuiDockManager::SetActivePane(std::string_view id)
{
    if (!impl_->tree.SetActivePane(id))
        return false;
    Impl::GroupView* view = nullptr;
    const int index = impl_->TagIndexOf(id, view);
    if (index >= 0 && view != nullptr && view->page != nullptr)
        view->page->SetSelectedIndex(index, false);
    impl_->SetActivePaneId(std::string(id));
    return true;
}

bool DuiDockManager::SetPaneAutoHide(std::string_view id, bool autoHide, DuiDockSlot slot)
{
    if (!impl_->tree.SetPaneAutoHide(id, autoHide, slot))
        return false;
    impl_->Rebuild(*this);
    return true;
}

bool DuiDockManager::IsPaneAutoHidden(std::string_view id) const
{
    return impl_->tree.IsPaneAutoHidden(id);
}

void DuiDockManager::SetAutoHideStripThickness(int pixels)
{
    impl_->stripThickness = (std::max)(8, pixels);
    Layout(Bounds());
}

void DuiDockManager::SetTabWidthRange(int minimum, int maximum)
{
    impl_->tabWidthMinimum = (std::max)(24, minimum);
    impl_->tabWidthMaximum = (std::max)(impl_->tabWidthMinimum, maximum);
}

void DuiDockManager::SetHostFactory(ui::IUiHostFactory* factory) { impl_->hostFactory = factory; }

bool DuiDockManager::FloatPane(std::string_view id)
{
    if (impl_->hostFactory == nullptr || !impl_->tree.Contains(id) || impl_->floats.count(std::string(id)) > 0)
        return false;
    const std::string paneId{id};
    const std::string title = impl_->tree.PaneTitle(id);
    // 记录收回时的落位锚点：同组里第一个其它窗格；该组消失后回中央组
    Impl::FloatRecord record;
    record.title = title;
    if (const DuiDockNode* group = impl_->tree.FindGroup(id); group != nullptr)
    {
        for (const DuiDockPane& pane : group->panes)
        {
            if (pane.id != paneId)
            {
                record.anchorPaneId = pane.id;
                break;
            }
        }
    }
    // 内容经 Rebuild 落到 keepAlive 池，避免被当作已移除窗格释放
    impl_->keepAlivePaneId = paneId;
    if (!impl_->tree.RemovePane(id))
    {
        impl_->keepAlivePaneId.clear();
        return false;
    }
    impl_->Rebuild(*this);
    std::unique_ptr<core::Control> content = std::move(impl_->floatedContent);
    if (content == nullptr)
        return false;

    std::unique_ptr<ui::IFrameHost> frame = impl_->hostFactory->CreateFrameHost();
    if (frame == nullptr)
        return false; // 内容随局部变量释放，等同于该窗格被关闭
    ui::DuiFrameOptions options;
    options.title = title;
    // 观感对齐 DuiDialog：共用同一套主题槽（标题栏、轮廓、按钮反馈色）
    options.theme = &Theme();
    options.icon = impl_->IconOf(paneId);
    const core::Size desired = content->DesiredSize();
    options.size = {(std::max)(320, desired.width), (std::max)(240, desired.height)};
    options.minimumSize = {200, 150};
    options.resizable = true;
    // 标题栏提供「停靠」按钮：窗格从窗口侧也能收回停靠区，不必回到停靠树操作
    ui::DuiFrameCaptionButton dockButton;
    dockButton.id = kDockBackCaptionId;
    dockButton.action = ui::DuiFrameCaptionAction::Custom;
    dockButton.glyph = "Dock";
    dockButton.tooltip = "Dock back";
    options.captionButtons.push_back(std::move(dockButton));
    core::Control* rawContent = content.get();
    const bool shown = frame->Show(options, std::move(content),
        [rawContent](core::Rect bounds) { rawContent->Layout(bounds); },
        [rawContent](render::Canvas& canvas, core::Rect dirty)
        {
            if (auto* renderable = dynamic_cast<render::DuiRenderable*>(rawContent))
                renderable->Paint(canvas, dirty);
        });
    if (!shown)
        return false;

    frame->SetCaptionButtonHandler([this, paneId](int id)
    {
        if (id != kDockBackCaptionId)
            return;
        // 本回调运行在悬浮窗自身的消息栈内：DockFloatingPane 只做摘出 + 异步关窗，
        // 窗体对象留给安全点释放（见 retiredFrames），因此这里不会自毁。
        (void)DockFloatingPane(paneId);
    });

    // 悬浮窗关闭即窗格关闭，但此刻正在该窗口的关闭回调栈内：不能在此销毁它自己，
    // 只把对象移到待释放队列，由 FlushRebuild 在安全点销毁。
    frame->SetClosedHandler([this, paneId]
    {
        const auto found = impl_->floats.find(paneId);
        if (found != impl_->floats.end())
        {
            impl_->retiredFrames.push_back(std::move(found->second));
            impl_->floats.erase(found);
        }
        impl_->floatRecords.erase(paneId);
        impl_->pendingClosed.push_back(paneId);
    });
    // 保留窗口配置：后续改标题/图标时需要用它再 SetOptions
    record.options = options;
    impl_->floats[paneId] = std::move(frame);
    impl_->floatRecords[paneId] = std::move(record);
    if (impl_->paneFloated)
        impl_->paneFloated(paneId);
    return true;
}

bool DuiDockManager::IsPaneFloating(std::string_view id) const
{
    return impl_->floats.count(std::string(id)) > 0;
}

bool DuiDockManager::DockFloatingPane(std::string_view id)
{
    const auto found = impl_->floats.find(std::string(id));
    if (found == impl_->floats.end() || found->second == nullptr)
        return false;
    const std::string paneId{id};
    Impl::FloatRecord record;
    if (const auto recordFound = impl_->floatRecords.find(paneId);
        recordFound != impl_->floatRecords.end())
    {
        record = std::move(recordFound->second);
    }
    std::unique_ptr<ui::IFrameHost> frame = std::move(found->second);
    impl_->floats.erase(found);
    impl_->floatRecords.erase(paneId);
    // 清掉回调：收回不是"关闭窗格"，不该触发 SetPaneClosedHandler；
    // 也不能再受理该窗口的标题栏按钮（它马上就要消失）
    frame->SetClosedHandler({});
    frame->SetCaptionButtonHandler({});
    std::unique_ptr<core::Control> content = frame->DetachContent();
    // 只请求异步关窗，绝不在此销毁窗体对象：本函数可能正运行在该窗体自己的消息回调内，
    // 销毁它会释放正在执行的回调闭包。对象登记到 retiredFrames，由安全点释放。
    frame->RequestClose();
    impl_->retiredFrames.push_back(std::move(frame));
    if (content == nullptr)
        return false; // 后端不支持摘出内容：窗格保持关闭状态

    // 回到原组；原组不存在时回中央组
    const bool restored = !record.anchorPaneId.empty()
        && impl_->tree.AddPaneBeside(record.anchorPaneId, paneId, record.title, DuiDockSlot::Center);
    if (!restored)
        impl_->tree.AddPane(paneId, record.title, DuiDockSlot::Center);
    impl_->detached.emplace(paneId, std::move(content));
    // Rebuild 会触到 Layout → FlushRebuild：此刻本窗体的对象尚未销毁，须临时禁止清扫
    const bool wasInsideCallback = impl_->insideFrameCallback;
    impl_->insideFrameCallback = true;
    impl_->Rebuild(*this);
    impl_->insideFrameCallback = wasInsideCallback;
    impl_->SetActivePaneId(paneId);
    return true;
}

bool DuiDockManager::SetPaneIcon(std::string_view id, std::shared_ptr<const render::DuiImage> icon)
{
    if (!impl_->tree.Contains(id))
        return false;
    const std::string paneId{id};
    if (icon)
        impl_->iconByPaneId[paneId] = std::move(icon);
    else
        impl_->iconByPaneId.erase(paneId);
    // 标签页图标
    Impl::GroupView* view = nullptr;
    const int index = impl_->TagIndexOf(id, view);
    if (index >= 0 && view != nullptr && view->page != nullptr)
        view->page->SetPageIcon(index, impl_->IconOf(paneId));
    // 悬浮窗标题栏图标
    const auto floating = impl_->floats.find(paneId);
    const auto record = impl_->floatRecords.find(paneId);
    if (floating != impl_->floats.end() && record != impl_->floatRecords.end()
        && floating->second != nullptr)
    {
        record->second.options.icon = impl_->IconOf(paneId);
        floating->second->SetOptions(record->second.options);
    }
    return true;
}

std::vector<std::string> DuiDockManager::FloatingPaneIds() const
{
    std::vector<std::string> ids;
    ids.reserve(impl_->floats.size());
    for (const auto& entry : impl_->floats)
        ids.push_back(entry.first);
    return ids;
}

bool DuiDockManager::SaveLayout(std::string& text) const { return impl_->tree.SaveLayout(text); }

bool DuiDockManager::LoadLayout(std::string_view text)
{
    if (!impl_->tree.LoadLayout(text))
        return false;
    impl_->Rebuild(*this);
    return true;
}

void DuiDockManager::SetPaneClosedHandler(std::function<void(std::string)> handler)
{
    impl_->paneClosed = std::move(handler);
}

void DuiDockManager::SetActivePaneChangedHandler(std::function<void(std::string)> handler)
{
    impl_->activePaneChanged = std::move(handler);
}

void DuiDockManager::SetPaneFloatedHandler(std::function<void(std::string)> handler)
{
    impl_->paneFloated = std::move(handler);
}

const DuiDockTree& DuiDockManager::Tree() const { return impl_->tree; }
bool DuiDockManager::Dragging() const { return impl_->dragging; }

std::string DuiDockManager::DropHint() const
{
    return impl_->dragging ? std::string(DropKindName(impl_->drop.kind)) : std::string{};
}

void DuiDockManager::FlushRebuild()
{
    if (impl_->pendingRebuild)
        impl_->Rebuild(*this);
    // 释放已关闭的悬浮窗体：必须离开其消息回调（否则会释放正在执行的回调闭包）
    if (!impl_->insideFrameCallback)
        impl_->retiredFrames.clear();
    if (impl_->pendingClosed.empty())
        return;
    // 先把待通知队列取出再回调，避免回调内再次关闭窗格导致迭代失效
    std::vector<std::string> closed;
    closed.swap(impl_->pendingClosed);
    if (impl_->paneClosed)
    {
        for (const std::string& paneId : closed)
            impl_->paneClosed(paneId);
    }
}

core::Control* DuiDockManager::HitTest(core::Point point)
{
    if (!EffectivelyVisible() || !Enabled() || !Bounds().Contains(point))
        return nullptr;
    // 标签条与悬停滑出面板由本控件接管：拖动停靠需要跨组连续收到指针事件
    if (impl_->HitTabStrip(point) != nullptr)
        return this;
    if (!impl_->peekPaneId.empty() && impl_->peekRect.Contains(point))
        return this;
    return core::Control::HitTest(point);
}

bool DuiDockManager::OnEvent(const core::Event& event)
{
    if (!Enabled())
        return false;
    switch (event.type)
    {
    case core::EventType::PointerDown:
    {
        // 悬停滑出面板内的交互转发给窗格内容（不改变焦点归属）
        if (!impl_->peekPaneId.empty() && impl_->peekRect.Contains(event.position))
        {
            const auto found = impl_->autoHideContents.find(impl_->peekPaneId);
            if (found != impl_->autoHideContents.end() && found->second != nullptr)
            {
                if (core::Control* target = found->second->HitTest(event.position))
                    target->OnEvent(event);
            }
            return true;
        }
        std::uint64_t nodeId{};
        list::DuiTab* header = impl_->HitTabStrip(event.position, &nodeId);
        if (header == nullptr)
        {
            const std::string hidden = impl_->HitStripItem(event.position);
            if (!hidden.empty())
            {
                // 窄条条目：按下即临时滑出，指针离开后自动收回
                impl_->peekPaneId = hidden;
                impl_->peekRect = {};
                Layout(Bounds());
                return true;
            }
            return false;
        }
        const int tagIndex = header->HitTestIndex(event.position);
        if (tagIndex < 0)
            return false;
        const auto view = impl_->groupByNode.find(nodeId);
        if (view == impl_->groupByNode.end() || tagIndex >= static_cast<int>(view->second.paneIds.size()))
            return false;
        impl_->dragPaneId = view->second.paneIds[static_cast<std::size_t>(tagIndex)];
        impl_->dragGroupNodeId = nodeId;
        impl_->dragHeader = header;
        impl_->dragStart = event.position;
        impl_->dragPoint = event.position;
        impl_->dragCandidate = true;
        impl_->dragging = false;
        impl_->drop = {};
        // 先让标签控件自己处理按下（选中、按下态），本控件随后接管指针
        (void)header->OnEvent(event);
        SetCaptured(true);
        return true;
    }
    case core::EventType::PointerMove:
    {
        if (impl_->dragging)
        {
            impl_->dragPoint = event.position;
            impl_->drop = impl_->ComputeDrop(event.position);
            return true;
        }
        if (impl_->dragCandidate)
        {
            if (impl_->dragHeader != nullptr)
                (void)impl_->dragHeader->OnEvent(event);
            const int dx = event.position.x - impl_->dragStart.x;
            const int dy = event.position.y - impl_->dragStart.y;
            if (dx * dx + dy * dy >= kDragThreshold * kDragThreshold)
            {
                impl_->dragging = true;
                impl_->dragPoint = event.position;
                impl_->drop = impl_->ComputeDrop(event.position);
            }
            return true;
        }
        // 未拖动：把指针事件转给标签控件，维持 hover 高亮
        bool handled{};
        if (list::DuiTab* header = impl_->HitTabStrip(event.position))
            handled = header->OnEvent(event);
        if (event.type == core::EventType::PointerWheel && handled)
            return true;
        // 悬停滑出：指针进入窄条条目则滑出，离开窄条与面板则收回
        const std::string hidden = impl_->HitStripItem(event.position);
        const bool overPeek = !impl_->peekPaneId.empty() && impl_->peekRect.Contains(event.position);
        bool peekChanged{};
        if (!overPeek && hidden != impl_->peekPaneId)
        {
            if (hidden.empty())
            {
                impl_->peekPaneId.clear();
                impl_->peekRect = {};
            }
            else
            {
                impl_->peekPaneId = hidden;
            }
            Layout(Bounds());
            peekChanged = true;
        }
        // 改动了自身状态的移动事件必须消费，避免上层再按原样处理
        return handled || peekChanged || !hidden.empty() || overPeek;
    }
    case core::EventType::PointerUp:
    {
        if (impl_->dragging)
        {
            const DropTarget target = impl_->drop.kind == DropKind::None
                ? impl_->ComputeDrop(event.position) : impl_->drop;
            const std::string draggedPane = impl_->dragPaneId;
            list::DuiTab* header = impl_->dragHeader;
            impl_->ResetDrag();
            SetCaptured(false);
            // 拖动期间标签控件收不到抬起事件，补一条取消事件复位其内部的按下态与捕获标记
            if (header != nullptr)
            {
                core::Event cancel;
                cancel.type = core::EventType::PointerCancel;
                (void)header->OnEvent(cancel);
            }
            if (target.kind == DropKind::Float)
            {
                (void)FloatPane(draggedPane);
            }
            else if (impl_->ApplyDrop(draggedPane, target))
            {
                impl_->Rebuild(*this);
            }
            return true;
        }
        if (impl_->dragCandidate)
        {
            list::DuiTab* header = impl_->dragHeader != nullptr ? impl_->dragHeader
                                                               : impl_->HitTabStrip(event.position);
            impl_->ResetDrag();
            SetCaptured(false);
            if (header != nullptr)
                (void)header->OnEvent(event);
            return true;
        }
        return false;
    }
    case core::EventType::PointerCancel:
        if (impl_->dragging || impl_->dragCandidate)
        {
            list::DuiTab* header = impl_->dragHeader;
            impl_->ResetDrag();
            SetCaptured(false);
            if (header != nullptr)
                (void)header->OnEvent(event);
            return true;
        }
        return false;
    default:
        return false;
    }
}

void DuiDockManager::Layout(core::Rect bounds)
{
    FlushRebuild();
    SetBounds(bounds);
    Impl* state = impl_.get();

    // 先按窄条占用扣除内容区，再摆放分割树
    core::Rect content = bounds;
    state->stripRects[0] = {};
    state->stripRects[1] = {};
    state->stripRects[2] = {};
    state->stripRects[3] = {};
    const auto reserve = [state, &content](DuiDockSlot slot)
    {
        if (!state->HasStrip(slot))
            return;
        const int strip = state->stripThickness;
        switch (slot)
        {
        case DuiDockSlot::Left:
            state->stripRects[0] = {content.left, content.top, content.left + strip, content.bottom};
            content.left += strip;
            break;
        case DuiDockSlot::Right:
            state->stripRects[1] = {content.right - strip, content.top, content.right, content.bottom};
            content.right -= strip;
            break;
        case DuiDockSlot::Top:
            state->stripRects[2] = {content.left, content.top, content.right, content.top + strip};
            content.top += strip;
            break;
        case DuiDockSlot::Bottom:
            state->stripRects[3] = {content.left, content.bottom - strip, content.right, content.bottom};
            content.bottom -= strip;
            break;
        default:
            break;
        }
    };
    for (const DuiDockSlot slot : kStripSlots)
        reserve(slot);
    state->contentRect = content;

    for (const auto& child : Children())
        child->Layout(content);

    // 滑出面板：贴在对应窄条内侧
    if (!state->peekPaneId.empty())
    {
        const auto found = std::find_if(state->tree.AutoHideItems().begin(), state->tree.AutoHideItems().end(),
            [state](const DuiDockAutoHideItem& item) { return item.id == state->peekPaneId; });
        if (found != state->tree.AutoHideItems().end())
        {
            state->peekRect = state->PeekRectOf(found->slot);
            const auto entry = state->autoHideContents.find(state->peekPaneId);
            if (entry != state->autoHideContents.end() && entry->second != nullptr)
            {
                entry->second->Layout({state->peekRect.left + 1, state->peekRect.top + 1,
                                       state->peekRect.right - 1, state->peekRect.bottom - 1});
            }
        }
    }
}

void DuiDockManager::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    // 标签回调可能挂起了重建，绘制前是安全点：此刻回调栈已退出。
    const_cast<DuiDockManager*>(this)->FlushRebuild();
    if (!EffectivelyVisible())
        return;
    const core::Rect clipped = core::Rect::Intersect(Bounds(), dirty);
    if (clipped.Empty())
        return;
    const core::DuiTheme& theme = Theme();
    canvas.FillRect(clipped, theme.Get(core::ThemeSlot::SurfaceBackground));
    render::PaintChildren(*this, canvas, dirty);

    Impl* state = impl_.get();
    // 空布局提示：整块区域没有任何窗格也没有窄条
    if (state->tree.Empty() && !state->HasStrip(DuiDockSlot::Left) && !state->HasStrip(DuiDockSlot::Right)
        && !state->HasStrip(DuiDockSlot::Top) && !state->HasStrip(DuiDockSlot::Bottom))
    {
        render::DuiTextStyle hintStyle;
        hintStyle.color = theme.Get(core::ThemeSlot::TextSubtle);
        canvas.DrawText(kEmptyHint, clipped, hintStyle, render::DuiTextAlignment::Center, false);
    }

    // 自动隐藏窄条
    for (const DuiDockSlot slot : kStripSlots)
    {
        const int strip = StripIndex(slot);
        if (strip < 0)
            continue;
        const core::Rect stripRect = state->stripRects[strip];
        if (stripRect.Empty())
            continue;
        const core::Rect visibleStrip = core::Rect::Intersect(stripRect, dirty);
        if (visibleStrip.Empty())
            continue;
        canvas.FillRect(visibleStrip, theme.Get(core::ThemeSlot::PanelHeaderBackground));
        const std::vector<std::string> ids = state->HiddenOn(slot);
        const bool vertical = IsVerticalStrip(slot);
        for (int index = 0; index < static_cast<int>(ids.size()); ++index)
        {
            const core::Rect itemRect = state->StripItemRect(slot, index);
            if (core::Rect::Intersect(itemRect, dirty).Empty())
                continue;
            const bool hot = ids[static_cast<std::size_t>(index)] == state->peekPaneId;
            if (hot && slot == DuiDockSlot::Left)
                canvas.FillRect(core::Rect::Intersect(itemRect, dirty),
                                theme.Get(core::ThemeSlot::ControlHover));
            render::DuiTextStyle style;
            style.color = hot ? theme.Get(core::ThemeSlot::BrandPrimary)
                              : theme.Get(core::ThemeSlot::PanelText);
            PaintStripLabel(canvas, state->tree.PaneTitle(ids[static_cast<std::size_t>(index)]),
                            itemRect, vertical, style);
        }
        // 窄条与内容区之间留一条分隔线
        const core::Color border = theme.Get(core::ThemeSlot::GridBorder);
        if (slot == DuiDockSlot::Left)
            canvas.FillRect({stripRect.right - 1, stripRect.top, stripRect.right, stripRect.bottom}, border);
        else if (slot == DuiDockSlot::Right)
            canvas.FillRect({stripRect.left, stripRect.top, stripRect.left + 1, stripRect.bottom}, border);
        else if (slot == DuiDockSlot::Top)
            canvas.FillRect({stripRect.left, stripRect.bottom - 1, stripRect.right, stripRect.bottom}, border);
        else
            canvas.FillRect({stripRect.left, stripRect.top, stripRect.right, stripRect.top + 1}, border);
    }

    // 悬停滑出面板：自绘表面后绘制收起窗格的内容
    if (!state->peekPaneId.empty() && !state->peekRect.Empty())
    {
        const core::Rect panel = core::Rect::Intersect(state->peekRect, dirty);
        if (!panel.Empty())
        {
            canvas.FillRect(panel, theme.Get(core::ThemeSlot::SurfaceBackground));
            const auto content = state->autoHideContents.find(state->peekPaneId);
            if (content != state->autoHideContents.end() && content->second != nullptr)
            {
                canvas.PushClip(state->peekRect);
                if (auto* renderable = dynamic_cast<render::DuiRenderable*>(content->second.get()))
                    renderable->Paint(canvas, panel);
                canvas.PopClip();
            }
            canvas.StrokeRoundedRect(state->peekRect, 0, theme.Get(core::ThemeSlot::PopupBorder), 1.0F);
        }
    }

    // 拖动过程中的落点预览与幻影
    if (state->dragging)
    {
        // 十字停靠指示器：只在命中目标组时出现
        if (!state->guideRect.Empty())
        {
            const core::Color border = theme.Get(core::ThemeSlot::GridBorder);
            const core::Color hover = theme.Get(core::ThemeSlot::BrandPrimary);
            const core::Color surface = theme.Get(core::ThemeSlot::SurfaceBackground);
            const core::Color glyph = theme.Get(core::ThemeSlot::PanelGlyph);
            for (const DropKind kind : {DropKind::Top, DropKind::Left, DropKind::Merge,
                                        DropKind::Right, DropKind::Bottom})
            {
                const core::Rect cell = state->GuideCellRect(kind);
                if (cell.Empty())
                    continue;
                const core::Rect cellClip = core::Rect::Intersect(cell, dirty);
                if (cellClip.Empty())
                    continue;
                const bool hot = state->drop.kind == kind;
                canvas.FillRect(cellClip, hot ? hover : surface);
                canvas.StrokeRoundedRect(cell, 0, hot ? hover : border, 1.0F);
                PaintGuideGlyph(canvas, cell, kind, hot ? surface : glyph);
            }
        }
        if (!state->drop.preview.Empty())
        {
            const core::Rect preview = core::Rect::Intersect(state->drop.preview, dirty);
            if (!preview.Empty())
            {
                core::Color fill = theme.Get(core::ThemeSlot::BrandPrimary);
                fill.alpha = 60;
                canvas.FillRect(preview, fill);
                canvas.StrokeRoundedRect(state->drop.preview, 0,
                                         theme.Get(core::ThemeSlot::BrandPrimary), 1.0F);
            }
        }
        const core::Rect ghost{state->dragPoint.x + 8, state->dragPoint.y + 8,
                               state->dragPoint.x + 8 + kGhostSize.width,
                               state->dragPoint.y + 8 + kGhostSize.height};
        core::Color ghostFill = theme.Get(core::ThemeSlot::BrandPrimary);
        ghostFill.alpha = 180;
        canvas.FillRoundedRect(ghost, 4, ghostFill);
        const render::DuiTextStyle ghostText{theme.Get(core::ThemeSlot::TextOnPrimary), {}, 9, false};
        canvas.DrawText(state->tree.PaneTitle(state->dragPaneId), ghost, ghostText,
                        render::DuiTextAlignment::Center, false);
    }
}

core::DuiAccessibilityData DuiDockManager::CreateAccessibilityData() const
{
    const std::string active = ActivePane();
    return {core::DuiAccessibilityRole::Pane, "Dock", active, {}, false, {}};
}

} // namespace ysDui::controls::docking
