#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"

namespace ysDui::controls::basic {

struct DuiToolBarCommand final {
    std::uint32_t id{};
    std::string text;
};

class DuiToolBar final : public core::Control, public render::DuiRenderable {
public:
    DuiToolBar();
    ~DuiToolBar() override;
    DuiToolBar(const DuiToolBar&) = delete;
    DuiToolBar& operator=(const DuiToolBar&) = delete;
    DuiToolBar(DuiToolBar&&) noexcept;
    DuiToolBar& operator=(DuiToolBar&&) noexcept;

    void AddButton(std::string text, std::uint32_t id);
    void AddSeparator();
    void AddStretch();
    void ClearItems();
    [[nodiscard]] int ItemCount() const;
    void SetTextMeasurer(render::DuiTextMeasurer* measurer);
    void SetTextStyle(render::DuiTextStyle style);
    void SetCommandHandler(std::function<void(std::uint32_t)> handler);
    void SetOverflowRequestedHandler(std::function<void(std::vector<DuiToolBarCommand>)> handler);
    [[nodiscard]] bool HasOverflow() const;
    [[nodiscard]] core::Rect OverflowRect() const;
    [[nodiscard]] core::Rect ItemRect(int index) const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> toolBar_;
};

} // namespace ysDui::controls::basic
