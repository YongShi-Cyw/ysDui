/**
 * 文件名：DuiResourcePackage.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的只读资源包接口。
 */
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ysDui::resource {

/**
 * 资源包内容加密密钥。
 * 用途：承载 XChaCha20-Poly1305 所需的固定 32 字节原始密钥，不暴露加密后端类型。
 */
using DuiResourceKey = std::array<std::uint8_t, 32>;

/**
 * 基于 ZIP 的只读资源包。
 * 用途：按 UTF-8 条目名称读取经过 Deflate 压缩的资源，不暴露 ZIP 后端类型。
 */
class DuiResourcePackage final
{
public:
    ~DuiResourcePackage();
    DuiResourcePackage(const DuiResourcePackage&) = delete;
    DuiResourcePackage& operator=(const DuiResourcePackage&) = delete;
    DuiResourcePackage(DuiResourcePackage&&) noexcept;
    DuiResourcePackage& operator=(DuiResourcePackage&&) noexcept;

    /**
     * 打开资源包文件。
     * @param path 资源包的 UTF-8 文件路径。
     * @param key 可选的 32 字节原始密钥；加密资源包读取时必须提供匹配密钥。
     * @return 成功时返回资源包对象，否则返回空值。
     */
    [[nodiscard]] static std::optional<DuiResourcePackage> Open(
        std::string_view path, std::optional<DuiResourceKey> key = std::nullopt);

    /**
     * 查询资源条目是否存在。
     * @param name 资源条目的 UTF-8 名称。
     * @return 条目存在时返回 true。
     */
    [[nodiscard]] bool Contains(std::string_view name) const;

    /**
     * 读取资源条目的原始字节。
     * @param name 资源条目的 UTF-8 名称。
     * @return 成功时返回条目字节，否则返回空值。
     */
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> Read(std::string_view name) const;

    /**
     * 枚举资源包中所有非目录条目的 UTF-8 名称。
     * @return 条目名称列表。
     */
    [[nodiscard]] std::vector<std::string> Entries() const;

private:
    class Impl;
    explicit DuiResourcePackage(std::unique_ptr<Impl> implementation);
    std::unique_ptr<Impl> implementation_;
};

} // namespace ysDui::resource
