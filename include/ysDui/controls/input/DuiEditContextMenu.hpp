/**
 * 文件名：DuiEditContextMenu.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明文本输入控件右键菜单的无平台命令模型。
 */
#pragma once

#include <string_view>
#include <vector>

namespace ysDui::controls::input {

/**
 * 文本输入右键菜单命令。
 */
enum class DuiEditContextCommand
{
    Cut,       // 剪切选中文本。
    Copy,      // 复制选中文本。
    Paste,     // 粘贴剪贴板文本。
    SelectAll, // 选中全部文本。
    Separator  // 分隔菜单组。
};

/**
 * 构建文本输入右键菜单所需的状态。
 */
struct DuiEditContextState final
{
    bool readOnly{};         // 文本是否只读。
    bool hasSelection{};     // 是否存在非空选区。
    bool clipboardHasText{}; // 剪贴板是否包含可粘贴文本。
    bool hasText{};          // 输入框是否包含文本。
    bool password{};         // 是否为密码输入。
};

/**
 * 右键菜单的一项。
 */
struct DuiEditContextMenuItem final
{
    DuiEditContextCommand command;
    bool enabled{};
};

/**
 * 根据输入状态构建标准编辑右键菜单。
 * @param state 文本输入当前状态。
 * @return 按显示顺序排列的菜单项。
 */
[[nodiscard]] inline std::vector<DuiEditContextMenuItem> BuildDuiEditContextMenu(const DuiEditContextState& state)
{
    const bool copyEnabled = state.hasSelection && !state.password;
    std::vector<DuiEditContextMenuItem> items;
    if (!state.readOnly) {
        items.push_back({DuiEditContextCommand::Cut, copyEnabled});
        items.push_back({DuiEditContextCommand::Copy, copyEnabled});
        items.push_back({DuiEditContextCommand::Paste, state.clipboardHasText});
    } else {
        items.push_back({DuiEditContextCommand::Copy, copyEnabled});
    }
    items.push_back({DuiEditContextCommand::Separator, false});
    items.push_back({DuiEditContextCommand::SelectAll, state.hasText});
    return items;
}

/**
 * 返回标准编辑命令的 UTF-16 菜单文案。
 * @param command 菜单命令。
 * @return 对应文案；分隔条返回空文本。
 */
[[nodiscard]] inline std::string_view DuiEditContextCommandLabel(DuiEditContextCommand command)
{
    switch (command) {
    case DuiEditContextCommand::Cut: return "剪切(&T)";
    case DuiEditContextCommand::Copy: return "复制(&C)";
    case DuiEditContextCommand::Paste: return "粘贴(&P)";
    case DuiEditContextCommand::SelectAll: return "全选(&A)";
    case DuiEditContextCommand::Separator: return {};
    }
    return {};
}

} // namespace ysDui::controls::input
