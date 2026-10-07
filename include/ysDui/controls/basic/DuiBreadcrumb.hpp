#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::basic {

class DuiBreadcrumb final : public core::Control, public render::DuiRenderable {
public:
    DuiBreadcrumb();
    ~DuiBreadcrumb() override;
    DuiBreadcrumb(const DuiBreadcrumb&) = delete;
    DuiBreadcrumb& operator=(const DuiBreadcrumb&) = delete;
    DuiBreadcrumb(DuiBreadcrumb&&) noexcept;
    DuiBreadcrumb& operator=(DuiBreadcrumb&&) noexcept;
    void SetItems(std::vector<std::string> items);
    void AddItem(std::string text);
    void ClearItems();
    [[nodiscard]] const std::vector<std::string>& Items() const;
    void SetItemClickedHandler(std::function<void(int)> handler);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;
private:
    class Impl;
    std::unique_ptr<Impl> breadcrumb_;
};

} // namespace ysDui::controls::basic
