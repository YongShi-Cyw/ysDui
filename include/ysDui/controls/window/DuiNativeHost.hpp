#pragma once

#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiNativeViewHost.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::window {

class DuiNativeHost final : public core::Control, public render::DuiRenderable {
public:
    DuiNativeHost();
    ~DuiNativeHost() override;
    DuiNativeHost(const DuiNativeHost&) = delete;
    DuiNativeHost& operator=(const DuiNativeHost&) = delete;
    void SetNativeViewHost(ui::DuiNativeViewHost* host);
    bool SetChild(ui::NativeViewRef child, ui::DuiNativeViewOptions options = {});
    [[nodiscard]] ui::NativeViewRef DetachChild();
    void SetPreferredSize(core::Size size);
    [[nodiscard]] core::Size PreferredSize() const;
    void SetShowPlaceholder(bool value);
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    void SetVisible(bool visible);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;
private:
    class Impl;
    std::unique_ptr<Impl> nativeHost_;
};

} // namespace ysDui::controls::window
