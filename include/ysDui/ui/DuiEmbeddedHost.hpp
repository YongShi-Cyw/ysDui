#pragma once

#include "ysDui/ui/DuiPopupHost.hpp"

namespace ysDui::ui {

struct DuiEmbeddedHostOptions final {
    core::Rect bounds;
    bool dismissOnFocusLost{};
};

class IEmbeddedHost {
public:
    virtual ~IEmbeddedHost() = default;
    virtual bool Show(const DuiEmbeddedHostOptions& options, std::unique_ptr<core::Control> content,
                      IPopupHost::LayoutHandler layout, IPopupHost::PaintHandler paint) = 0;
    virtual void Hide() = 0;
    virtual void RequestHide() = 0;
    virtual void SetBounds(core::Rect bounds) = 0;
    [[nodiscard]] virtual bool Visible() const = 0;
    /** @return 当前嵌入宿主的弱引用；宿主隐藏后引用失效。 */
    [[nodiscard]] virtual HostRef Reference() const = 0;
};

} // namespace ysDui::ui
