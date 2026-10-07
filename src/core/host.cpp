#include "ysDui/core/DuiHost.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace ysDui::core {
namespace {
bool ClearHoveredControl(Control& control, const Control* hovered)
{
    if (&control == hovered)
    {
        control.SetHovered(false);
        return true;
    }
    for (const auto& child : control.Children())
    {
        if (ClearHoveredControl(*child, hovered))
            return true;
    }
    return false;
}

bool ContainsControl(const Control* root, const Control* candidate)
{
    if (root == nullptr || candidate == nullptr)
        return false;
    if (root == candidate)
        return true;
    for (const auto& child : root->Children())
        if (ContainsControl(child.get(), candidate))
            return true;
    return false;
}

void CollectFocusableControls(Control& control, std::vector<Control*>& controls)
{
    // Enabled() 已沿父链求值，禁用容器下的后代不会被收集
    if (control.EffectivelyVisible() && control.Enabled()
        && control.Accessibility().keyboardFocusable)
    {
        controls.push_back(&control);
    }
    for (const auto& child : control.Children())
        CollectFocusableControls(*child, controls);
}
}

class Host::Impl {
public:
    Impl()
    {
        themeSubscription = theme.SubscribeScoped([this]
        {
            if (onThemeChanged)
                onThemeChanged();
        });
    }

    std::unique_ptr<Control> root;
    Control* captured{};
    Control* focused{};
    Control* hovered{};
    PointerHoverHandlerId nextPointerHoverHandler{};
    PointerHoverHandlerId pointerHoverHandler{};
    std::function<void(Control*, Point)> onPointerHover;
    AnimationClock clock;
    DuiUiDispatcher dispatcher;
    DuiDpiScale dpiScale;
    DuiTheme theme;
    DuiSubscription themeSubscription;
    std::function<void()> onThemeChanged;

    bool MoveFocus(bool reverse)
    {
        if (!root) return false;
        std::vector<Control*> controls;
        CollectFocusableControls(*root, controls);
        if (controls.empty()) return false;

        const auto current = std::find(controls.begin(), controls.end(), focused);
        Control* next = nullptr;
        if (current == controls.end())
            next = reverse ? controls.back() : controls.front();
        else if (reverse)
            next = current == controls.begin() ? controls.back() : *std::prev(current);
        else
            next = std::next(current) == controls.end() ? controls.front() : *std::next(current);

        if (focused != next && ContainsControl(root.get(), focused))
            focused->SetFocused(false);
        focused = next;
        focused->SetFocused(true);
        return true;
    }
};

Host::Host() : impl_(std::make_unique<Impl>()) {}
Host::~Host() = default;
Host::Host(Host&&) noexcept = default;
Host& Host::operator=(Host&&) noexcept = default;
std::unique_ptr<Control> Host::ReleaseRoot()
{
    if (impl_->hovered != nullptr && impl_->onPointerHover)
        impl_->onPointerHover(nullptr, {});
    if (ContainsControl(impl_->root.get(), impl_->focused))
        impl_->focused->SetFocused(false);
    impl_->captured = nullptr;
    impl_->focused = nullptr;
    impl_->hovered = nullptr;
    std::unique_ptr<Control> released = std::move(impl_->root);
    if (released != nullptr)
        released->SetInheritedTheme(nullptr);
    return released;
}

void Host::SetRoot(std::unique_ptr<Control> root) {
    if (impl_->hovered != nullptr && impl_->onPointerHover)
        impl_->onPointerHover(nullptr, {});
    if (ContainsControl(impl_->root.get(), impl_->focused))
        impl_->focused->SetFocused(false);
    impl_->captured = nullptr;
    impl_->focused = nullptr;
    impl_->hovered = nullptr;
    impl_->root = std::move(root);
    if (impl_->root != nullptr)
    {
        impl_->root->SetInheritedTheme(&impl_->theme);
        impl_->root->InvalidateLayout();
    }
}
Control* Host::Root() const { return impl_->root.get(); }
Control* Host::HitTest(Point point) const { return impl_->root ? impl_->root->HitTest(point) : nullptr; }
Control* Host::FocusedControl() const
{
    return ContainsControl(impl_->root.get(), impl_->focused) ? impl_->focused : nullptr;
}
Control* Host::CapturedControl() const
{
    return ContainsControl(impl_->root.get(), impl_->captured) ? impl_->captured : nullptr;
}
void Host::SetFocusedControl(Control* control) {
    if (control != nullptr && !ContainsControl(impl_->root.get(), control)) return;
    if (impl_->focused == control) return;
    if (ContainsControl(impl_->root.get(), impl_->focused)) impl_->focused->SetFocused(false);
    impl_->focused = control;
    if (impl_->focused) impl_->focused->SetFocused(true);
}
Host::PointerHoverHandlerId Host::SetPointerHoverHandler(std::function<void(Control*, Point)> handler) {
    impl_->onPointerHover = std::move(handler);
    impl_->pointerHoverHandler = impl_->onPointerHover ? ++impl_->nextPointerHoverHandler : 0;
    impl_->hovered = nullptr;
    return impl_->pointerHoverHandler;
}
void Host::RemovePointerHoverHandler(PointerHoverHandlerId handler) {
    if (handler != 0 && handler == impl_->pointerHoverHandler) {
        impl_->onPointerHover = {};
        impl_->pointerHoverHandler = 0;
        impl_->hovered = nullptr;
    }
}
AnimationClock& Host::Clock() { return impl_->clock; }
DuiUiDispatcher& Host::Dispatcher() { return impl_->dispatcher; }
void Host::SetDpiScale(DuiDpiScale scale) { impl_->dpiScale = scale; }
DuiDpiScale Host::DpiScale() const { return impl_->dpiScale; }
DuiTheme& Host::Theme() { return impl_->theme; }
const DuiTheme& Host::Theme() const { return impl_->theme; }
void Host::SetThemeChangedHandler(std::function<void()> handler)
{
    impl_->onThemeChanged = std::move(handler);
}

