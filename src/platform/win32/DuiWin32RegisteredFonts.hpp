/**
 * 文件名：DuiWin32RegisteredFonts.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-30
 * 用途：在 Win32 字体注册器与 DirectWrite 文本后端之间传递私有字体文件。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ysDui::platform::win32::registered_fonts {

using Token = std::size_t;

struct Snapshot final
{
    std::size_t generation{};
    std::vector<std::wstring> files;
    std::vector<std::shared_ptr<const std::vector<std::uint8_t>>> memoryFiles;
};

[[nodiscard]] Token AddFile(std::wstring path);
[[nodiscard]] Token AddMemory(std::shared_ptr<const std::vector<std::uint8_t>> bytes);
void Remove(Token token);
[[nodiscard]] Snapshot FontSnapshot();

} // namespace ysDui::platform::win32::registered_fonts
