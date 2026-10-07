#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"

namespace ysDui::controls::list {

class DuiTab final : public core::Control, public render::DuiRenderable {
public:
    DuiTab();
    ~DuiTab() override;
    DuiTab(const DuiTab&) = delete;
    DuiTab& operator=(const DuiTab&) = delete;
    DuiTab(DuiTab&&) noexcept;
    DuiTab& operator=(DuiTab&&) noexcept;

    int AddTab(std::string text, bool closeable = false, bool dropdown = false,
               std::uintptr_t value = 0, std::shared_ptr<const render::DuiImage> icon = {});
    void InsertTab(int index, std::string text, bool closeable = false, bool dropdown = false,
                   std::uintptr_t value = 0, std::shared_ptr<const render::DuiImage> icon = {});
    void RemoveTab(int index);
    void ClearTabs();
    [[nodiscard]] int Count() const;
    [[nodiscard]] std::string TextAt(int index) const;
    void SetTextAt(int index, std::string text);
    [[nodiscard]] std::uintptr_t ValueAt(int index) const;
    void SetValueAt(int index, std::uintptr_t value);
    [[nodiscard]] int FindByValue(std::uintptr_t value) const;
    void SetCloseable(int index, bool closeable);
    [[nodiscard]] bool Closeable(int index) const;
    void SetDropdown(int index, bool dropdown);
    [[nodiscard]] bool Dropdown(int index) const;
    void SetIcon(int index, std::shared_ptr<const render::DuiImage> icon);
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& IconAt(int index) const;

    void SetSelectedIndex(int index, bool notify = true);
    [[nodiscard]] int SelectedIndex() const;
    void SetSelectionChangedHandler(std::function<void(int)> handler);
    void SetCloseHandler(std::function<void(int)> handler);
    void SetDropdownHandler(std::function<void(int)> handler);
    void SetReorderedHandler(std::function<void(int, int)> handler);

    void SetTextMeasurer(render::DuiTextMeasurer* measurer);
    void SetTextStyle(render::DuiTextStyle style);
    void SetTabHeight(int pixels);
    [[nodiscard]] int TabHeight() const;
    void SetMinTabWidth(int pixels);
    void SetMaxTabWidth(int pixels);
    void SetTabPadding(int pixels);
    void SetGap(int pixels);
    void SetIconSize(int pixels);
    void SetIconGap(int pixels);
    void SetAutoFitTabWidth(bool enabled);
    void SetWheelSelect(bool enabled);
    void SetReorderEnabled(bool enabled);
    void SetScrollStepPixels(int pixels);
    void SetScrollOffset(int pixels);
    [[nodiscard]] int ScrollOffset() const;
    [[nodiscard]] int MaxScrollOffset() const;
    [[nodiscard]] bool NeedsScroll() const;
    [[nodiscard]] bool CanScrollLeft() const;
    [[nodiscard]] bool CanScrollRight() const;
    void EnsureVisible(int index);
    void MoveTab(int from, int insertionIndex);

    void SetBackgroundColor(core::Color color);
    void SetTabColor(core::Color color);
    void SetHoverColor(core::Color color);
    void SetSelectedColor(core::Color color);
    void SetTextColor(core::Color color);
    void SetSelectedTextColor(core::Color color);
    void SetBorderColor(core::Color color);
    [[nodiscard]] core::Rect TabRect(int index) const;
    [[nodiscard]] core::Rect CloseRect(int index) const;
    [[nodiscard]] core::Rect DropdownRect(int index) const;
    [[nodiscard]] core::Rect LeftArrowRect() const;
    [[nodiscard]] core::Rect RightArrowRect() const;
    [[nodiscard]] int HitTestIndex(core::Point point) const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    class Impl;
    std::unique_ptr<Impl> tab_;
};

} // namespace ysDui::controls::list
