/**
 * 文件名：DuiDockTree.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：停靠布局模型的树操作、不变式整理与 XML 存取实现。
 */
#include "ysDui/controls/docking/DuiDockTree.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

#include "pugixml.hpp"

namespace ysDui::controls::docking {
namespace {
/** 切分比例下限：保证两侧都留出可点击区域。 */
constexpr double kMinFraction = 0.05;
/** 切分比例上限。 */
constexpr double kMaxFraction = 0.95;
/** 新建左右栏时该栏占锚点的比例。 */
constexpr double kSideFraction = 0.25;
/** 新建上下栏时该栏占整棵树的比例。 */
constexpr double kBarFraction = 0.28;
/** 新建中央内容组时左侧锚点保留的比例。 */
constexpr double kCenterAnchorFraction = 0.3;
/** 分割节点恒定的子节点数量。 */
constexpr int kSplitChildCount = 2;

/** 槽位下标，与 `DuiDockSlot` 顺序一致。 */
constexpr int kSlotCenterIndex = 4;

/** @return 槽位在布局文件中的名字。 */
const char* SlotName(DuiDockSlot slot)
{
    switch (slot)
    {
    case DuiDockSlot::Left: return "left";
    case DuiDockSlot::Right: return "right";
    case DuiDockSlot::Top: return "top";
    case DuiDockSlot::Bottom: return "bottom";
    case DuiDockSlot::Center: return "center";
    }
    return "left";
}

/** @return 由布局文件中的名字解析槽位；不认识时退回左侧。 */
DuiDockSlot SlotFromName(std::string_view name)
{
    if (name == "right") return DuiDockSlot::Right;
    if (name == "top") return DuiDockSlot::Top;
    if (name == "bottom") return DuiDockSlot::Bottom;
    if (name == "center") return DuiDockSlot::Center;
    return DuiDockSlot::Left;
}

/** @return 该槽位是否沿水平方向切分整棵树。 */
bool IsBarSlot(DuiDockSlot slot)
{
    return slot == DuiDockSlot::Top || slot == DuiDockSlot::Bottom;
}

/** @return 新组是否位于第一个子节点位置。 */
bool IsFirstSlot(DuiDockSlot slot)
{
    return slot == DuiDockSlot::Left || slot == DuiDockSlot::Top;
}

/** @return 该槽位新建组时的默认切分比例（第一个子节点占比）。 */
double DefaultFraction(DuiDockSlot slot)
{
    switch (slot)
    {
    case DuiDockSlot::Left: return kSideFraction;
    case DuiDockSlot::Right: return 1.0 - kSideFraction;
    case DuiDockSlot::Top: return kBarFraction;
    case DuiDockSlot::Bottom: return 1.0 - kBarFraction;
    case DuiDockSlot::Center: return kCenterAnchorFraction;
    }
    return 0.5;
}

/** @return 该槽位对应的分割方向。 */
DuiDockOrientation OrientationOf(DuiDockSlot slot)
{
    return IsBarSlot(slot) ? DuiDockOrientation::Horizontal : DuiDockOrientation::Vertical;
}

/** 递归查找标识匹配的节点。 */
DuiDockNode* FindNode(DuiDockNode& node, std::uint64_t id)
{
    if (node.id == id)
        return &node;
    for (const auto& child : node.children)
    {
        if (DuiDockNode* found = FindNode(*child, id))
            return found;
    }
    return nullptr;
}

/** 递归查找持有指定标识节点的 owning 指针，便于就地替换。 */
std::unique_ptr<DuiDockNode>* FindHolder(std::unique_ptr<DuiDockNode>& holder, std::uint64_t id)
{
    if (!holder)
        return nullptr;
    if (holder->id == id)
        return &holder;
    for (auto& child : holder->children)
    {
        if (std::unique_ptr<DuiDockNode>* found = FindHolder(child, id))
            return found;
    }
    return nullptr;
}

/** 递归查找容纳指定窗格的组。 */
DuiDockNode* GroupOfPane(DuiDockNode& node, std::string_view paneId)
{
    if (node.kind == DuiDockNodeKind::Group)
    {
        for (const DuiDockPane& pane : node.panes)
        {
            if (pane.id == paneId)
                return &node;
        }
        return nullptr;
    }
    for (const auto& child : node.children)
    {
        if (DuiDockNode* found = GroupOfPane(*child, paneId))
            return found;
    }
    return nullptr;
}

/** 深度优先收集全部窗格。 */
void CollectPanes(const DuiDockNode& node, std::vector<DuiDockPane>& panes)
{
    if (node.kind == DuiDockNodeKind::Group)
    {
        panes.insert(panes.end(), node.panes.begin(), node.panes.end());
        return;
    }
    for (const auto& child : node.children)
        CollectPanes(*child, panes);
}

/** 深度优先查找第一个非空组。 */
const DuiDockNode* FirstGroupWithPanes(const DuiDockNode& node)
{
    if (node.kind == DuiDockNodeKind::Group)
        return node.panes.empty() ? nullptr : &node;
    for (const auto& child : node.children)
    {
        if (const DuiDockNode* found = FirstGroupWithPanes(*child))
            return found;
    }
    return nullptr;
}

/** 深度优先查找第一个组（可空）。 */
DuiDockNode* FirstGroup(DuiDockNode& node)
{
    if (node.kind == DuiDockNodeKind::Group)
        return &node;
    for (auto& child : node.children)
    {
        if (DuiDockNode* found = FirstGroup(*child))
            return found;
    }
    return nullptr;
}

/**
 * 整理子树并报告其是否含有窗格。
 * 空组被回收；只有一个有内容子节点的分割被折叠、幸存子节点上提并保留自身标识。
 * @return 子树含有窗格时返回 true
 */
bool NormalizeHolder(std::unique_ptr<DuiDockNode>& holder)
{
    if (!holder)
        return false;
    if (holder->kind == DuiDockNodeKind::Group)
    {
        if (holder->panes.empty())
            return false;
        holder->active = (std::clamp)(holder->active, 0, static_cast<int>(holder->panes.size()) - 1);
        return true;
    }
    if (holder->children.size() != kSplitChildCount)
        return false;
    const bool firstHasPanes = NormalizeHolder(holder->children[0]);
    const bool secondHasPanes = NormalizeHolder(holder->children[1]);
    if (firstHasPanes && secondHasPanes)
    {
        holder->fraction = (std::clamp)(holder->fraction, kMinFraction, kMaxFraction);
        return true;
    }
    if (!firstHasPanes && !secondHasPanes)
        return false;
    // 先用临时量取出幸存子节点，再替换 holder，避免读取已释放的分割节点
    std::unique_ptr<DuiDockNode> survivor = std::move(holder->children[firstHasPanes ? 0 : 1]);
    holder = std::move(survivor);
    return true;
}

/** 递归为新载入的节点分配标识（布局文件不保存标识）。 */
void AssignMissingIds(DuiDockNode& node, std::uint64_t& nextId)
{
    if (node.id == 0)
        node.id = nextId++;
    for (auto& child : node.children)
        AssignMissingIds(*child, nextId);
}

/** 递归剔除当前不存在的窗格引用，并标记被认领的窗格。 */
void FilterPanes(DuiDockNode& node, const std::vector<DuiDockPane>& known, std::vector<bool>& claimed)
{
    if (node.kind == DuiDockNodeKind::Group)
    {
        std::vector<DuiDockPane> kept;
        for (const DuiDockPane& pane : node.panes)
        {
            const auto found = std::find_if(known.begin(), known.end(),
                [&pane](const DuiDockPane& candidate) { return candidate.id == pane.id; });
            if (found == known.end())
                continue;
            kept.push_back(*found);
            claimed[static_cast<std::size_t>(found - known.begin())] = true;
        }
        node.panes = std::move(kept);
        return;
    }
    for (auto& child : node.children)
        FilterPanes(*child, known, claimed);
}
} // namespace

class DuiDockTree::Impl
{
public:
    std::unique_ptr<DuiDockNode> root;
    std::vector<DuiDockAutoHideItem> autoHidden; // 已收起到边缘窄条的窗格
    std::uint64_t nextNodeId{1};
    std::uint64_t slotGroupIds[5]{}; // 索引顺序同 DuiDockSlot

