/**
 * 文件名：DuiSkin.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的皮肤目录、皮肤会话和逻辑资源路径解析接口。
 */
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/core/DuiSubscription.hpp"

namespace ysDui::core {

/**
 * 皮肤目录中的单个皮肤描述。
 * 用途：保存皮肤的稳定标识、显示名称和逻辑资源目录前缀。
 */
struct DuiSkinDescriptor final
{
    int id{};                    ///< 皮肤的稳定整数标识。
    std::string name;            ///< 面向用户显示的 UTF-8 名称。
    std::string resourcePrefix;  ///< 皮肤资源的 UTF-8 逻辑目录前缀。
};

/**
 * 只读皮肤目录。
 * 用途：解析源库兼容的 UTF-8 SkinList XML，而不依赖文件系统、图像类型或平台后端。
 */
class DuiSkinCatalog final
{
public:
    DuiSkinCatalog();
    ~DuiSkinCatalog();
    DuiSkinCatalog(const DuiSkinCatalog&) = delete;
    DuiSkinCatalog& operator=(const DuiSkinCatalog&) = delete;
    DuiSkinCatalog(DuiSkinCatalog&&) noexcept;
    DuiSkinCatalog& operator=(DuiSkinCatalog&&) noexcept;

    /**
     * 从 UTF-8 SkinList XML 加载皮肤目录。
     * @param xml 含 Skin、SkinID、SkinName、SkinPath 与可选 CurSkinID 的 XML 文本。
     * @return 目录完整有效时返回 true；失败时保留原有目录。
     */
    bool Load(std::string_view xml);

    /**
     * 获取目录内全部皮肤。
     * @return 按 XML 中出现顺序复制的皮肤描述列表。
     */
    [[nodiscard]] std::vector<DuiSkinDescriptor> Skins() const;

    /**
     * 按标识查找皮肤。
     * @param id 待查找的皮肤标识。
     * @return 找到时返回皮肤描述，否则返回空值。
     */
    [[nodiscard]] std::optional<DuiSkinDescriptor> Find(int id) const;

    /**
     * 获取配置的默认皮肤。
     * @return CurSkinID 有效时返回对应皮肤；未指定时返回目录第一项；目录为空时返回空值。
     */
    [[nodiscard]] std::optional<DuiSkinDescriptor> DefaultSkin() const;

private:
    class Impl;
    std::unique_ptr<Impl> catalog_;
};

/**
 * 显式拥有皮肤选择状态的会话。
 * 用途：替代源库全局单例，提供皮肤切换通知和按当前皮肤解析逻辑资源路径的能力。
 */
class DuiSkinSession final
{
public:
    DuiSkinSession();
    ~DuiSkinSession();
    DuiSkinSession(const DuiSkinSession&) = delete;
    DuiSkinSession& operator=(const DuiSkinSession&) = delete;
    DuiSkinSession(DuiSkinSession&&) noexcept;
    DuiSkinSession& operator=(DuiSkinSession&&) noexcept;

    /**
     * 设置会话使用的皮肤目录。
     * @param catalog 目录的共享只读所有权；空指针会清空当前皮肤。
     */
    void SetCatalog(std::shared_ptr<const DuiSkinCatalog> catalog);

    /**
     * 切换当前皮肤。
     * @param id 目录中存在的皮肤标识。
     * @return 皮肤存在且成功切换时返回 true。
     */
    bool SetCurrentSkin(int id);

    /**
     * 获取当前皮肤。
     * @return 当前皮肤描述；未设置目录或皮肤时返回空值。
     */
    [[nodiscard]] std::optional<DuiSkinDescriptor> CurrentSkin() const;

    /**
     * 依据当前皮肤拼接逻辑资源路径。
     * @param path 不带皮肤目录前缀的 UTF-8 逻辑路径。
     * @return 当前皮肤的资源前缀与路径以单个斜杠拼接；无当前皮肤时原样返回。
     */
    [[nodiscard]] std::string ResolveResourcePath(std::string_view path) const;

    /**
     * 订阅当前皮肤变更，并由返回句柄自动管理订阅生命周期。
     * @param callback 切换目录或皮肤后调用的函数。
     * @return 移动式订阅句柄；空回调返回空句柄。
     */
    [[nodiscard]] DuiSubscription SubscribeScoped(std::function<void()> callback);

    /**
     * 订阅当前皮肤变更。
     * @param callback 切换目录或皮肤后调用的函数。
     * @return 用于取消订阅的非零令牌；空回调返回零。
     */
    [[deprecated("Use SubscribeScoped()")]] std::size_t Subscribe(std::function<void()> callback);

    /**
     * 取消皮肤变更订阅。
     * @param token Subscribe 返回的令牌。
     */
    [[deprecated("Use DuiSubscription::Reset()")]] void Unsubscribe(std::size_t token);

private:
    class Impl;
    static void Notify(Impl& session);
    std::unique_ptr<Impl> session_;
};

} // namespace ysDui::core
