/**
 * 文件名：theme_resource_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：验证主题、皮肤目录和资源路径解析逻辑。
 */
#include "test_support.hpp"

int main()
{
    using namespace ysDui::core;
    auto skinCatalog = std::make_shared<ysDui::core::DuiSkinCatalog>();
    assert(skinCatalog->Load(
        "<SkinList><Skin><SkinID>1</SkinID><SkinName>Light</SkinName><SkinPath>skins/light</SkinPath></Skin>"
        "<Skin><SkinID>2</SkinID><SkinName>Dark</SkinName><SkinPath>skins/dark/</SkinPath></Skin>"
        "<CurSkinID>2</CurSkinID></SkinList>"));
    assert(skinCatalog->Skins().size() == 2 && skinCatalog->DefaultSkin()->id == 2);
    const auto originalSkins = skinCatalog->Skins();
    assert(!skinCatalog->Load("<SkinList><Skin><SkinID>2</SkinID></Skin></SkinList>"));
    assert(skinCatalog->Skins().size() == originalSkins.size());
    ysDui::core::DuiSkinSession skinSession;
    int skinChanged{};
    {
        auto skinSubscription = skinSession.SubscribeScoped([&skinChanged] { ++skinChanged; });
        assert(skinSubscription);
        skinSession.SetCatalog(skinCatalog);
        assert(skinChanged == 1 && skinSession.CurrentSkin()->name == "Dark");
        assert(skinSession.ResolveResourcePath("icons/close.png") == "skins/dark/icons/close.png");
        assert(skinSession.SetCurrentSkin(1) && skinChanged == 2);
        assert(skinSession.ResolveResourcePath("/icons/close.png") == "skins/light/icons/close.png");
        assert(!skinSession.SetCurrentSkin(99));
    }
    assert(skinSession.SetCurrentSkin(2) && skinChanged == 2);
    assert(!skinSession.SubscribeScoped({}));

    DuiTheme theme;
    assert(theme.GetStateColor(ThemeSlot::BrandPrimary, VisualState::Hover) == theme.Get(ThemeSlot::BrandHover));
    int notifications{};
    auto subscription = theme.SubscribeScoped([&notifications] { ++notifications; });
    assert(subscription);
    theme.ApplyPreset(ThemePreset::Dark);
    assert(notifications == 1);
    assert((theme.Get(ThemeSlot::SurfaceBackground) == Color{30, 32, 38, 255}));
    auto movedSubscription = std::move(subscription);
    assert(!subscription && movedSubscription);
    movedSubscription.Reset();
    movedSubscription.Reset();
    theme.ApplyPreset(ThemePreset::HighContrast);
    assert(notifications == 1);
    assert((theme.Get(ThemeSlot::BrandPrimary) == Color{255, 255, 0, 255}));
    assert((theme.Get(ThemeSlot::TextLink) == Color{0, 255, 255, 255}));
    theme.SetDefaultFontFamily("Microsoft YaHei UI");
    const unsigned elementVersion = theme.Version();
    theme.ApplyPreset(ThemePreset::ElementLight);
    assert(theme.Version() == elementVersion + 1);
    assert(theme.DefaultFontFamily() == "Microsoft YaHei UI");
    assert((theme.Get(ThemeSlot::BrandPrimary) == Color{64, 158, 255, 255}));
    assert((theme.Get(ThemeSlot::BrandHover) == Color{102, 177, 255, 255}));
    assert((theme.Get(ThemeSlot::BrandPressed) == Color{58, 142, 230, 255}));
    assert((theme.Get(ThemeSlot::StatusOnline) == Color{103, 194, 58, 255}));
    assert((theme.Get(ThemeSlot::StatusAway) == Color{230, 162, 60, 255}));
    assert((theme.Get(ThemeSlot::Danger) == Color{245, 108, 108, 255}));
    assert((theme.Get(ThemeSlot::FieldBorder) == Color{220, 223, 230, 255}));
    assert((theme.Get(ThemeSlot::FieldFocusBorder) == Color{64, 158, 255, 255}));
    assert((theme.Get(ThemeSlot::FieldPlaceholder) == Color{168, 171, 178, 255}));
    assert((theme.Get(ThemeSlot::GridSelection) == Color{236, 245, 255, 255}));
    assert((theme.Get(ThemeSlot::SpreadsheetActiveCell) == Color{64, 158, 255, 255}));
    assert((theme.Get(ThemeSlot::DialogAccent) == Color{64, 158, 255, 255}));
    assert(theme.GetStateColor(ThemeSlot::BrandPrimary, VisualState::Hover)
        == theme.Get(ThemeSlot::BrandHover));
    const unsigned version = theme.Version();
    assert(!theme.SubscribeScoped({}));
    theme.SetDefaultFontPointSize(1);
    assert(theme.DefaultFontPointSize() == 6);
    assert(theme.Version() == version + 1);

    DuiTheme movableTheme;
    int discardedNotifications{};
    int retainedNotifications{};
    auto assignedSubscription = movableTheme.SubscribeScoped(
        [&discardedNotifications] { ++discardedNotifications; });
    auto replacementSubscription = movableTheme.SubscribeScoped(
        [&retainedNotifications] { ++retainedNotifications; });
    assignedSubscription = std::move(replacementSubscription);
    movableTheme.Set(ThemeSlot::BrandPrimary, {1, 2, 3, 255});
    assert(discardedNotifications == 0 && retainedNotifications == 1);
    DuiTheme movedTheme(std::move(movableTheme));
    movedTheme.Set(ThemeSlot::BrandPrimary, {4, 5, 6, 255});
    assert(retainedNotifications == 2);

    auto themedRoot = std::make_unique<Control>();
    auto themedChild = std::make_unique<Control>();
    Control* themedRootRaw = themedRoot.get();
    Control* themedChildRaw = themedChild.get();
    themedRoot->AddChild(std::move(themedChild));
    Host themedHost;
    int themeChanges{};
    themedHost.SetThemeChangedHandler([&themeChanges] { ++themeChanges; });
    themedHost.SetRoot(std::move(themedRoot));
    assert(&themedRootRaw->Theme() == &themedHost.Theme());
    assert(&themedChildRaw->Theme() == &themedHost.Theme());
    DuiTheme controlTheme;
    controlTheme.ApplyPreset(ThemePreset::Dark);
    themedRootRaw->SetTheme(&controlTheme);
    assert(&themedRootRaw->Theme() == &controlTheme);
    assert(&themedChildRaw->Theme() == &controlTheme);
    themedRootRaw->SetTheme(nullptr);
    assert(&themedChildRaw->Theme() == &themedHost.Theme());
    themedHost.Theme().Set(ThemeSlot::BrandPrimary, {1, 2, 3, 255});
    assert(themeChanges == 1);

}
