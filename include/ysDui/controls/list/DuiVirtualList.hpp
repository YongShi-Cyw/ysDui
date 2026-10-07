#pragma once

#include <functional>
#include <memory>

#include "ysDui/controls/input/DuiScrollBar.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::list {

class DuiVirtualList final : public core::Control, public render::DuiRenderable {
public:
    using RowRenderer = std::function<void(render::Canvas&, int, core::Rect, bool, bool)>;
    using RowClickHandler = std::function<void(int, bool)>;

    DuiVirtualList();
    ~DuiVirtualList() override;
    DuiVirtualList(const DuiVirtualList&) = delete;
    DuiVirtualList& operator=(const DuiVirtualList&) = delete;
    DuiVirtualList(DuiVirtualList&&) = delete;
    DuiVirtualList& operator=(DuiVirtualList&&) = delete;

    void SetRowCount(int count);
    [[nodiscard]] int RowCount() const;
    void SetRowHeight(int pixels);
    [[nodiscard]] int RowHeight() const;
    void SetRowRenderer(RowRenderer renderer);
    void SetRowClickHandler(RowClickHandler handler);
    void SetSelectionChangedHandler(std::function<void(int)> handler);
    void SetSelectedIndex(int index, bool notify = true);
    [[nodiscard]] int SelectedIndex() const;
    void SetScrollPosition(int pixels);
    [[nodiscard]] int ScrollPosition() const;
    void EnsureVisible(int index);
    [[nodiscard]] core::Rect RowRect(int index) const;
    [[nodiscard]] int IndexFromPoint(core::Point point) const;
    void SetBackgroundColor(core::Color color);
    void SetSelectedColor(core::Color color);
    void SetHoverColor(core::Color color);
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    void UpdateScrollRange();
    [[nodiscard]] int VisibleRows() const;
    [[nodiscard]] int BodyWidth() const;

    class Impl;
    std::unique_ptr<Impl> virtualList_;
};

} // namespace ysDui::controls::list
