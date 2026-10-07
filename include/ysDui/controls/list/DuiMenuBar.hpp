#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiHostRef.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"

namespace ysDui::controls::list {
class DuiMenu;
}

namespace ysDui::ui {
class IUiHostFactory;
}

namespace ysDui::controls::list {

struct DuiMenuBarItem final {
    std::uint32_t id{};
    std::string text;
    bool enabled{true};
};

class DuiMenuBar final : public core::Control, public render::DuiRenderable {
public:
    DuiMenuBar();
    ~DuiMenuBar() override;
    DuiMenuBar(const DuiMenuBar&) = delete;
    DuiMenuBar& operator=(const DuiMenuBar&) = delete;
    DuiMenuBar(DuiMenuBar&&) noexcept;
    DuiMenuBar& operator=(DuiMenuBar&&) noexcept;

    int AddItem(std::uint32_t id, std::string text, DuiMenu* dropdown = nullptr);
    void RemoveItem(int index);
    void ClearItems();
    [[nodiscard]] int ItemCount() const;
    [[nodiscard]] DuiMenuBarItem ItemAt(int index) const;
    void SetItemEnabled(std::uint32_t id, bool enabled);
    [[nodiscard]] bool ItemEnabled(std::uint32_t id) const;
    void SetDropdown(int index, DuiMenu* dropdown);
    [[nodiscard]] DuiMenu* Dropdown(int index) const;
    void SetItemHeight(int pixels);
    [[nodiscard]] int ItemHeight() const;

    void SetPopupContext(ui::IUiHostFactory& factory, ui::HostRef owner);
    void SetTextMeasurer(render::DuiTextMeasurer* measurer);
    void SetTextStyle(render::DuiTextStyle style);
    void SetCommandHandler(std::function<void(std::uint32_t)> handler);
    void SetDropdownChangedHandler(std::function<void(int, bool)> handler);
    [[nodiscard]] int ActiveIndex() const;
    [[nodiscard]] int HoveredIndex() const;
    [[nodiscard]] core::Rect ItemRect(int index) const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    bool ProcessMnemonic(char character);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> menuBar_;
};

} // namespace ysDui::controls::list
