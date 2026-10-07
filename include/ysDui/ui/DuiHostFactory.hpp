#pragma once

#include <memory>

#include "ysDui/ui/DuiNativeViewHost.hpp"
#include "ysDui/ui/DuiEmbeddedHost.hpp"
#include "ysDui/ui/DuiFrameHost.hpp"
#include "ysDui/ui/DuiLayeredHost.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"

namespace ysDui::ui {

class IUiHostFactory {
public:
    virtual ~IUiHostFactory() = default;
    [[nodiscard]] virtual std::unique_ptr<IEmbeddedHost> CreateEmbeddedHost(HostRef parent) = 0;
    [[nodiscard]] virtual std::unique_ptr<IFrameHost> CreateFrameHost() = 0;
    [[nodiscard]] virtual std::unique_ptr<DuiNativeViewHost> CreateNativeViewHost(HostRef parent) = 0;
    [[nodiscard]] virtual std::unique_ptr<IPopupHost> CreatePopupHost(HostRef owner) = 0;
    [[nodiscard]] virtual std::unique_ptr<ILayeredHost> CreateLayeredHost(HostRef owner) = 0;
};

} // namespace ysDui::ui
