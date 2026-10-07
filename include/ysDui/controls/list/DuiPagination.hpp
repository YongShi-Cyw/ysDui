/**
 * 文件名：DuiPagination.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-31
 * 用途：提供页码、上一页和下一页切换控件。
 */
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::list {

class DuiPagination final : public core::Control, public render::DuiRenderable {
public:
    DuiPagination();
    ~DuiPagination() override;
    DuiPagination(const DuiPagination&) = delete;
    DuiPagination& operator=(const DuiPagination&) = delete;
    DuiPagination(DuiPagination&&) noexcept;
    DuiPagination& operator=(DuiPagination&&) noexcept;

    void SetPageCount(int count);
    [[nodiscard]] int PageCount() const;
    void SetCurrentPage(int page, bool notify = false);
    [[nodiscard]] int CurrentPage() const;
    void SetMaxVisiblePages(int count);
    [[nodiscard]] int MaxVisiblePages() const;
    void SetPageChangedHandler(std::function<void(int)> handler);
    [[nodiscard]] core::Rect PageRect(int page) const;
    [[nodiscard]] core::Rect PreviousRect() const;
    [[nodiscard]] core::Rect NextRect() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    void RebuildVisiblePages();

    class Impl;
    std::unique_ptr<Impl> pagination_;
};

} // namespace ysDui::controls::list
