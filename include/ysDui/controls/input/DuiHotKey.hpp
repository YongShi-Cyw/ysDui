#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::input {

class DuiHotKey final : public core::Control, public render::DuiRenderable {
public:
    DuiHotKey();
    ~DuiHotKey() override;
    DuiHotKey(const DuiHotKey&) = delete;
    DuiHotKey& operator=(const DuiHotKey&) = delete;
    DuiHotKey(DuiHotKey&&) noexcept;
    DuiHotKey& operator=(DuiHotKey&&) noexcept;

    void SetHotKey(unsigned int key, unsigned int modifiers, bool notify = false);
    void Clear(bool notify = false);
    [[nodiscard]] unsigned int Key() const;
    [[nodiscard]] unsigned int Modifiers() const;
    [[nodiscard]] bool Empty() const;
    [[nodiscard]] static std::string FormatHotKey(unsigned int key, unsigned int modifiers);
    [[nodiscard]] std::string DisplayText() const;
    void SetValueChangedHandler(std::function<void(unsigned int, unsigned int)> handler);
    void Layout(core::Rect bounds);
    [[nodiscard]] core::Size DesiredSize() const override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> hotKey_;
};

} // namespace ysDui::controls::input
