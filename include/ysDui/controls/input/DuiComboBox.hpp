#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"
#include "ysDui/ui/DuiTextInput.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::render {
class DuiImage;
}

namespace ysDui::controls::input {

class DuiComboBox final : public core::Control, public render::DuiRenderable {
public:
    DuiComboBox(); ~DuiComboBox() override;
    DuiComboBox(const DuiComboBox&) = delete; DuiComboBox& operator=(const DuiComboBox&) = delete;
    DuiComboBox(DuiComboBox&&) = delete; DuiComboBox& operator=(DuiComboBox&&) = delete;
    int AddItem(std::string text); void RemoveItem(int index); void ClearItems();
    [[nodiscard]] int Count() const; [[nodiscard]] std::string TextAt(int index) const;
    /** 设置指定选项的图标；空指针表示使用组合框统一图标。 */
    void SetItemIcon(int index, std::shared_ptr<const render::DuiImage> icon);
    /** 返回指定选项的图标；索引无效或未设置时返回空指针。 */
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& ItemIconAt(int index) const;
    void SetSelectedIndex(int index, bool notify = true); [[nodiscard]] int SelectedIndex() const;
    void SetEditable(bool editable); [[nodiscard]] bool Editable() const;
    void SetTextInput(ui::DuiTextInput* input); void SetPopupHost(ui::IPopupHost* popup);
    void SetText(std::string text, bool notify = false); [[nodiscard]] std::string Text() const;
    void SetIncrementalSearch(bool enabled); void SetSubstringSearch(bool enabled);
    [[nodiscard]] std::vector<int> FilteredIndices(std::string_view query) const;
    void SetMaxVisibleItems(int count); void SetItemHeight(int pixels);
    void SetArrowColor(core::Color color); [[nodiscard]] core::Color ArrowColor() const;
    /**
     * 设置是否绘制自身背景填充
     * 说明：置为 false 时只画边框/文字/箭头，背景由父级（或透明画布）呈现，
     *       供需要在自定义背景上叠加组合框的场景使用。
     */
    void SetBackgroundVisible(bool visible);
    /** @return 是否绘制自身背景填充。 */
    [[nodiscard]] bool BackgroundVisible() const;
    /** 设置只读显示区文字前的图标；传入空指针时清除图标。 */
    void SetLeadingIcon(std::shared_ptr<const render::DuiImage> icon);
    /** 返回只读显示区当前使用的前置图标。 */
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& LeadingIcon() const;
    /** 设置前置图标的逻辑像素尺寸。 */
    void SetLeadingIconSize(core::Size size);
    /** 返回前置图标的逻辑像素尺寸。 */
    [[nodiscard]] core::Size LeadingIconSize() const;
    /** 设置前置图标与文字之间的逻辑像素间距。 */
    void SetLeadingIconGap(int pixels);
    /** 返回前置图标与文字之间的逻辑像素间距。 */
    [[nodiscard]] int LeadingIconGap() const;
    void SetSelectionChangedHandler(std::function<void(int)> handler);
    void OpenPopup(); void ClosePopup(); [[nodiscard]] bool PopupOpen() const;
    [[nodiscard]] core::Rect ArrowRect() const; void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override; void Paint(render::Canvas& canvas, core::Rect dirty) const override;
private: class Impl; std::unique_ptr<Impl> comboBox_;
};

} // namespace ysDui::controls::input
