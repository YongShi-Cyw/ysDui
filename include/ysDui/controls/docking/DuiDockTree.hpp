/**
 * 文件名：DuiDockTree.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：停靠布局的纯数据模型：嵌套分割树 + 标签组，含不变式整理与 XML 存取。
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ysDui::controls::docking {

/** 停靠槽位。 */
enum class DuiDockSlot
{
    Left,   // 左侧栏：沿竖直方向切分锚点，新组位于左侧。
    Right,  // 右侧栏：沿竖直方向切分锚点，新组位于右侧。
    Top,    // 顶部栏：沿水平方向切分整棵树，新组位于上方。
    Bottom, // 底部栏：沿水平方向切分整棵树，新组位于下方。
    Center, // 中央内容区：后建的左右栏以该组为锚点切分。
};

/** 分割方向。 */
enum class DuiDockOrientation
{
    Vertical,   // 左右二分：分割条竖直。
    Horizontal, // 上下二分：分割条水平。
};

/** 窗格描述。布局只持久化 id 与标题，内容由调用方另行提供。 */
struct DuiDockPane final
{
    std::string id;    // 稳定标识，由调用方给定，跨次运行不变。
    std::string title; // 标签文字。
};

/** 布局树节点种类。 */
enum class DuiDockNodeKind
{
    Group, // 叶子：一个标签组，可容纳多个窗格。
    Split, // 内部节点：二分区域，`children` 恒为两个。
};

/** 布局树节点。 */
struct DuiDockNode final
{    std::uint64_t id{};                                           // 节点标识，供视图回写时定位。
    DuiDockNodeKind kind{DuiDockNodeKind::Group};                 // 节点种类。
    DuiDockOrientation orientation{DuiDockOrientation::Vertical};  // 仅 Split 有效：子节点排布方向。
    double fraction{0.5};                                         // 仅 Split 有效：第一个子节点占比。
    std::vector<std::unique_ptr<DuiDockNode>> children;           // 仅 Split：恰好两个子节点。
    std::vector<DuiDockPane> panes;                               // 仅 Group：组内窗格。
    int active{};                                                 // 仅 Group：选中标签下标。
};

/** 自动隐藏窗格：收起为指定边缘的窄条。 */
struct DuiDockAutoHideItem final
{
    std::string id;                       // 窗格标识。
    std::string title;                    // 窗格标题，展开时恢复用。
    DuiDockSlot slot{DuiDockSlot::Left};  // 收起到哪条边。
};

/**
 * 停靠布局模型。
 *
 * 用嵌套分割树表达任意停靠结构，取代扁平的五槽模型：每个 `Split` 二分一块区域，
 * 每个 `Group` 是一个标签组；左右栏切分锚点组，上下栏切分整棵树。
 *
 * 不变式由 `normalize()` 统一维护（每次结构变更后调用）：
 * - 空 `Group` 被回收；`Split` 只有一个有内容的子节点时被折叠、子节点上提并保留其标识；
 * - `Split` 恰好两个子节点；`Group::active` 始终指向有效下标。
 *
 * 窗格内容不归本模型管理，模型只持有 id 与标题。
 */
class DuiDockTree final
{
public:
    DuiDockTree();
    ~DuiDockTree();
    DuiDockTree(const DuiDockTree&) = delete;
    DuiDockTree& operator=(const DuiDockTree&) = delete;
    DuiDockTree(DuiDockTree&&) noexcept;
    DuiDockTree& operator=(DuiDockTree&&) noexcept;

    /**
     * 追加窗格。
     * 槽位组已存在时并入该组；否则按槽位新建组（`Top` / `Bottom` 切分整棵树，
     * 其余槽位切分锚点）。空树的第一个窗格无论槽位都占据整块区域。
     * @param id 窗格标识；为空或重复时失败
     * @param title 标签文字
     * @param slot 目标槽位
     * @return 成功返回 true
     */
    bool AddPane(std::string id, std::string title, DuiDockSlot slot = DuiDockSlot::Center);

    /**
     * 在指定窗格所在组旁切出新组并放入新窗格，是嵌套分割的入口。
     * @param anchorPaneId 参照窗格
     * @param id 新窗格标识
     * @param title 新窗格标题
     * @param slot 新组相对参照组的位置；`Center` 表示并入参照组
     * @param fraction 切分比例；仅新建分割时生效
     * @return 成功返回 true
     */
    bool AddPaneBeside(std::string_view anchorPaneId, std::string id, std::string title,
                       DuiDockSlot slot, double fraction = 0.5);

