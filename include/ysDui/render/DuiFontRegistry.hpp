/**
 * 文件名：DuiFontRegistry.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明渲染后端提供的字体注册能力。
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ysDui::render {

class DuiFontRegistry
{
public:
    virtual ~DuiFontRegistry() = default;
    virtual bool RegisterFile(std::string path) = 0;
    virtual bool RegisterMemory(std::vector<std::uint8_t> bytes) = 0;
    [[nodiscard]] virtual bool HasFamily(std::string_view family) const = 0;
};

} // namespace ysDui::render