    Impl() { root = MakeGroup(); }

    /** @return 新建一个空标签组。 */
    std::unique_ptr<DuiDockNode> MakeGroup()
    {
        auto node = std::make_unique<DuiDockNode>();
        node->id = nextNodeId++;
        node->kind = DuiDockNodeKind::Group;
        return node;
    }

    /** @return 新建一个二分子树。 */
    std::unique_ptr<DuiDockNode> MakeSplit(DuiDockOrientation orientation, double fraction)
    {
        auto node = std::make_unique<DuiDockNode>();
        node->id = nextNodeId++;
        node->kind = DuiDockNodeKind::Split;
        node->orientation = orientation;
        node->fraction = (std::clamp)(fraction, kMinFraction, kMaxFraction);
        return node;
    }

    std::uint64_t& SlotId(DuiDockSlot slot) { return slotGroupIds[static_cast<int>(slot)]; }
    std::uint64_t SlotId(DuiDockSlot slot) const { return slotGroupIds[static_cast<int>(slot)]; }

    /** @return 自动隐藏条目标识列表。 */
    std::vector<std::string> AutoHideIds() const
    {
        std::vector<std::string> ids;
        ids.reserve(autoHidden.size());
        for (const DuiDockAutoHideItem& item : autoHidden)
            ids.push_back(item.id);
        return ids;
    }