void Host::PrepareFrame()
{
    // 先应用跨线程投递的任务，再做布局：两者同帧完成，避免先画一帧旧布局
    impl_->dispatcher.Drain();
    if (impl_->root)
        impl_->root->PerformLayout();
}

bool Host::Dispatch(const Event& event) {
    if (!impl_->root) { return false; }
    if (!ContainsControl(impl_->root.get(), impl_->captured)) impl_->captured = nullptr;
    if (!ContainsControl(impl_->root.get(), impl_->focused)) impl_->focused = nullptr;
    if (!ContainsControl(impl_->root.get(), impl_->hovered)) impl_->hovered = nullptr;
    if (event.type == EventType::KeyDown && event.key == key::Tab
        && impl_->MoveFocus((event.modifiers & modifier::Shift) != 0U))
    {
        return true;
    }
    Control* pointerTarget{};
    if (event.type == EventType::PointerMove) {
        Control* hovered = impl_->root->HitTest(event.position);
        if (hovered != impl_->hovered) {
            if (impl_->hovered != nullptr)
            {
                Control* previous = impl_->hovered;
                (void)ClearHoveredControl(*impl_->root, previous);
                // 通知旧悬停控件清理内部悬停态（如按钮高亮下标）。
                Event leave{};
                leave.type = EventType::PointerLeave;
                leave.position = event.position;
                previous->OnEvent(leave);
            }
            impl_->hovered = hovered;
            if (hovered != nullptr)
                hovered->SetHovered(true);
            if (impl_->onPointerHover) impl_->onPointerHover(hovered, event.position);
        }
    } else if (event.type == EventType::PointerLeave) {
        pointerTarget = impl_->captured != nullptr ? impl_->captured : impl_->hovered;
        if (impl_->hovered != nullptr)
            (void)ClearHoveredControl(*impl_->root, impl_->hovered);
        impl_->hovered = nullptr;
        if (impl_->onPointerHover) impl_->onPointerHover(nullptr, event.position);
    } else if (event.type == EventType::PointerDown && impl_->hovered != nullptr) {
        Control* previous = impl_->hovered;
        (void)ClearHoveredControl(*impl_->root, previous);
        Event leave{};
        leave.type = EventType::PointerLeave;
        leave.position = event.position;
        previous->OnEvent(leave);
        impl_->hovered = nullptr;
        if (impl_->onPointerHover) impl_->onPointerHover(nullptr, event.position);
    }

    Control* target = nullptr;
    if (event.type == EventType::DpiChanged) {
        target = impl_->root.get();
    } else if (event.type == EventType::KeyDown || event.type == EventType::KeyUp
        || event.type == EventType::TextInput || event.type == EventType::CompositionStart
        || event.type == EventType::CompositionUpdate || event.type == EventType::CompositionEnd
        || event.type == EventType::FocusGained || event.type == EventType::FocusLost) {
        target = impl_->focused ? impl_->focused : impl_->root.get();
    } else if (pointerTarget != nullptr) {
        target = pointerTarget;
    } else {
        target = impl_->captured;
        if (target == nullptr) { target = impl_->root->HitTest(event.position); }
    }
    if (target == nullptr) { return false; }
    if (event.type == EventType::PointerDown && impl_->focused != target) {
        SetFocusedControl(target);
    }
    bool handled{};
    std::vector<Control*> eventPath;
    for (Control* current = target; current != nullptr; current = current->Parent())
        eventPath.push_back(current);
    for (Control* current : eventPath) {
        if (!ContainsControl(impl_->root.get(), current)) break;
        if (current->OnEvent(event)) { handled = true; break; }
    }
    if (event.type == EventType::PointerDown && ContainsControl(impl_->root.get(), target) && target->Captured()) {
        impl_->captured = target;
    }
    if (event.type == EventType::PointerUp && ContainsControl(impl_->root.get(), impl_->captured)
        && !impl_->captured->Captured()) {
        impl_->captured = nullptr;
    }
    if (event.type == EventType::PointerCancel && ContainsControl(impl_->root.get(), impl_->captured)) {
        impl_->captured->SetCaptured(false);
        impl_->captured = nullptr;
    }
    return handled;
}

} // namespace ysDui::core
