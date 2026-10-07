#include "ysDui/controls/graph/DuiNodeGraph.hpp"

#include <algorithm>
#include <utility>

namespace ysDui::controls::graph {
namespace {

template <typename T>
T* FindByIndex(std::vector<T>& items, const std::unordered_map<std::uint64_t, std::size_t>& index,
               std::uint64_t id)
{
    const auto found = index.find(id);
    if (found == index.end() || found->second >= items.size())
        return nullptr;
    return &items[found->second];
}

template <typename T>
const T* FindByIndex(const std::vector<T>& items,
                     const std::unordered_map<std::uint64_t, std::size_t>& index, std::uint64_t id)
{
    const auto found = index.find(id);
    if (found == index.end() || found->second >= items.size())
        return nullptr;
    return &items[found->second];
}

} // namespace

void DuiNodeGraph::rebuildIndices()
{
    nodeIndex_.clear();
    pinIndex_.clear();
    linkIndex_.clear();
    nodeIndex_.reserve(nodes_.size());
    pinIndex_.reserve(pins_.size());
    linkIndex_.reserve(links_.size());
    for (std::size_t index = 0; index < nodes_.size(); ++index)
        nodeIndex_[nodes_[index].id.value] = index;
    for (std::size_t index = 0; index < pins_.size(); ++index)
        pinIndex_[pins_[index].id.value] = index;
    for (std::size_t index = 0; index < links_.size(); ++index)
        linkIndex_[links_[index].id.value] = index;
}

DuiNodeId DuiNodeGraph::AddNode(std::string title, core::Point position)
{
    DuiGraphNode node;
    node.id = DuiNodeId{nextId_++};
    node.kind = DuiNodeKind::Normal;
    node.title = std::move(title);
    node.position = position;
    nodes_.push_back(std::move(node));
    nodeIndex_[nodes_.back().id.value] = nodes_.size() - 1;
    return nodes_.back().id;
}

DuiNodeId DuiNodeGraph::AddGroup(std::string title, core::Rect bounds)
{
    DuiGraphNode node;
    node.id = DuiNodeId{nextId_++};
    node.kind = DuiNodeKind::Group;
    node.title = std::move(title);
    node.position = {bounds.left, bounds.top};
    node.size = {bounds.Width(), bounds.Height()};
    nodes_.push_back(std::move(node));
    nodeIndex_[nodes_.back().id.value] = nodes_.size() - 1;
    return nodes_.back().id;
}

DuiNodeId DuiNodeGraph::AddComment(std::string title, core::Rect bounds)
{
    DuiGraphNode node;
    node.id = DuiNodeId{nextId_++};
    node.kind = DuiNodeKind::Comment;
    node.title = std::move(title);
    node.position = {bounds.left, bounds.top};
    node.size = {(std::max)(1, bounds.Width()), (std::max)(1, bounds.Height())};
    nodes_.push_back(std::move(node));
    nodeIndex_[nodes_.back().id.value] = nodes_.size() - 1;
    return nodes_.back().id;
}

DuiPinId DuiNodeGraph::AddPin(DuiNodeId node, std::string name, DuiPinKind kind,
                              bool multipleConnections, std::string typeId)
{
    const DuiGraphNode* entry = FindNode(node);
    if (entry == nullptr || entry->kind != DuiNodeKind::Normal)
        return {};
    DuiGraphPin pin;
    pin.id = DuiPinId{nextId_++};
    pin.node = node;
    pin.kind = kind;
    pin.name = std::move(name);
    pin.typeId = std::move(typeId);
    pin.multipleConnections = multipleConnections;
    pins_.push_back(std::move(pin));
    pinIndex_[pins_.back().id.value] = pins_.size() - 1;
    return pins_.back().id;
}

bool DuiNodeGraph::CanLink(DuiPinId start, DuiPinId end) const
{
    if (!start.Valid() || !end.Valid() || start == end)
        return false;
    const DuiGraphPin* startPin = FindPin(start);
    const DuiGraphPin* endPin = FindPin(end);
    if (startPin == nullptr || endPin == nullptr)
        return false;
    // 必须从输出指向输入
    if (startPin->kind != DuiPinKind::Output || endPin->kind != DuiPinKind::Input)
        return false;
    // 同一节点内部的引脚互不相连（避免自环；跨节点才构成数据流）
    if (startPin->node == endPin->node)
        return false;
    // 两端都声明类型时必须一致；空类型视为通配
    if (!startPin->typeId.empty() && !endPin->typeId.empty() && startPin->typeId != endPin->typeId)
        return false;
    for (const auto& link : links_)
    {
        if (link.start == start && link.end == end)
            return false;
    }
    return true;
}

DuiLinkId DuiNodeGraph::AddLink(DuiPinId start, DuiPinId end)
{
    if (!CanLink(start, end))
        return {};

    // 单连接引脚：先断开其已有连线，形成"新连接顶掉旧连接"
    const auto dropExisting = [this](DuiPinId pin)
    {
        const DuiGraphPin* target = FindPin(pin);
        if (target == nullptr || target->multipleConnections)
            return;
        std::erase_if(links_, [pin](const DuiGraphLink& link)
        {
            return link.start == pin || link.end == pin;
        });
    };
    dropExisting(start);
    dropExisting(end);
    rebuildIndices();
    // 顶替后目标可能已不存在（例如被自己刚才的连接影响），再校验一次
    if (!CanLink(start, end))
        return {};

    DuiGraphLink link;
    link.id = DuiLinkId{nextId_++};
    link.start = start;
    link.end = end;
    links_.push_back(link);
    linkIndex_[links_.back().id.value] = links_.size() - 1;
    return link.id;
}

bool DuiNodeGraph::RemoveNode(DuiNodeId node)
{
    if (FindNode(node) == nullptr)
        return false;
    // 先收集该节点的引脚，再删连线与引脚，最后删节点，避免悬空引用
    std::vector<DuiPinId> pins = PinsOf(node);
    for (const DuiPinId pin : pins)
        RemovePin(pin);
    std::erase_if(nodes_, [node](const DuiGraphNode& candidate) { return candidate.id == node; });
    rebuildIndices();
    return true;
}

bool DuiNodeGraph::RemovePin(DuiPinId pin)
{
    if (FindPin(pin) == nullptr)
        return false;
    std::erase_if(links_, [pin](const DuiGraphLink& link)
    {
        return link.start == pin || link.end == pin;
    });
    std::erase_if(pins_, [pin](const DuiGraphPin& candidate) { return candidate.id == pin; });
    rebuildIndices();
    return true;
}

bool DuiNodeGraph::RemoveLink(DuiLinkId link)
{
    const std::size_t before = links_.size();
    std::erase_if(links_, [link](const DuiGraphLink& candidate) { return candidate.id == link; });
    if (links_.size() == before)
        return false;
    rebuildIndices();
    return true;
}

int DuiNodeGraph::BreakLinksOfNode(DuiNodeId node)
{
    if (FindNode(node) == nullptr)
        return 0;
    const std::size_t before = links_.size();
    std::erase_if(links_, [this, node](const DuiGraphLink& link)
    {
        const DuiGraphPin* startPin = FindPin(link.start);
        const DuiGraphPin* endPin = FindPin(link.end);
        return (startPin != nullptr && startPin->node == node)
            || (endPin != nullptr && endPin->node == node);
    });
    if (links_.size() != before)
        rebuildIndices();
    return static_cast<int>(before - links_.size());
}

void DuiNodeGraph::Clear()
{
    nodes_.clear();
    pins_.clear();
    links_.clear();
    nodeIndex_.clear();
    pinIndex_.clear();
    linkIndex_.clear();
}

const DuiGraphNode* DuiNodeGraph::FindNode(DuiNodeId node) const
{
    return FindByIndex(nodes_, nodeIndex_, node.value);
}

DuiGraphNode* DuiNodeGraph::FindNode(DuiNodeId node)
{
    return FindByIndex(nodes_, nodeIndex_, node.value);
}

const DuiGraphPin* DuiNodeGraph::FindPin(DuiPinId pin) const
{
    return FindByIndex(pins_, pinIndex_, pin.value);
}

DuiGraphPin* DuiNodeGraph::FindPin(DuiPinId pin)
{
    return FindByIndex(pins_, pinIndex_, pin.value);
}

const DuiGraphLink* DuiNodeGraph::FindLink(DuiLinkId link) const
{
    return FindByIndex(links_, linkIndex_, link.value);
}

std::vector<DuiPinId> DuiNodeGraph::PinsOf(DuiNodeId node) const
{
    std::vector<DuiPinId> result;
    for (const auto& pin : pins_)
    {
        if (pin.node == node)
            result.push_back(pin.id);
    }
    return result;
}

std::vector<DuiLinkId> DuiNodeGraph::LinksOfPin(DuiPinId pin) const
{
    std::vector<DuiLinkId> result;
    for (const auto& link : links_)
    {
        if (link.start == pin || link.end == pin)
            result.push_back(link.id);
    }
    return result;
}

std::vector<DuiLinkId> DuiNodeGraph::LinksOfNode(DuiNodeId node) const
{
    std::vector<DuiLinkId> result;
    for (const auto& link : links_)
    {
        const DuiGraphPin* startPin = FindPin(link.start);
        const DuiGraphPin* endPin = FindPin(link.end);
        if ((startPin != nullptr && startPin->node == node)
            || (endPin != nullptr && endPin->node == node))
        {
            result.push_back(link.id);
        }
    }
    return result;
}

DuiPinId DuiNodeGraph::ConnectedPin(DuiPinId pin) const
{
    for (const auto& link : links_)
    {
        if (link.start == pin)
            return link.end;
        if (link.end == pin)
            return link.start;
    }
    return {};
}

bool DuiNodeGraph::RestoreNode(const DuiGraphNode& node)
{
    if (!node.id.Valid() || FindNode(node.id) != nullptr)
        return false;
    nodes_.push_back(node);
    nodeIndex_[nodes_.back().id.value] = nodes_.size() - 1;
    // 计数器必须跳过已恢复的 id，避免后续新增撞号
    nextId_ = (std::max)(nextId_, node.id.value + 1);
    return true;
}

bool DuiNodeGraph::RestorePin(const DuiGraphPin& pin)
{
    if (!pin.id.Valid() || FindPin(pin.id) != nullptr || FindNode(pin.node) == nullptr)
        return false;
    pins_.push_back(pin);
    pinIndex_[pins_.back().id.value] = pins_.size() - 1;
    nextId_ = (std::max)(nextId_, pin.id.value + 1);
    return true;
}

bool DuiNodeGraph::RestoreLink(const DuiGraphLink& link)
{
    if (!link.id.Valid() || FindLink(link.id) != nullptr)
        return false;
    // 引用完整性：两端必须已存在且方向合法
    if (!CanLink(link.start, link.end))
        return false;
    links_.push_back(link);
    linkIndex_[links_.back().id.value] = links_.size() - 1;
    nextId_ = (std::max)(nextId_, link.id.value + 1);
    return true;
}

bool DuiNodeGraph::SetNodePosition(DuiNodeId node, core::Point position)
{
    DuiGraphNode* target = FindNode(node);
    if (target == nullptr)
        return false;
    target->position = position;
    return true;
}

bool DuiNodeGraph::SetGroupSize(DuiNodeId group, core::Size size)
{
    DuiGraphNode* target = FindNode(group);
    if (target == nullptr
        || (target->kind != DuiNodeKind::Group && target->kind != DuiNodeKind::Comment))
        return false;
    // 至少保留可点中的最小矩形，避免被拖成负数
    target->size = {(std::max)(1, size.width), (std::max)(1, size.height)};
    return true;
}

bool DuiNodeGraph::belongsByGeometry(const DuiGraphNode& node, const DuiGraphNode& group) const
{
    if (node.kind != DuiNodeKind::Normal || group.kind != DuiNodeKind::Group)
        return false;
    const core::Rect groupBounds{group.position.x, group.position.y,
                                 group.position.x + group.size.width,
                                 group.position.y + group.size.height};
    return groupBounds.Contains(node.position);
}

bool DuiNodeGraph::MoveGroup(DuiNodeId group, core::Point delta)
{
    DuiGraphNode* target = FindNode(group);
    if (target == nullptr || target->kind != DuiNodeKind::Group)
        return false;
    // 先收集成员再平移分组，避免大位移后几何判定把成员漏掉
    std::vector<DuiGraphNode*> members;
    for (auto& node : nodes_)
    {
        if (node.kind != DuiNodeKind::Normal)
            continue;
        if (node.group == group || (!node.group.Valid() && belongsByGeometry(node, *target)))
            members.push_back(&node);
    }
    target->position.x += delta.x;
    target->position.y += delta.y;
    for (DuiGraphNode* node : members)
    {
        node->position.x += delta.x;
        node->position.y += delta.y;
        node->group = group;
    }
    return true;
}

bool DuiNodeGraph::NodeBelongsTo(DuiNodeId node, DuiNodeId group) const
{
    const DuiGraphNode* nodeEntry = FindNode(node);
    const DuiGraphNode* groupEntry = FindNode(group);
    if (nodeEntry == nullptr || groupEntry == nullptr)
        return false;
    if (nodeEntry->group.Valid())
        return nodeEntry->group == group;
    return belongsByGeometry(*nodeEntry, *groupEntry);
}

void DuiNodeGraph::RefreshGroupMembership()
{
    for (auto& node : nodes_)
    {
        if (node.kind != DuiNodeKind::Normal)
        {
            node.group = {};
            continue;
        }
        node.group = {};
        for (const auto& candidate : nodes_)
        {
            if (candidate.kind == DuiNodeKind::Group && belongsByGeometry(node, candidate))
                node.group = candidate.id;
        }
    }
}

core::Rect DuiNodeGraph::ContentBounds() const
{
    core::Rect result{};
    bool first = true;
    for (const auto& node : nodes_)
    {
        const core::Rect bounds{node.position.x, node.position.y,
                                node.position.x + node.size.width,
                                node.position.y + node.size.height};
        if (first)
        {
            result = bounds;
            first = false;
            continue;
        }
        result.left = (std::min)(result.left, bounds.left);
        result.top = (std::min)(result.top, bounds.top);
        result.right = (std::max)(result.right, bounds.right);
        result.bottom = (std::max)(result.bottom, bounds.bottom);
    }
    return first ? core::Rect{} : result;
}

} // namespace ysDui::controls::graph
