#include "ysDui/core/DuiControl.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::core {

class Control::Impl {
public:
    std::shared_ptr<int> lifetime{std::make_shared<int>()};
    Rect bounds;
    Control* parent{};
    const DuiTheme* explicitTheme{};
    const DuiTheme* inheritedTheme{};
    bool visible{true};
    bool enabled{true};
    bool focused{};
    bool hovered{};
    bool captured{};
    bool layoutDirty{};
    bool inLayout{};
    DuiPointerCursor pointerCursor{DuiPointerCursor::Arrow};
    std::string name;
    std::string accessibilityName;
    std::string accessibilityDescription;
    std::string accessibilityIdentifier;
    std::vector<std::unique_ptr<Control>> children;
};

Control::Control() : impl_(std::make_unique<Impl>()) {}
Control::~Control() = default;
Control::Control(Control&& other) noexcept : impl_(std::move(other.impl_)) {
    if (impl_) {
        impl_->parent = nullptr;
        for (const auto& child : impl_->children) child->impl_->parent = this;
    }
}
Control& Control::operator=(Control&& other) noexcept {
    if (this == &other) return *this;
    Control* parent = impl_ ? impl_->parent : nullptr;
    impl_ = std::move(other.impl_);
    if (impl_) {
        impl_->parent = parent;
        for (const auto& child : impl_->children) child->impl_->parent = this;
    }
    return *this;
}

void Control::SetBounds(Rect bounds) { impl_->bounds = bounds; }
Rect Control::Bounds() const { return impl_->bounds; }
void Control::Layout(Rect bounds)
{
    SetBounds(bounds);
    impl_->layoutDirty = false;
}
void Control::InvalidateLayout()
{
    if (!impl_ || impl_->inLayout)
        return;
    for (Control* node = this; node != nullptr; node = node->impl_->parent)
    {
        if (node->impl_->inLayout)
            return;
    }
    for (Control* node = this; node != nullptr; node = node->impl_->parent)
        node->impl_->layoutDirty = true;
}
bool Control::LayoutDirty() const { return impl_->layoutDirty; }
void Control::PerformLayout()
{
    if (!impl_->layoutDirty || impl_->inLayout)
        return;
    impl_->inLayout = true;
    impl_->layoutDirty = false;
    Layout(Bounds());
    impl_->inLayout = false;
}
Size Control::DesiredSize() const { return {}; }
void Control::SetVisible(bool visible)
{
    if (impl_->visible == visible)
        return;
    impl_->visible = visible;
    InvalidateLayout();
}
bool Control::Visible() const { return impl_->visible; }
bool Control::EffectivelyVisible() const {
    for (const Control* current = this; current != nullptr; current = current->impl_->parent) {
        if (!current->Visible()) { return false; }
    }
    return true;
}
void Control::SetEnabled(bool enabled) { impl_->enabled = enabled; }
bool Control::Enabled() const {
    // 沿父链求值：任一祖先被禁用则自身视为禁用（与 EffectivelyVisible 同构）。
    // 容器因此可以整体停用子树：子控件无需感知，会在绘制上取禁用配色、在命中测试上直接落空。
    for (const Control* current = this; current != nullptr; current = current->impl_->parent) {
        if (!current->impl_->enabled) { return false; }
    }
    return true;
}
void Control::SetFocused(bool focused) { impl_->focused = focused; }
bool Control::Focused() const { return impl_->focused; }
void Control::SetHovered(bool hovered) { impl_->hovered = hovered; }
bool Control::Hovered() const { return impl_->hovered; }
void Control::SetCaptured(bool captured) { impl_->captured = captured; }
bool Control::Captured() const { return impl_->captured; }
VisualState Control::GetVisualState() const {
    if (!Enabled()) { return VisualState::Disabled; }
    if (Captured() && Hovered()) { return VisualState::Active; }
    if (Hovered()) { return VisualState::Hover; }
    if (Focused()) { return VisualState::Focused; }
    return VisualState::Normal;
}
void Control::SetName(std::string name) { impl_->name = std::move(name); }
const std::string& Control::Name() const { return impl_->name; }
void Control::SetAccessibilityName(std::string name) { impl_->accessibilityName = std::move(name); }
void Control::SetAccessibilityDescription(std::string description)
{
    impl_->accessibilityDescription = std::move(description);
}
void Control::SetAccessibilityIdentifier(std::string identifier)
{
    impl_->accessibilityIdentifier = std::move(identifier);
}
DuiAccessibilityData Control::Accessibility() const
{
    const std::weak_ptr<int> lifetime = impl_->lifetime;
    DuiAccessibilityData data = CreateAccessibilityData();
    if (lifetime.expired())
        return data;
    if (!impl_->accessibilityName.empty())
        data.name = impl_->accessibilityName;
    if (!impl_->accessibilityDescription.empty())
        data.description = impl_->accessibilityDescription;
    data.identifier = impl_->accessibilityIdentifier;
    return data;
}
Control* Control::Parent() const { return impl_->parent; }
void Control::SetTheme(const DuiTheme* theme)
{
    impl_->explicitTheme = theme;
    const DuiTheme* inherited = &Theme();
    for (const auto& child : impl_->children)
        child->SetInheritedTheme(inherited);
}
const DuiTheme& Control::Theme() const
{
    if (impl_->explicitTheme != nullptr)
        return *impl_->explicitTheme;
    if (impl_->inheritedTheme != nullptr)
        return *impl_->inheritedTheme;
    static const DuiTheme defaultTheme;
    return defaultTheme;
}
void Control::SetInheritedTheme(const DuiTheme* theme)
{
    impl_->inheritedTheme = theme;
    const DuiTheme* inherited = &Theme();
    for (const auto& child : impl_->children)
        child->SetInheritedTheme(inherited);
}
void Control::AddChild(std::unique_ptr<Control> child) {
    if (child) {
        child->impl_->parent = this;
        child->SetInheritedTheme(&Theme());
        impl_->children.push_back(std::move(child));
        InvalidateLayout();
    }
}
std::unique_ptr<Control> Control::RemoveChild(Control* child) {
    const auto iterator = std::find_if(impl_->children.begin(), impl_->children.end(),
        [child](const std::unique_ptr<Control>& candidate) { return candidate.get() == child; });
    if (iterator == impl_->children.end()) return {};
    (*iterator)->impl_->parent = nullptr;
    (*iterator)->SetInheritedTheme(nullptr);
    std::unique_ptr<Control> result = std::move(*iterator);
    impl_->children.erase(iterator);
    InvalidateLayout();
    return result;
}
std::vector<std::unique_ptr<Control>>& Control::Children() { return impl_->children; }
const std::vector<std::unique_ptr<Control>>& Control::Children() const { return impl_->children; }
void Control::SetPointerCursor(DuiPointerCursor cursor) { impl_->pointerCursor = cursor; }
DuiPointerCursor Control::PointerCursor() const { return impl_->pointerCursor; }

Control* Control::HitTest(Point point) {
    if (!EffectivelyVisible() || !Enabled() || !Bounds().Contains(point)) { return nullptr; }
    for (auto it = impl_->children.rbegin(); it != impl_->children.rend(); ++it) {
        if (Control* hit = (*it)->HitTest(point)) { return hit; }
    }
    return this;
}

DuiAccessibilityData Control::CreateAccessibilityData() const { return {}; }
bool Control::PerformAccessibilityAction(DuiAccessibilityAction, std::string_view) { return false; }
bool Control::OnEvent(const Event&) { return false; }

} // namespace ysDui::core
