#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiNinePatch.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::basic {

enum class DuiButtonKind {
    Push,
    Icon,
    /** 整颗按钮触发下拉：点击任意位置调用下拉回调（用于展开菜单）。 */
    DropDown,
    /** 分裂按钮：左侧主区域触发点击回调，右侧窄条触发下拉回调。 */
    Split,
};

enum class DuiButtonVariant {
    Primary,
    Default,
    Danger,
    Ghost,
    Outlined,
    Text,
};

struct DuiButtonSkin final {
    std::shared_ptr<const render::DuiImage> normal;
    std::shared_ptr<const render::DuiImage> hovered;
    std::shared_ptr<const render::DuiImage> pressed;
    std::shared_ptr<const render::DuiImage> disabled;
    render::DuiNinePatchInsets insets;
};

class DuiButton final : public core::Control, public render::DuiRenderable {
public:
    DuiButton();
    ~DuiButton() override;
    DuiButton(const DuiButton&) = delete;
    DuiButton& operator=(const DuiButton&) = delete;
    DuiButton(DuiButton&&) noexcept;
    DuiButton& operator=(DuiButton&&) noexcept;

    void SetText(std::string text);
    [[nodiscard]] const std::string& Text() const;
    void SetKind(DuiButtonKind kind);
    [[nodiscard]] DuiButtonKind Kind() const;
    void SetVariant(DuiButtonVariant variant);
    [[nodiscard]] DuiButtonVariant Variant() const;
    void SetSkin(DuiButtonSkin skin);
    [[nodiscard]] const DuiButtonSkin& Skin() const;
    void SetLeadingIcon(std::shared_ptr<const render::DuiImage> icon);
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& LeadingIcon() const;
    void SetLeadingIconSize(core::Size size);
    [[nodiscard]] core::Size LeadingIconSize() const;
    void SetLeadingIconGap(int pixels);
    [[nodiscard]] int LeadingIconGap() const;
    void SetTextStyle(render::DuiTextStyle style);
    [[nodiscard]] const render::DuiTextStyle& TextStyle() const;
    void SetClickHandler(std::function<void()> handler);
    /**
     * 设置下拉回调。
     * 用途：`DropDown` 时整颗按钮触发；`Split` 时仅右侧窄条触发。
     * 未设置且 kind 为 `DropDown` 时回退到点击回调，便于按普通按钮方式打开菜单。
     */
    void SetDropDownHandler(std::function<void()> handler);
    /** @return 下拉窄条区域；非 `DropDown` / `Split` 时为空矩形。 */
    [[nodiscard]] core::Rect DropDownRect() const;
    /** @return 主操作区域；`Split` 时为左侧部分，其余 kind 为整个按钮。 */
    [[nodiscard]] core::Rect MainRect() const;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;
    bool PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                    std::string_view value) override;

private:
    void Activate();
    class Impl;
    std::unique_ptr<Impl> button_;
};

} // namespace ysDui::controls::basic
