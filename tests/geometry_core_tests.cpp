/**
 * 文件名：geometry_core_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：验证几何、DPI、事件、宿主、动画与无障碍核心逻辑。
 */
#include "test_support.hpp"

namespace {

class FocusableControl final : public ysDui::core::Control
{
protected:
    [[nodiscard]] ysDui::core::DuiAccessibilityData CreateAccessibilityData() const override
    {
        return {ysDui::core::DuiAccessibilityRole::Button, {}, {}, {}, true, {}};
    }
};

} // namespace

int main()
{
    using namespace ysDui::core;
    assert((Rect::Intersect({0, 0, 10, 10}, {5, 5, 15, 15}) == Rect{5, 5, 10, 10}));
    DuiDpiScale highDpi(144);
    assert(highDpi.Dpi() == 144);
    assert(highDpi.Scale(8) == 12);
    assert(highDpi.Unscale(12) == 8);
    assert((highDpi.Scale(Point{3, 5}) == Point{5, 8}));
    assert((highDpi.Scale(Size{101, 51}) == Size{152, 77}));
    assert((highDpi.Scale(Rect{1, 3, 102, 54}) == Rect{2, 5, 153, 81}));
    assert((highDpi.Unscale(Rect{2, 5, 153, 81}) == Rect{1, 3, 102, 54}));
    DuiDpiScale doubleDpi(192);
    assert((doubleDpi.Scale(Rect{-10, 4, 21, 16}) == Rect{-20, 8, 42, 32}));
    assert(DuiDpiScale(0).Dpi() == DuiDpiScale::DefaultDpi);
    Host dpiHost;
    dpiHost.SetDpiScale(highDpi);
    assert(dpiHost.DpiScale() == highDpi);
    ysDui::ui::DuiDropPayload files = ysDui::ui::DuiDropFiles{{"C:\\input.txt"}};
    assert(std::holds_alternative<ysDui::ui::DuiDropFiles>(files));
    assert(std::get<ysDui::ui::DuiDropFiles>(files).paths.front() == "C:\\input.txt");
    ysDui::ui::DuiDropPayload image = ysDui::ui::DuiDropImage{};
    assert(std::holds_alternative<ysDui::ui::DuiDropImage>(image));
    assert(ysDui::core::IsLeapYear(2024) && !ysDui::core::IsLeapYear(2100));
    assert(ysDui::core::DaysInMonth(2024, 2) == 29);
    assert(ysDui::core::IsValidDate(2024, 2, 29) && !ysDui::core::IsValidDate(2023, 2, 29));
    assert(ysDui::core::IsValidTime(23, 59, 59) && !ysDui::core::IsValidTime(24, 0, 0));
    assert(ysDui::core::mnemonic::FindCharacter("&Save") == 's');
    assert(ysDui::core::mnemonic::FindCharacter("Save && Quit") == '\0');
    assert(ysDui::core::mnemonic::StripPrefix("Save && &Close") == "Save & Close");
    AnimationClock clock;
    double animationProgress{};
    (void)clock.Schedule(100, [&animationProgress](double value) { animationProgress = value; });
    clock.Advance(40);
    assert(animationProgress == 0.4);
    const auto cancelled = clock.Schedule(10, [&animationProgress](double) { animationProgress = -1.0; });
    clock.Cancel(cancelled);
    clock.Advance(60);
    assert(animationProgress == 1.0);
    bool animationActive{};
    clock.SetActivityChangedHandler([&animationActive](bool active) { animationActive = active; });
    const auto activeTask = clock.Schedule(10, [](double) {});
    assert(animationActive && clock.HasScheduledTasks());
    clock.Cancel(activeTask);
    assert(!animationActive && !clock.HasScheduledTasks());

    auto root = std::make_unique<Control>();
    root->SetBounds({0, 0, 100, 100});
    auto child = std::make_unique<EventControl>();
    child->SetBounds({10, 10, 20, 20});
    EventControl* raw = child.get();
    root->AddChild(std::move(child));
    Host host;
    host.SetRoot(std::move(root));
    assert(host.HitTest({15, 15}) == raw);
    assert(host.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {15, 15})));
    assert(raw->received);
    assert(host.FocusedControl() == raw);
    host.SetFocusedControl(nullptr);
    assert(host.FocusedControl() == nullptr && !raw->Focused());

    auto capturing = std::make_unique<CapturingControl>();
    capturing->SetBounds({0, 0, 20, 20});
    CapturingControl* capturingRaw = capturing.get();
    Host captureHost;
    captureHost.SetRoot(std::move(capturing));
    assert(captureHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {5, 5})));
    assert(capturingRaw->Captured());
    assert(captureHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerCancel, {5, 5})));
    assert(capturingRaw->cancelled && !capturingRaw->Captured());

    OverlayControl overlay;
    overlay.SetBounds({0, 0, 100, 100});
    overlay.SetAlpha(-1.0);
    assert(overlay.Alpha() == 0.0 && overlay.HitTest({10, 10}) == nullptr);
    overlay.SetAlpha(2.0);
    assert(overlay.Alpha() == 1.0 && overlay.HitTest({10, 10}) == nullptr);

    auto accessibleRoot = std::make_unique<Control>();
    auto accessibleGroup = std::make_unique<AccessibleControl>(DuiAccessibilityRole::Group);
    auto accessibleButton = std::make_unique<AccessibleControl>(DuiAccessibilityRole::Button);
    auto accessibleText = std::make_unique<AccessibleControl>(DuiAccessibilityRole::Text);
    AccessibleControl* rawAccessibleGroup = accessibleGroup.get();
    AccessibleControl* rawAccessibleButton = accessibleButton.get();
    AccessibleControl* rawAccessibleText = accessibleText.get();
    accessibleGroup->AddChild(std::move(accessibleButton));
    accessibleRoot->AddChild(std::move(accessibleGroup));
    accessibleRoot->AddChild(std::move(accessibleText));
    assert(IsAccessibilityNodeExposed(accessibleRoot.get()));
    assert(EffectiveAccessibilityRole(accessibleRoot.get()) == DuiAccessibilityRole::Group);
    assert(AccessibilityFirstChild(accessibleRoot.get()) == rawAccessibleGroup);
    assert(AccessibilityLastChild(accessibleRoot.get()) == rawAccessibleText);
    assert(AccessibilityParent(rawAccessibleButton) == rawAccessibleGroup);
    assert(AccessibilityNextSibling(rawAccessibleGroup) == rawAccessibleText);
    assert(AccessibilityPreviousSibling(rawAccessibleText) == rawAccessibleGroup);
    assert(DeepestAccessibleDescendant(accessibleRoot.get()) == accessibleRoot.get());
    assert(DeepestAccessibleDescendant(rawAccessibleButton) == rawAccessibleButton);

    rawAccessibleButton->SetAccessibilityName("Save changes");
    rawAccessibleButton->SetAccessibilityDescription("Writes the form to disk");
    rawAccessibleButton->SetAccessibilityIdentifier("9101");
    const auto accessibilityOverride = rawAccessibleButton->Accessibility();
    assert(accessibilityOverride.role == DuiAccessibilityRole::Button);
    assert(accessibilityOverride.name == "Save changes");
    assert(accessibilityOverride.description == "Writes the form to disk");
    assert(accessibilityOverride.identifier == "9101");

    Control state;
    assert(state.GetVisualState() == VisualState::Normal);
    state.SetFocused(true);
    assert(state.GetVisualState() == VisualState::Focused);
    state.SetHovered(true);
    assert(state.GetVisualState() == VisualState::Hover);
    state.SetCaptured(true);
    assert(state.GetVisualState() == VisualState::Active);
    state.SetEnabled(false);
    assert(state.GetVisualState() == VisualState::Disabled);

    auto disabledRoot = std::make_unique<Control>();
    disabledRoot->SetBounds({0, 0, 20, 20});
    disabledRoot->SetEnabled(false);
    Host disabledHost;
    disabledHost.SetRoot(std::move(disabledRoot));
    assert(!disabledHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {1, 1})));
    assert(!disabledHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Tab)));

    auto tabRoot = std::make_unique<Control>();
    auto firstFocusable = std::make_unique<FocusableControl>();
    FocusableControl* rawFirstFocusable = firstFocusable.get();
    auto hiddenContainer = std::make_unique<Control>();
    hiddenContainer->SetVisible(false);
    auto hiddenFocusable = std::make_unique<FocusableControl>();
    FocusableControl* rawHiddenFocusable = hiddenFocusable.get();
    hiddenContainer->AddChild(std::move(hiddenFocusable));
    auto disabledContainer = std::make_unique<Control>();
    disabledContainer->SetEnabled(false);
    auto disabledFocusable = std::make_unique<FocusableControl>();
    FocusableControl* rawDisabledFocusable = disabledFocusable.get();
    disabledContainer->AddChild(std::move(disabledFocusable));
    auto nestedContainer = std::make_unique<Control>();
    auto secondFocusable = std::make_unique<FocusableControl>();
    FocusableControl* rawSecondFocusable = secondFocusable.get();
    auto thirdFocusable = std::make_unique<FocusableControl>();
    FocusableControl* rawThirdFocusable = thirdFocusable.get();
    nestedContainer->AddChild(std::move(secondFocusable));
    tabRoot->AddChild(std::move(firstFocusable));
    tabRoot->AddChild(std::move(hiddenContainer));
    tabRoot->AddChild(std::move(disabledContainer));
    tabRoot->AddChild(std::move(nestedContainer));
    tabRoot->AddChild(std::move(thirdFocusable));
    Host tabHost;
    tabHost.SetRoot(std::move(tabRoot));
    assert(tabHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Tab)));
    assert(tabHost.FocusedControl() == rawFirstFocusable);
    assert(tabHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Tab)));
    assert(tabHost.FocusedControl() == rawSecondFocusable);
    assert(tabHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Tab)));
    assert(tabHost.FocusedControl() == rawThirdFocusable);
    assert(tabHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Tab)));
    assert(tabHost.FocusedControl() == rawFirstFocusable);
    assert(tabHost.Dispatch(ysDui::test::MakeEvent(
        EventType::KeyDown, {}, key::Tab, modifier::Shift)));
    assert(tabHost.FocusedControl() == rawThirdFocusable);
    tabHost.SetFocusedControl(rawHiddenFocusable);
    assert(tabHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Tab)));
    assert(tabHost.FocusedControl() == rawFirstFocusable);
    tabHost.SetFocusedControl(rawDisabledFocusable);
    assert(tabHost.Dispatch(ysDui::test::MakeEvent(
        EventType::KeyDown, {}, key::Tab, modifier::Shift)));
    assert(tabHost.FocusedControl() == rawThirdFocusable);

}
