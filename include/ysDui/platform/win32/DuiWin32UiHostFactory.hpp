#pragma once

#include <memory>

#include "ysDui/ui/DuiHostFactory.hpp"

namespace ysDui::platform::win32 {

class Win32UiHostFactory final : public ui::IUiHostFactory {
public:
    Win32UiHostFactory() = default;
    ~Win32UiHostFactory() override = default;
    [[nodiscard]] std::unique_ptr<ui::IEmbeddedHost> CreateEmbeddedHost(ui::HostRef parent) override;
    [[nodiscard]] std::unique_ptr<ui::IFrameHost> CreateFrameHost() override;
    [[nodiscard]] std::unique_ptr<ui::DuiNativeViewHost> CreateNativeViewHost(ui::HostRef parent) override;
    [[nodiscard]] std::unique_ptr<ui::IPopupHost> CreatePopupHost(ui::HostRef owner) override;
    [[nodiscard]] std::unique_ptr<ui::ILayeredHost> CreateLayeredHost(ui::HostRef owner) override;
};

} // namespace ysDui::platform::win32
