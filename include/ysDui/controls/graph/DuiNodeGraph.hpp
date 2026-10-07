/**
 * 文件名：DuiNodeGraph.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明蓝图节点编辑器的数据模型——节点、引脚与连线，以及连线的合法性校验。
 *
 * 说明：本类型是纯数据（值语义、可拷贝），不含任何绘制或平台依赖；
 *       编辑器持有的整图快照即用它实现撤销/重做与保存/加载。
 */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::controls::graph {

/**
 * 强类型标识符。
 * 用途：节点/引脚/连线各用独立的类型，编译期阻止把引脚 id 误传到节点参数。
 *       值为 0 表示无效。
 */
struct DuiNodeId final
{
    std::uint64_t value{};
    [[nodiscard]] constexpr bool Valid() const { return value != 0; }
    constexpr bool operator==(const DuiNodeId&) const = default;
};

struct DuiPinId final
{
    std::uint64_t value{};
    [[nodiscard]] constexpr bool Valid() const { return value != 0; }
    constexpr bool operator==(const DuiPinId&) const = default;
};

struct DuiLinkId final
{
    std::uint64_t value{};
    [[nodiscard]] constexpr bool Valid() const { return value != 0; }
    constexpr bool operator==(const DuiLinkId&) const = default;
};

/** 引脚方向。连线必须从 Output 到 Input。 */
enum class DuiPinKind
{
    Input,
    Output,
};

/**
 * 节点种类。
 * - Normal：普通业务节点，可挂引脚。
 * - Group：分组框，不承载引脚；按几何收纳普通节点。
 * - Comment：注释框，不承载引脚、不收纳节点；可拖动/缩放/改名。
 */
enum class DuiNodeKind
{
    Normal,
    Group,
    Comment,
};

/** 引脚。 */
struct DuiGraphPin final
{
    DuiPinId id{};
    DuiNodeId node{};
    DuiPinKind kind{DuiPinKind::Input};
    std::string name;
    /**
     * 类型标识。两端都非空且不相等时拒绝连线；任一方为空表示通配。
     */
    std::string typeId;
    /** 是否允许接入多条连线；false 时新连线会顶掉已有连线。 */
    bool multipleConnections{};
    /** 显式配色；hasColor 为 false 时由编辑器按引脚方向取主题配色。 */
    core::Color color{};
    bool hasColor{};
};

/** 节点。 */
struct DuiGraphNode final
{
    DuiNodeId id{};
    DuiNodeKind kind{DuiNodeKind::Normal};
    std::string title;
    /** 画布坐标（左上角）。 */
    core::Point position{};
    /**
     * 画布坐标下的节点尺寸。
     * 由编辑器的布局阶段写入（标题 + 引脚 + 内嵌内容共同决定），属于缓存值，
     * 请勿手工设置；反向依赖它做业务判断前请先确保布局已执行。
     */
    core::Size size{};
    /** 显式配色；hasColor 为 false 时由编辑器取主题配色。 */
    core::Color color{};
    bool hasColor{};
    /**
     * 所属分组。无效表示未绑定；由 RefreshGroupMembership 按几何写入，
     * MoveGroup 优先按该字段整组平移，避免大位移后几何判定失效。
     */
    DuiNodeId group{};
};

/** 连线。 */
struct DuiGraphLink final
{
    DuiLinkId id{};
    DuiPinId start{};
    DuiPinId end{};
    /** 显式配色；hasColor 为 false 时由编辑器取主题配色。 */
    core::Color color{};
    bool hasColor{};
};

/**
 * 蓝图图模型。
 * 所有增删都做完整性维护：删除节点会连带删除其引脚与相关连线。
 */
class DuiNodeGraph final
{
public:
    DuiNodeGraph() = default;

    /**
     * 追加一个普通节点。
     * @param title 标题文本。
     * @param position 画布坐标下的左上角位置。
     * @return 新节点 id；永远有效。
     */
    DuiNodeId AddNode(std::string title, core::Point position = {});

    /**
     * 追加一个分组框。
     * @param title 分组标题。
     * @param bounds 画布坐标下的分组矩形。
     * @return 新节点 id；永远有效。
     */
    DuiNodeId AddGroup(std::string title, core::Rect bounds);

    /**
     * 追加一个注释框（便签）。
     * @param title 注释标题。
     * @param bounds 画布坐标下的矩形。
     * @return 新节点 id；永远有效。
     */
    DuiNodeId AddComment(std::string title, core::Rect bounds);

    /**
     * 为节点追加引脚。
     * @param node 目标普通节点；分组/注释或不存在时返回无效 id。
     * @param name 引脚显示名。
     * @param kind 输入或输出。
     * @param multipleConnections 是否允许接入多条连线。
     * @return 新引脚 id；节点不存在或非普通节点时返回无效 id。
     */
    DuiPinId AddPin(DuiNodeId node, std::string name, DuiPinKind kind,
                    bool multipleConnections = false, std::string typeId = {});

    /**
     * 建立连线。
     * 校验：两端引脚均存在、方向为 Output -> Input、类型兼容、不接在同一引脚、不与已有连线重复。
     * @return 新连线 id；校验失败时返回无效 id。
     */
    DuiLinkId AddLink(DuiPinId start, DuiPinId end);

    /** 删除节点，并连带删除其全部引脚与相关连线。返回是否删除成功。 */
    bool RemoveNode(DuiNodeId node);
    /** 删除引脚，并连带删除其全部连线。返回是否删除成功。 */
    bool RemovePin(DuiPinId pin);
    /** 删除连线。返回是否删除成功。 */
    bool RemoveLink(DuiLinkId link);
    /** 删除节点的全部连线（保留节点与引脚）。返回删除的连线数。 */
    int BreakLinksOfNode(DuiNodeId node);
    /** 清空全部内容。 */
    void Clear();

    [[nodiscard]] const std::vector<DuiGraphNode>& Nodes() const { return nodes_; }
    [[nodiscard]] const std::vector<DuiGraphPin>& Pins() const { return pins_; }
    [[nodiscard]] const std::vector<DuiGraphLink>& Links() const { return links_; }
    /** 非 const 版本供编辑器的布局阶段写回节点尺寸；请勿用它绕过增删接口的完整性维护。 */
    [[nodiscard]] std::vector<DuiGraphNode>& Nodes() { return nodes_; }
    [[nodiscard]] bool Empty() const { return nodes_.empty(); }

    [[nodiscard]] const DuiGraphNode* FindNode(DuiNodeId node) const;
    [[nodiscard]] DuiGraphNode* FindNode(DuiNodeId node);
    [[nodiscard]] const DuiGraphPin* FindPin(DuiPinId pin) const;
    [[nodiscard]] DuiGraphPin* FindPin(DuiPinId pin);
    [[nodiscard]] const DuiGraphLink* FindLink(DuiLinkId link) const;

    /** @return 指定节点的引脚，按添加顺序；节点不存在时返回空。 */
    [[nodiscard]] std::vector<DuiPinId> PinsOf(DuiNodeId node) const;
    /** @return 与指定引脚相连的连线。 */
    [[nodiscard]] std::vector<DuiLinkId> LinksOfPin(DuiPinId pin) const;
    /** @return 与指定节点任一端相连的连线。 */
    [[nodiscard]] std::vector<DuiLinkId> LinksOfNode(DuiNodeId node) const;
    /** @return 指定引脚当前连接的引脚（取第一条连线）；无连线时返回无效 id。 */
    [[nodiscard]] DuiPinId ConnectedPin(DuiPinId pin) const;

    /**
     * 以显式 id 恢复节点（供反序列化使用）。
     * 说明：持久化格式必须保 id 稳定，否则"保存-读取-再保存"会让宿主持有的 id 全部失效。
     * @return id 无效、为 0 或已存在时返回 false。
     */
    bool RestoreNode(const DuiGraphNode& node);
    /** 以显式 id 恢复引脚；其所属节点必须已存在。 */
    bool RestorePin(const DuiGraphPin& pin);
    /** 以显式 id 恢复连线；两端引脚必须已存在。 */
    bool RestoreLink(const DuiGraphLink& link);

    /** 设置节点画布坐标。返回节点是否存在。 */
    bool SetNodePosition(DuiNodeId node, core::Point position);
    /**
     * 设置分组/注释框尺寸。
     * @return 节点存在且为 Group 或 Comment 时返回 true。
     */
    bool SetGroupSize(DuiNodeId group, core::Size size);
    /** 移动整组：把分组矩形与其内全部普通节点一起平移。返回分组是否存在。 */
    bool MoveGroup(DuiNodeId group, core::Point delta);

    /**
     * 节点是否属于分组：已写入 group 字段时以字段为准，否则按左上角是否落在分组矩形内。
     */
    [[nodiscard]] bool NodeBelongsTo(DuiNodeId node, DuiNodeId group) const;
    /** 按几何把普通节点绑定到包含它的分组（重叠时后添加的分组优先）。 */
    void RefreshGroupMembership();

    /** @return 包含全部节点的画布包围盒；无节点时返回空矩形。 */
    [[nodiscard]] core::Rect ContentBounds() const;

    /**
     * 预判连线是否合法；不改动模型。
     * @return 合法返回 true。
     */
    [[nodiscard]] bool CanLink(DuiPinId start, DuiPinId end) const;

private:
    void rebuildIndices();
    [[nodiscard]] bool belongsByGeometry(const DuiGraphNode& node, const DuiGraphNode& group) const;

    std::vector<DuiGraphNode> nodes_;
    std::vector<DuiGraphPin> pins_;
    std::vector<DuiGraphLink> links_;
    std::unordered_map<std::uint64_t, std::size_t> nodeIndex_;
    std::unordered_map<std::uint64_t, std::size_t> pinIndex_;
    std::unordered_map<std::uint64_t, std::size_t> linkIndex_;
    std::uint64_t nextId_{1};
};

} // namespace ysDui::controls::graph