    /** @return 指向自动隐藏条目的指针；未收起时为空。 */
    DuiDockAutoHideItem* FindAutoHide(std::string_view id)
    {
        const auto found = std::find_if(autoHidden.begin(), autoHidden.end(),
            [id](const DuiDockAutoHideItem& item) { return item.id == id; });
        return found == autoHidden.end() ? nullptr : &*found;
    }

    /** @return 左右栏的锚点：中央组存在时为其标识，否则为整棵树。 */
    std::uint64_t AnchorId() const
    {
        return slotGroupIds[kSlotCenterIndex] != 0 ? slotGroupIds[kSlotCenterIndex] : root->id;
    }

    /** 清理指向已消失节点的槽位记录。 */
    void DropStaleSlots()
    {
        for (std::uint64_t& slotId : slotGroupIds)
        {
            if (slotId != 0 && FindNode(*root, slotId) == nullptr)
                slotId = 0;
        }
    }

    /** 整理不变式并清理槽位记录。 */
    void Normalize()
    {
        if (!NormalizeHolder(root))
        {
            // 整棵树已无窗格：退回一个空组，保持根始终是有效节点
            root = MakeGroup();
            nextNodeId = std::max(nextNodeId, root->id + 1);
        }
        DropStaleSlots();
    }

    /** 把节点包裹进新的分割，按槽位决定新组位置。 */
    bool WrapNode(std::uint64_t targetId, std::unique_ptr<DuiDockNode> group, DuiDockSlot slot,
                  double fraction)
    {
        std::unique_ptr<DuiDockNode>* holder = FindHolder(root, targetId);
        if (holder == nullptr || *holder == nullptr)
            return false;
        auto split = MakeSplit(OrientationOf(slot), fraction);
        split->children.push_back(std::move(*holder));
        // 第一个子节点应是既有内容还是新组，取决于槽位方向
        if (IsFirstSlot(slot))
            split->children.insert(split->children.begin(), std::move(group));
        else
            split->children.push_back(std::move(group));
        if (split->children.size() != kSplitChildCount)
            return false;
        *holder = std::move(split);
        return true;
    }

