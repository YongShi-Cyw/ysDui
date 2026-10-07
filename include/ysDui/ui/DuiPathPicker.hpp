/**
 * 文件名：DuiPathPicker.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明平台无关的 UTF-8 路径选择能力。
 */
#pragma once

#include <string>
#include <string_view>

namespace ysDui::ui {

enum class DuiPathPickerMode
{
    OpenFile,
    SaveFile,
    Folder,
};

struct DuiPathPickerResult final
{
    bool accepted{};
    std::string path;
};

class DuiPathPicker
{
public:
    virtual ~DuiPathPicker() = default;
    /** @return 用户确认状态及 UTF-8 路径。 */
    [[nodiscard]] virtual DuiPathPickerResult Browse(DuiPathPickerMode mode,
                                                      std::string_view initialPath) = 0;
};

} // namespace ysDui::ui
