#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/controls/input/DuiScrollBar.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::render {
class DuiImage;
}

namespace ysDui::controls::list {

class DuiListBox final : public core::Control, public render::DuiRenderable {
public:
    DuiListBox();
    ~DuiListBox() override;
    DuiListBox(const DuiListBox&) = delete;
    DuiListBox& operator=(const DuiListBox&) = delete;
    DuiListBox(DuiListBox&&) = delete;
    DuiListBox& operator=(DuiListBox&&) = delete;

    int AddItem(std::string text, std::uintptr_t value = 0);
    void InsertItem(int index, std::string text, std::uintptr_t value = 0);
    void RemoveItem(int index);
    void ClearItems();
    [[nodiscard]] int Count() const;
    [[nodiscard]] std::string TextAt(int index) const;
    [[nodiscard]] std::uintptr_t ValueAt(int index) const;
    void SetTextAt(int index, std::string text);
    void SetValueAt(int index, std::uintptr_t value);
    /** 设置指定条目的前置图标；空指针表示使用列表统一图标。 */
    void SetIconAt(int index, std::shared_ptr<const render::DuiImage> icon);
    /** 返回指定条目的前置图标；索引无效或未设置时返回空指针。 */
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& IconAt(int index) const;

    void SetSelectedIndex(int index, bool notify = true);
    [[nodiscard]] int SelectedIndex() const;
    void SetMultiSelect(bool enabled);
    [[nodiscard]] bool MultiSelect() const;
    [[nodiscard]] bool IsSelected(int index) const;
    void SetSelected(int index, bool selected, bool notify = true);
    [[nodiscard]] int SelectionCount() const;
    [[nodiscard]] std::vector<int> SelectedIndices() const;
    void ClearSelection(bool notify = true);

    void SetCheckboxesVisible(bool visible);
    [[nodiscard]] bool CheckboxesVisible() const;
    [[nodiscard]] bool IsChecked(int index) const;
    void SetChecked(int index, bool checked, bool notify = true);
    void SetReorderEnabled(bool enabled);
    [[nodiscard]] bool ReorderEnabled() const;
    void MoveItem(int from, int insertionIndex, bool notify = true);

    void SetRowHeight(int pixels);
    [[nodiscard]] int RowHeight() const;
    void SetScrollPosition(int pixels);
    [[nodiscard]] int ScrollPosition() const;
    void EnsureVisible(int index);
    [[nodiscard]] core::Rect RowRect(int index) const;
    [[nodiscard]] int IndexFromPoint(core::Point point) const;
    /** 设置所有条目文字前共用的图标；传入空指针时清除图标。 */
    void SetLeadingIcon(std::shared_ptr<const render::DuiImage> icon);
    /** 返回所有条目当前共用的前置图标。 */
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& LeadingIcon() const;
    /** 设置条目前置图标的逻辑像素尺寸。 */
    void SetLeadingIconSize(core::Size size);
    /** 返回条目前置图标的逻辑像素尺寸。 */
    [[nodiscard]] core::Size LeadingIconSize() const;
    /** 设置条目前置图标与文字之间的逻辑像素间距。 */
    void SetLeadingIconGap(int pixels);
    /** 返回条目前置图标与文字之间的逻辑像素间距。 */
    [[nodiscard]] int LeadingIconGap() const;
    void SetTextStyle(render::DuiTextStyle style);
    void SetBackgroundColor(core::Color color);
    void SetTextColor(core::Color color);
    void SetSelectedColor(core::Color color);
    void SetSelectedTextColor(core::Color color);
    void SetHoverColor(core::Color color);
    void SetSelectionChangedHandler(std::function<void(int)> handler);
    void SetCheckChangedHandler(std::function<void(int, bool)> handler);
    void SetReorderedHandler(std::function<void(int)> handler);
    void SetItemClickedHandler(std::function<void(int, bool)> handler);
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    void UpdateScrollRange();
    [[nodiscard]] int VisibleRows() const;
    [[nodiscard]] int BodyWidth() const;

    class Impl;
    std::unique_ptr<Impl> listBox_;
};

} // namespace ysDui::controls::list