    /** 在参照组旁切出新组并放入窗格。 */
    bool SplitBeside(DuiDockNode& anchorGroup, DuiDockPane pane, DuiDockSlot slot, double fraction)
    {
        if (slot == DuiDockSlot::Center)
        {
            anchorGroup.panes.push_back(std::move(pane));
            anchorGroup.active = static_cast<int>(anchorGroup.panes.size()) - 1;
            return true;
        }
        auto group = MakeGroup();
        const std::uint64_t groupId = group->id;
        group->panes.push_back(std::move(pane));
        if (!WrapNode(anchorGroup.id, std::move(group), slot, fraction))
            return false;
        // 槽位尚无归属时登记新组，供后续同槽位窗格直接并入
        if (SlotId(slot) == 0 || FindNode(*root, SlotId(slot)) == nullptr)
            SlotId(slot) = groupId;
        return true;
    }
};

DuiDockTree::DuiDockTree() : impl_(std::make_unique<Impl>()) {}
DuiDockTree::~DuiDockTree() = default;
DuiDockTree::DuiDockTree(DuiDockTree&&) noexcept = default;
DuiDockTree& DuiDockTree::operator=(DuiDockTree&&) noexcept = default;

bool DuiDockTree::AddPane(std::string id, std::string title, DuiDockSlot slot)
{
    if (id.empty() || Contains(id))
        return false;
    DuiDockPane pane{id, std::move(title)};
    // 空树：任何槽位的第一个窗格都占据整块区域
    if (impl_->root->kind == DuiDockNodeKind::Group && impl_->root->panes.empty())
    {
        impl_->root->panes.push_back(std::move(pane));
        impl_->root->active = 0;
        impl_->SlotId(slot) = impl_->root->id;
        return true;
    }
    const std::uint64_t slotId = impl_->SlotId(slot);
    if (slotId != 0)
    {
        if (DuiDockNode* group = FindNode(*impl_->root, slotId))
        {
            group->panes.push_back(std::move(pane));
            group->active = static_cast<int>(group->panes.size()) - 1;
            return true;
        }
    }
    // 新建槽位组：上下栏切分整棵树，其余切分锚点
    const std::uint64_t targetId = IsBarSlot(slot) ? impl_->root->id : impl_->AnchorId();
    auto group = impl_->MakeGroup();
    const std::uint64_t groupId = group->id;
    group->panes.push_back(std::move(pane));
    if (!impl_->WrapNode(targetId, std::move(group), slot, DefaultFraction(slot)))
        return false;
    impl_->SlotId(slot) = groupId;
    return true;
}

bool DuiDockTree::AddPaneBeside(std::string_view anchorPaneId, std::string id, std::string title,
                                DuiDockSlot slot, double fraction)
{
    if (id.empty() || Contains(id))
        return false;
    DuiDockNode* anchor = GroupOfPane(*impl_->root, anchorPaneId);
    if (anchor == nullptr)
        return false;
    return impl_->SplitBeside(*anchor, DuiDockPane{id, std::move(title)}, slot, fraction);
}

bool DuiDockTree::RemovePane(std::string_view id)
{
    if (impl_->FindAutoHide(id) != nullptr)
    {
        impl_->autoHidden.erase(std::find_if(impl_->autoHidden.begin(), impl_->autoHidden.end(),
            [id](const DuiDockAutoHideItem& item) { return item.id == id; }));
        return true;
    }
    DuiDockNode* group = GroupOfPane(*impl_->root, id);
    if (group == nullptr)
        return false;
    const auto found = std::find_if(group->panes.begin(), group->panes.end(),
        [id](const DuiDockPane& pane) { return pane.id == id; });
    if (found == group->panes.end())
        return false;
    group->panes.erase(found);
    impl_->Normalize();
    return true;
}

bool DuiDockTree::MovePane(std::string_view id, std::string_view targetId, DuiDockSlot slot,
                           double fraction)
{
    if (id.empty() || id == targetId)
        return false;
    DuiDockNode* target = GroupOfPane(*impl_->root, targetId);
    if (target == nullptr)
        return false;
    DuiDockNode* source = GroupOfPane(*impl_->root, id);
    if (source == nullptr)
        return false;
    const auto found = std::find_if(source->panes.begin(), source->panes.end(),
        [id](const DuiDockPane& pane) { return pane.id == id; });
    if (found == source->panes.end())
        return false;
    const DuiDockPane pane = *found;
    source->panes.erase(found);
    if (slot == DuiDockSlot::Center)
    {
        // 目标组仍持有目标窗格，因此整理后指针依旧有效（节点对象由 owning 指针搬迁，地址不变）
        target->panes.push_back(pane);
        target->active = static_cast<int>(target->panes.size()) - 1;
    }
    else if (!impl_->SplitBeside(*target, pane, slot, fraction))
    {
        target->panes.push_back(pane);
        target->active = static_cast<int>(target->panes.size()) - 1;
    }
    impl_->Normalize();
    return true;
}

bool DuiDockTree::SetPaneTitle(std::string_view id, std::string title)
{
    DuiDockNode* group = GroupOfPane(*impl_->root, id);
    if (group == nullptr)
        return false;
    for (DuiDockPane& pane : group->panes)
    {
        if (pane.id == id)
        {
            pane.title = std::move(title);
            return true;
        }
    }
    return false;
}

const DuiDockNode& DuiDockTree::Root() const { return *impl_->root; }
bool DuiDockTree::Empty() const { return FirstGroupWithPanes(*impl_->root) == nullptr; }
bool DuiDockTree::ReorderPane(std::uint64_t groupNodeId, int from, int to)
{
    DuiDockNode* group = FindNode(*impl_->root, groupNodeId);
    if (group == nullptr || group->kind != DuiDockNodeKind::Group)
        return false;
    const int count = static_cast<int>(group->panes.size());
    if (from < 0 || from >= count)
        return false;
    to = (std::clamp)(to, 0, count - 1);
    if (from == to)
        return true;
    DuiDockPane pane = std::move(group->panes[static_cast<std::size_t>(from)]);
    group->panes.erase(group->panes.begin() + from);
    group->panes.insert(group->panes.begin() + to, std::move(pane));
    // 选中项跟随被移动的标签
    if (group->active == from)
        group->active = to;
    else if (from < group->active && group->active <= to)
        --group->active;
    else if (to <= group->active && group->active < from)
        ++group->active;
    return true;
}
bool DuiDockTree::Contains(std::string_view id) const
{
    return GroupOfPane(*impl_->root, id) != nullptr
        || std::any_of(impl_->autoHidden.begin(), impl_->autoHidden.end(),
                       [id](const DuiDockAutoHideItem& item) { return item.id == id; });
}

int DuiDockTree::PaneCount() const
{
    std::vector<DuiDockPane> panes;
    CollectPanes(*impl_->root, panes);
    return static_cast<int>(panes.size()) + static_cast<int>(impl_->autoHidden.size());
}

std::vector<std::string> DuiDockTree::PaneIds() const
{
    std::vector<DuiDockPane> panes;
    CollectPanes(*impl_->root, panes);
    std::vector<std::string> ids;
    ids.reserve(panes.size() + impl_->autoHidden.size());
    for (const DuiDockPane& pane : panes)
        ids.push_back(pane.id);
    for (const DuiDockAutoHideItem& item : impl_->autoHidden)
        ids.push_back(item.id);
    return ids;
}

std::string DuiDockTree::PaneTitle(std::string_view id) const
{
    if (const DuiDockAutoHideItem* hidden = impl_->FindAutoHide(id))
        return hidden->title;
    const DuiDockNode* group = GroupOfPane(*impl_->root, id);
    if (group == nullptr)
        return {};
    for (const DuiDockPane& pane : group->panes)
    {
        if (pane.id == id)
            return pane.title;
    }
    return {};
}

const std::vector<DuiDockAutoHideItem>& DuiDockTree::AutoHideItems() const
{
    return impl_->autoHidden;
}

bool DuiDockTree::IsPaneAutoHidden(std::string_view id) const
{
    return impl_->FindAutoHide(id) != nullptr;
}

bool DuiDockTree::SetPaneAutoHide(std::string_view id, bool autoHide, DuiDockSlot slot)
{
    if (autoHide)
    {
        DuiDockNode* group = GroupOfPane(*impl_->root, id);
        if (group == nullptr)
            return false;
        const auto found = std::find_if(group->panes.begin(), group->panes.end(),
            [id](const DuiDockPane& pane) { return pane.id == id; });
        if (found == group->panes.end())
            return false;
        const DuiDockPane pane = *found;
        group->panes.erase(found);
        impl_->autoHidden.push_back({pane.id, pane.title, slot});
        impl_->Normalize();
        return true;
    }
    DuiDockAutoHideItem* hidden = impl_->FindAutoHide(id);
    if (hidden == nullptr)
        return false;
    const DuiDockPane pane{hidden->id, hidden->title};
    impl_->autoHidden.erase(std::find_if(impl_->autoHidden.begin(), impl_->autoHidden.end(),
        [id](const DuiDockAutoHideItem& item) { return item.id == id; }));
    // 展开时并入中央组（原位置无法反推，收起到当前位置更易预期）
    return AddPane(pane.id, pane.title, DuiDockSlot::Center);
}

std::string DuiDockTree::ActivePane() const
{
    const std::uint64_t centerId = impl_->SlotId(DuiDockSlot::Center);
    if (centerId != 0)
    {
        const DuiDockNode* center = FindNode(*impl_->root, centerId);
        if (center != nullptr && !center->panes.empty())
            return center->panes[center->active].id;
    }
    if (const DuiDockNode* group = FirstGroupWithPanes(*impl_->root))
        return group->panes[group->active].id;
    return {};
}

const DuiDockNode* DuiDockTree::FindGroup(std::string_view paneId) const
{
    return GroupOfPane(*impl_->root, paneId);
}

bool DuiDockTree::SetActivePane(std::string_view paneId)
{
    DuiDockNode* group = GroupOfPane(*impl_->root, paneId);
    if (group == nullptr)
        return false;
    for (int index = 0; index < static_cast<int>(group->panes.size()); ++index)
    {
        if (group->panes[static_cast<std::size_t>(index)].id == paneId)
        {
            group->active = index;
            return true;
        }
    }
    return false;
}

bool DuiDockTree::SetGroupActive(std::uint64_t groupNodeId, int index)
{
    DuiDockNode* node = FindNode(*impl_->root, groupNodeId);
    if (node == nullptr || node->kind != DuiDockNodeKind::Group || node->panes.empty())
        return false;
    node->active = (std::clamp)(index, 0, static_cast<int>(node->panes.size()) - 1);
    return true;
}

bool DuiDockTree::SetSplitFraction(std::uint64_t splitNodeId, double fraction)
{
    DuiDockNode* node = FindNode(*impl_->root, splitNodeId);
    if (node == nullptr || node->kind != DuiDockNodeKind::Split)
        return false;
    node->fraction = (std::clamp)(fraction, kMinFraction, kMaxFraction);
    return true;
}

bool DuiDockTree::SaveLayout(std::string& text) const
{
    pugi::xml_document document;
    pugi::xml_node layout = document.append_child("dock-layout");
    layout.append_attribute("version").set_value(1);

    // 递归写出结构：分割记方向与比例，组记选中下标与窗格
    const auto writeNode = [](const auto& self, pugi::xml_node parent, const DuiDockNode& node) -> void
    {
        if (node.kind == DuiDockNodeKind::Group)
        {
            pugi::xml_node group = parent.append_child("group");
            group.append_attribute("active").set_value(node.active);
            for (const DuiDockPane& pane : node.panes)
            {
                pugi::xml_node child = group.append_child("pane");
                child.append_attribute("id").set_value(pane.id.c_str());
            }
            return;
        }
        pugi::xml_node split = parent.append_child("split");
        split.append_attribute("orientation").set_value(
            node.orientation == DuiDockOrientation::Vertical ? "vertical" : "horizontal");
        split.append_attribute("fraction").set_value(node.fraction);
        for (const auto& child : node.children)
            self(self, split, *child);
    };
    writeNode(writeNode, layout, *impl_->root);
    // 自动隐藏：记录贴附的边，展开时按登记的边恢复
    if (!impl_->autoHidden.empty())
    {
        pugi::xml_node hidden = layout.append_child("auto-hide");
        for (const DuiDockAutoHideItem& item : impl_->autoHidden)
        {
            pugi::xml_node child = hidden.append_child("pane");
            child.append_attribute("id").set_value(item.id.c_str());
            child.append_attribute("slot").set_value(SlotName(item.slot));
        }
    }

    std::ostringstream stream;
    document.save(stream, "  ", pugi::format_indent | pugi::format_no_declaration,
                  pugi::encoding_utf8);
    text = stream.str();
    return !text.empty();
}

bool DuiDockTree::LoadLayout(std::string_view text)
{
    pugi::xml_document document;
    const pugi::xml_parse_result result = document.load_buffer(text.data(), text.size(),
        pugi::parse_default, pugi::encoding_utf8);
    if (!result)
        return false;
    const pugi::xml_node layout = document.document_element();
    if (!layout || std::string(layout.name()) != "dock-layout")
        return false;
    // 布局正文是唯一的结构元素（group / split），另可有 auto-hide 兄弟元素
    pugi::xml_node content;
    pugi::xml_node autoHideNode;
    for (const pugi::xml_node child : layout.children())
    {
        if (child.type() != pugi::node_element)
            continue;
        const std::string name = child.name();
        if (name == "auto-hide")
        {
            if (autoHideNode)
                return false;
            autoHideNode = child;
            continue;
        }
        if (content)
            return false;
        content = child;
    }
    if (!content)
        return false;

    // 解析到临时树，成功后再替换，保证失败时不破坏现有布局
    const auto readNode = [](const auto& self, pugi::xml_node node)
        -> std::unique_ptr<DuiDockNode>
    {
        const std::string name = node.name();
        if (name == "group")
        {
            auto group = std::make_unique<DuiDockNode>();
            group->kind = DuiDockNodeKind::Group;
            group->active = node.attribute("active").as_int(0);
            for (const pugi::xml_node child : node.children("pane"))
            {
                DuiDockPane pane;
                pane.id = child.attribute("id").as_string();
                if (!pane.id.empty())
                    group->panes.push_back(std::move(pane));
            }
            return group;
        }
        if (name == "split")
        {
            auto split = std::make_unique<DuiDockNode>();
            split->kind = DuiDockNodeKind::Split;
            const std::string orientation = node.attribute("orientation").as_string("vertical");
            split->orientation = orientation == "horizontal"
                ? DuiDockOrientation::Horizontal : DuiDockOrientation::Vertical;
            split->fraction = node.attribute("fraction").as_double(0.5);
            for (const pugi::xml_node child : node.children())
            {
                if (child.type() != pugi::node_element)
                    continue;
                if (std::unique_ptr<DuiDockNode> parsed = self(self, child))
                    split->children.push_back(std::move(parsed));
            }
            if (split->children.size() != kSplitChildCount)
                return {};
            return split;
        }
        return {};
    };
    std::unique_ptr<DuiDockNode> parsed = readNode(readNode, content);
    if (!parsed)
        return false;

    // 布局只描述排布方式，窗格集合仍以当前树为准（含当前已收起的窗格）
    std::vector<DuiDockPane> known;
    CollectPanes(*impl_->root, known);
    for (const DuiDockAutoHideItem& item : impl_->autoHidden)
        known.push_back({item.id, item.title});
    std::vector<bool> claimed(known.size(), false);
    // 先认领布局里声明的自动隐藏窗格，避免它们被当成"未出现"补回树内
    std::vector<DuiDockAutoHideItem> parsedAutoHide;
    if (autoHideNode)
    {
        for (const pugi::xml_node child : autoHideNode.children("pane"))
        {
            const std::string id = child.attribute("id").as_string();
            if (id.empty())
                continue;
            const auto found = std::find_if(known.begin(), known.end(),
                [&id](const DuiDockPane& pane) { return pane.id == id; });
            if (found == known.end())
                continue; // 布局引用了当前不存在的窗格：忽略
            claimed[static_cast<std::size_t>(found - known.begin())] = true;
            parsedAutoHide.push_back(
                {found->id, found->title, SlotFromName(child.attribute("slot").as_string("left"))});
        }
    }
    FilterPanes(*parsed, known, claimed);
    if (!NormalizeHolder(parsed))
    {
        // 树内无窗格：仅当确有收起的窗格时保留空结构（此时界面只剩边缘窄条）
        if (parsedAutoHide.empty())
            return false;
        parsed = std::make_unique<DuiDockNode>();
        parsed->kind = DuiDockNodeKind::Group;
    }
    AssignMissingIds(*parsed, impl_->nextNodeId);
    // 布局中未出现的窗格补进第一个组，避免从界面上消失
    if (DuiDockNode* first = FirstGroup(*parsed))
    {
        for (std::size_t index = 0; index < known.size(); ++index)
        {
            if (!claimed[index])
                first->panes.push_back(known[index]);
        }
    }
    impl_->root = std::move(parsed);
    impl_->autoHidden = std::move(parsedAutoHide);
    impl_->nextNodeId = std::max(impl_->nextNodeId, impl_->root->id + 1);
    // 锚点重置：载入后的任意结构无法反推槽位归属，改为以活动窗格所在组为锚点
    for (std::uint64_t& slotId : impl_->slotGroupIds)
        slotId = 0;
    const std::string active = ActivePane();
    if (const DuiDockNode* group = GroupOfPane(*impl_->root, active))
        impl_->SlotId(DuiDockSlot::Center) = group->id;
    impl_->Normalize();
    return true;
}

} // namespace ysDui::controls::docking
