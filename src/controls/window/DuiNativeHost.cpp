#include "ysDui/controls/window/DuiNativeHost.hpp"

#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::window {
class DuiNativeHost::Impl {
public:
    ui::DuiNativeViewHost* host{};
    core::Size preferred{200, 120};
    bool placeholder{true};
};

DuiNativeHost::DuiNativeHost() : nativeHost_(std::make_unique<Impl>()) {}
DuiNativeHost::~DuiNativeHost() { static_cast<void>(DetachChild()); }
void DuiNativeHost::SetNativeViewHost(ui::DuiNativeViewHost* host)
{
    if (nativeHost_->host == host) return;
    if (nativeHost_->host) static_cast<void>(nativeHost_->host->Detach());
    nativeHost_->host = host;
    if (host) { host->SetBounds(Bounds()); host->SetVisible(Visible()); }
}
bool DuiNativeHost::SetChild(ui::NativeViewRef child, ui::DuiNativeViewOptions options)
{
    return nativeHost_->host && child.Valid() && nativeHost_->host->Attach(std::move(child), options);
}
ui::NativeViewRef DuiNativeHost::DetachChild()
{
    return nativeHost_->host ? nativeHost_->host->Detach() : ui::NativeViewRef{};
}
void DuiNativeHost::SetPreferredSize(core::Size size) { nativeHost_->preferred = {std::max(0, size.width), std::max(0, size.height)}; }
core::Size DuiNativeHost::PreferredSize() const { return nativeHost_->preferred; }
void DuiNativeHost::SetShowPlaceholder(bool value) { nativeHost_->placeholder = value; }
core::Size DuiNativeHost::DesiredSize() const { return nativeHost_->preferred; }
void DuiNativeHost::Layout(core::Rect bounds) { SetBounds(bounds); if (nativeHost_->host) nativeHost_->host->SetBounds(bounds); }
void DuiNativeHost::SetVisible(bool visible) { core::Control::SetVisible(visible); if (nativeHost_->host) nativeHost_->host->SetVisible(visible); }
bool DuiNativeHost::OnEvent(const core::Event&) { return false; }
void DuiNativeHost::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const auto bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty() || !nativeHost_->placeholder || (nativeHost_->host && nativeHost_->host->HasContent())) return;
    canvas.FillRect(bounds, Theme().Get(core::ThemeSlot::NativeHostBackground));
    canvas.StrokeRoundedRect(bounds, 0, Theme().Get(core::ThemeSlot::NativeHostBorder), 1.0F);
    render::DuiTextStyle style;
    style.color = Theme().Get(core::ThemeSlot::NativeHostText);
    canvas.DrawText("NativeHost", bounds, style, render::DuiTextAlignment::Center, false);
}
} // namespace ysDui::controls::window
