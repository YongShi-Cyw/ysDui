/**
 * 文件名：DuiAccessibility.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的无障碍语义和控件树遍历接口。
 */
#pragma once

#include <cstdint>
#include <string>

namespace ysDui::core {

class Control;

/** 无障碍节点角色。 */
enum class DuiAccessibilityRole
{
    // 未公开：节点只作为内部布局结构存在。
    None,
    // 窗格：承载一组相关内容。
    Pane,
    // 分组：组织具有无障碍语义的子节点。
    Group,
    // 按钮：可触发一个操作。
    Button,
    // 复选框：可在选中和未选中间切换。
    CheckBox,
    // 单选按钮：在一组互斥选项中选择一个。
    RadioButton,
    // 超链接：激活后导航到关联目标。
    Hyperlink,
    // 编辑框：接受文本输入。
    Edit,
    // 文本：显示不可编辑文本。
    Text,
    // 开关：表示二元状态。
    Switch,
    // 滑块：表示连续或离散数值。
    Slider,
    // 组合框：从多个候选项中选择。
    ComboBox,
    // 列表：呈现线性项目集合。
    List,
    Table,
    // 树：呈现层级项目集合。
    Tree,
    // 进度条：显示任务进度。
    ProgressBar,
    // 滚动条：控制内容滚动位置。
    ScrollBar,
    // 图像：显示非文本视觉内容。
    Image,
    // 工具栏：承载一组操作项。
    ToolBar,
    // 状态栏：显示状态信息。
    StatusBar,
    // 选项卡：切换关联页面。
    Tab,
    // 菜单：显示命令集合。
    Menu,
};

/** 平台无关的无障碍操作模式。 */
enum class DuiAccessibilityPattern : std::uint32_t
{
    None = 0,                    // 控件不提供可操作模式。
    Invoke = 1U << 0U,          // 控件可执行默认命令。
    Value = 1U << 1U,           // 控件公开并可选修改文本值。
    ExpandCollapse = 1U << 2U,  // 控件可展开或折叠。
    Selection = 1U << 3U,       // 控件公开当前选择范围。
    Grid = 1U << 4U,            // 控件公开二维行列与按坐标取项能力。
    Table = 1U << 5U,           // 控件公开表格方向与标题能力。
};

[[nodiscard]] constexpr DuiAccessibilityPattern operator|(DuiAccessibilityPattern left,
                                                           DuiAccessibilityPattern right) noexcept
{
    return static_cast<DuiAccessibilityPattern>(static_cast<std::uint32_t>(left)
                                                | static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr bool HasAccessibilityPattern(DuiAccessibilityPattern patterns,
                                                      DuiAccessibilityPattern pattern) noexcept
{
    return (static_cast<std::uint32_t>(patterns) & static_cast<std::uint32_t>(pattern)) != 0U;
}

/** 平台后端可请求控件执行的无障碍动作。 */
enum class DuiAccessibilityAction
{
    Invoke,    // 执行控件默认命令。
    SetValue,  // 将 UTF-8 文本值写入控件。
    Expand,    // 展开控件内容。
    Collapse,  // 折叠控件内容。
};

/** 平台无关的无障碍描述数据。 */
struct DuiAccessibilityData final
{
    DuiAccessibilityRole role{DuiAccessibilityRole::None};
    std::string name;
    std::string value;
    std::string description;
    bool keyboardFocusable{};
    std::string identifier;
    DuiAccessibilityPattern patterns{DuiAccessibilityPattern::None};
    bool readOnly{};
    bool expanded{};
};

/** 判断节点是否应在无障碍树中公开。 */
[[nodiscard]] bool IsAccessibilityNodeExposed(const Control* control);
/** 返回节点的有效角色，纯容器在有公开后代时作为分组公开。 */
[[nodiscard]] DuiAccessibilityRole EffectiveAccessibilityRole(const Control* control);
/** 返回最近的公开父节点。 */
[[nodiscard]] Control* AccessibilityParent(Control* control);
/** 返回第一个公开子节点。 */
[[nodiscard]] Control* AccessibilityFirstChild(Control* control);
/** 返回最后一个公开子节点。 */
[[nodiscard]] Control* AccessibilityLastChild(Control* control);
/** 返回下一个公开同级节点。 */
[[nodiscard]] Control* AccessibilityNextSibling(Control* control);
/** 返回上一个公开同级节点。 */
[[nodiscard]] Control* AccessibilityPreviousSibling(Control* control);
/** 从节点向下查找第一个公开节点；节点自身公开时直接返回自身。 */
[[nodiscard]] Control* DeepestAccessibleDescendant(Control* control);
/** 从节点向上查找最近的公开节点。 */
[[nodiscard]] Control* NearestAccessibleAncestor(Control* control);

} // namespace ysDui::core
