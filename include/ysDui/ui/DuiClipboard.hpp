/**
 * 文件名：DuiClipboard.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明平台无关的 UTF-8 文本剪贴板能力。
 */
#pragma once

#include <optional>
#include <string>

namespace ysDui::ui {

class DuiClipboard
{
public:
    virtual ~DuiClipboard() = default;
    /** @return 平台接受 UTF-8 文本时返回 true。 */
    virtual bool SetText(std::string text) = 0;
    [[nodiscard]] virtual std::optional<std::string> GetText() const { return std::nullopt; }
};

} // namespace ysDui::ui
