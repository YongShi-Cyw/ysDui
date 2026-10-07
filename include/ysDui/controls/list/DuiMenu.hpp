#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/core/DuiSubscription.hpp"
#include "ysDui/ui/DuiHostRef.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"

namespace ysDui::render {
class DuiImage;
class DuiTextMeasurer;
struct DuiTextStyle;
}

namespace ysDui::ui {
class IUiHostFactory;
}

namespace ysDui::controls::list {

class DuiMenu;

enum class DuiMenuItemKind {
    Command,
    Checkable,
    Separator,
    Submenu,
};

struct DuiMenuItem final {
    std::uint32_t id{};
    DuiMenuItemKind kind{DuiMenuItemKind::Command};
    std::string text;
    bool enabled{true};
    bool checked{};
    DuiMenu* submenu{};
    std::shared_ptr<const render::DuiImage> image; // 菜单项图标（可空）；存在图标的菜单项文字整体右移对齐
};

class DuiMenu final {
public:
    DuiMenu();
    ~DuiMenu();
    DuiMenu(const DuiMenu&) = delete;
    DuiMenu& operator=(const DuiMenu&) = delete;
    DuiMenu(DuiMenu&&) noexcept;
    DuiMenu& operator=(DuiMenu&&) noexcept;

    void AddItem(std::uint32_t id, std::string text);
    /**
     * 添加带图标的菜单项
     * @param id 菜单项标识
     * @param text 显示文本（可含助记符 &）
     * @param image 图标；为空则与 AddItem 等效
     */
    void AddItem(std::uint32_t id, std::string text, std::shared_ptr<const render::DuiImage> image);
    void AddCheckItem(std::uint32_t id, std::string text, bool checked = false);
    void AddSubmenu(std::uint32_t id, std::string text, DuiMenu* submenu);
    void AddSeparator();
    void ClearItems();
    [[nodiscard]] int ItemCount() const;
    [[nodiscard]] DuiMenuItem ItemAt(int index) const;
    void SetItemEnabled(std::uint32_t id, bool enabled);
    void SetItemChecked(std::uint32_t id, bool checked);
    [[nodiscard]] bool ItemChecked(std::uint32_t id) const;
    void SetTextMeasurer(render::DuiTextMeasurer* measurer);
    void SetTextStyle(render::DuiTextStyle style);
    void SetItemInvokedHandler(std::function<void(std::uint32_t)> handler);

    /**
     * 订阅菜单项执行事件，并由返回句柄自动管理订阅生命周期。
     * @param handler 菜单项执行后接收其标识的函数。
     * @return 移动式订阅句柄；空回调返回空句柄。
     */
    [[nodiscard]] core::DuiSubscription SubscribeItemInvokedScoped(
        std::function<void(std::uint32_t)> handler);

    /**
     * 订阅菜单关闭事件，并由返回句柄自动管理订阅生命周期。
     * @param handler 菜单关闭后调用的函数。
     * @return 移动式订阅句柄；空回调返回空句柄。
     */
    [[nodiscard]] core::DuiSubscription SubscribeClosedScoped(std::function<void()> handler);
    [[deprecated("Use SubscribeItemInvokedScoped()")]] [[nodiscard]] std::size_t SubscribeItemInvoked(
        std::function<void(std::uint32_t)> handler);
    [[deprecated("Use SubscribeClosedScoped()")]] [[nodiscard]] std::size_t SubscribeClosed(
        std::function<void()> handler);
    [[deprecated("Use DuiSubscription::Reset()")]] void Unsubscribe(std::size_t subscription);

    /**
     * 在锚点旁弹出菜单。
     * @param factory UI 宿主工厂
     * @param owner 归属宿主
     * @param anchor 锚点矩形（客户区坐标）
     * @param placement 相对锚点的优先放置方向，默认下方
     * @return 成功显示返回 true
     */
    bool Show(ui::IUiHostFactory& factory, ui::HostRef owner, core::Rect anchor,
              ui::DuiPopupPlacement placement = ui::DuiPopupPlacement::Below);
    void Hide();
    [[nodiscard]] bool Visible() const;

private:
    class Impl;
    std::unique_ptr<Impl> menu_;
};

} // namespace ysDui::controls::list