    /**
     * 移除窗格；空组会被回收，仅剩单个子节点的分割会被折叠。
     * @return 窗格存在并移除成功返回 true
     */
    bool RemovePane(std::string_view id);

    /**
     * 移动窗格到目标窗格处。
     * @param slot `Center` 表示并入目标组成为标签，其余表示在目标组旁切出新组
     * @param fraction 切分比例；仅新建分割时生效
     * @return 成功返回 true
     */
    bool MovePane(std::string_view id, std::string_view targetId,
                  DuiDockSlot slot = DuiDockSlot::Center, double fraction = 0.5);

    /**
     * 修改窗格标题。
     * @return 窗格存在返回 true
     */
    bool SetPaneTitle(std::string_view id, std::string title);

    /**
     * 调整组内窗格顺序（标签重排）。
     * @param groupNodeId 组节点标识
     * @param from 原下标
     * @param to 目标下标；越界会钳制到有效范围
     * @return 该标识指向一个组且下标有效时返回 true
     */
    bool ReorderPane(std::uint64_t groupNodeId, int from, int to);

    /** @return 布局树根节点；空树时为不含窗格的 `Group`。 */
    [[nodiscard]] const DuiDockNode& Root() const;
    /** @return 树中不含任何窗格时返回 true。 */
    [[nodiscard]] bool Empty() const;
    /** @return 存在该标识的窗格（含自动隐藏）返回 true。 */
    [[nodiscard]] bool Contains(std::string_view id) const;
    /** @return 窗格总数（含自动隐藏）。 */
    [[nodiscard]] int PaneCount() const;
    /** @return 全部窗格标识（含自动隐藏），先树内后自动隐藏。 */
    [[nodiscard]] std::vector<std::string> PaneIds() const;
    /** @return 窗格标题；不存在时为空串。 */
    [[nodiscard]] std::string PaneTitle(std::string_view id) const;
    /** @return 自动隐藏条目，按加入顺序。 */
    [[nodiscard]] const std::vector<DuiDockAutoHideItem>& AutoHideItems() const;
    /** @return 是否收起到边缘窄条。 */
    [[nodiscard]] bool IsPaneAutoHidden(std::string_view id) const;

    /**
     * 设置窗格是否收起到边缘窄条。
     * 收起时从树内移出（不占布局空间），展开时重新并入中央组。
     * @param id 窗格标识
     * @param autoHide true 收起，false 展开
     * @param slot 收起时贴附的边；展开时忽略
     * @return 窗格存在且状态有变化返回 true
     */
    bool SetPaneAutoHide(std::string_view id, bool autoHide, DuiDockSlot slot = DuiDockSlot::Left);
    /**
     * 当前活动窗格：优先中央组，其次深度优先遇到的第一个非空组。
     * @return 窗格标识；无窗格时为空串
     */
    [[nodiscard]] std::string ActivePane() const;
    /** @return 容纳该窗格的组；不存在时为空指针。 */
    [[nodiscard]] const DuiDockNode* FindGroup(std::string_view paneId) const;

    /**
     * 选中指定窗格。
     * @return 窗格存在返回 true
     */
    bool SetActivePane(std::string_view paneId);

    /**
     * 视图回写：设置某组选中的标签下标（下标越界时钳制）。
     * @param groupNodeId 组节点标识
     * @return 该标识指向一个组时返回 true
     */
    bool SetGroupActive(std::uint64_t groupNodeId, int index);

    /**
     * 视图回写：设置某分割的切分比例（比例会被钳制到可见范围）。
     * @param splitNodeId 分割节点标识
     * @return 该标识指向一个分割时返回 true
     */
    bool SetSplitFraction(std::uint64_t splitNodeId, double fraction);

    /**
     * 序列化布局为 UTF-8 XML。
     * 只写入结构与窗格 id：窗格集合与标题以调用方的窗格登记为准，不进布局文件，
     * 以免布局与登记不一致时静默改名或丢窗格。节点标识为运行时概念，同样不写入。
     * 自动隐藏的收起位置会写入 `<auto-hide>` 子元素。
     * @param text 输出文本
     * @return 成功返回 true
     */
    [[nodiscard]] bool SaveLayout(std::string& text) const;

    /**
     * 按给定布局重排当前窗格集合。
     * 布局中未出现的窗格会被补进第一个组（不会消失）；布局中出现但当前不存在的
     * 窗格会被忽略；解析失败时保留原有布局不变。
     * 窗格标题取自当前登记，布局文件不携带标题。
     * 载入后槽位锚点重置为活动窗格所在的组。
     * @param text UTF-8 XML 文本
     * @return 成功返回 true
     */
    bool LoadLayout(std::string_view text);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::controls::docking
