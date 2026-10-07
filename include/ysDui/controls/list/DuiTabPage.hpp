#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/controls/list/DuiTab.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::list {

class DuiTabPage final : public core::Control, public render::DuiRenderable {
public:
    DuiTabPage();
    ~DuiTabPage() override;
    DuiTabPage(const DuiTabPage&) = delete;
    DuiTabPage& operator=(const DuiTabPage&) = delete;
    DuiTabPage(DuiTabPage&&) = delete;
    DuiTabPage& operator=(DuiTabPage&&) = delete;

    void SetHeaderHeight(int pixels);
    [[nodiscard]] int HeaderHeight() const;
    void SetIconSize(int pixels);
    void SetIconGap(int pixels);
    void SetAutoFitTabWidth(bool enabled);
    int AddPage(std::string title, std::unique_ptr<core::Control> page,
                std::shared_ptr<const render::DuiImage> icon = {});
    void RemovePage(int index);
    void SetPage(int index, std::unique_ptr<core::Control> page);
    /**
     * 摘出指定页并交出所有权，不销毁页面控件。
     * 用途：页面需要在容器之间搬迁时（如停靠布局重建），先摘出再挂到新容器。
     * @param index 页下标
     * @return 页控件所有权；下标无效或该页为空时返回空指针
     */
    [[nodiscard]] std::unique_ptr<core::Control> ReleasePage(int index);
    [[nodiscard]] core::Control* PageAt(int index) const;
    [[nodiscard]] int PageCount() const;
    void SetPageTitle(int index, std::string title);
    [[nodiscard]] std::string PageTitle(int index) const;
    void SetPageIcon(int index, std::shared_ptr<const render::DuiImage> icon);
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& PageIcon(int index) const;
    void SetSelectedIndex(int index, bool notify = true);
    [[nodiscard]] int SelectedIndex() const;
    void SetSelectionChangedHandler(std::function<void(int)> handler);
    [[nodiscard]] DuiTab& Header();
    [[nodiscard]] const DuiTab& Header() const;
    [[nodiscard]] core::Rect HeaderRect() const;
    [[nodiscard]] core::Rect ContentRect() const;
    void Layout(core::Rect bounds);
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    void ApplyVisibility();
    void LayoutContent();

    class Impl;
    std::unique_ptr<Impl> tabPage_;
};

} // namespace ysDui::controls::list
