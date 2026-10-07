/**
 * 文件名：DuiUtf8Path.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：在内部文件边界将 UTF-8 文本转换为原生文件路径。
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ysDui::core::detail {

[[nodiscard]] inline std::optional<std::filesystem::path> PathFromUtf8(std::string_view value)
{
    if (value.empty())
        return std::nullopt;

    std::u8string utf8;
    utf8.reserve(value.size());
    for (const char character : value)
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));

    try
    {
        return std::filesystem::path(utf8);
    }
    catch (const std::filesystem::filesystem_error&)
    {
        return std::nullopt;
    }
}

[[nodiscard]] inline std::string PathToUtf8(const std::filesystem::path& value)
{
    const std::u8string utf8 = value.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

} // namespace ysDui::core::detail
