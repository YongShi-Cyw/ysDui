/**
 * 文件名：controls_basic_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：验证基础、反馈、媒体和常用列表控件行为。
 */
#include "test_support.hpp"

#include "ysDui/controls/content/DuiMarkdownView.hpp"
#include "ysDui/controls/media/DuiImageViewer.hpp"

class UnconditionalPane final : public ysDui::core::Control, public ysDui::render::DuiRenderable
{
public:
    explicit UnconditionalPane(ysDui::core::Color color) : color_(color) {}

    void Paint(ysDui::render::Canvas& canvas, ysDui::core::Rect) const override
    {
        canvas.FillRect(Bounds(), color_);
    }

private:
    ysDui::core::Color color_;
};

/** 首选高度固定的测试替身：让依赖内容高度的容器（如 DuiChatBubble）可确定地测量。 */
class FixedSizePane final : public ysDui::core::Control, public ysDui::render::DuiRenderable
{
public:
    explicit FixedSizePane(ysDui::core::Size desired) : desired_(desired) {}

    [[nodiscard]] ysDui::core::Size DesiredSize() const override { return desired_; }

    void Paint(ysDui::render::Canvas& canvas, ysDui::core::Rect) const override
    {
        canvas.FillRect(Bounds(), {10, 20, 30, 255});
    }

private:
    ysDui::core::Size desired_;
};

/** 高度可控的测试替身：用于验证变高列表的测量、失效与滚动。 */
class MutableHeightPane final : public ysDui::core::Control, public ysDui::render::DuiRenderable
{
public:
    explicit MutableHeightPane(int height) : height_(height) {}

    void SetDesiredHeight(int height) { height_ = height; }
    [[nodiscard]] int LastLayoutWidth() const { return lastLayoutWidth_; }

    [[nodiscard]] ysDui::core::Size DesiredSize() const override { return {0, height_}; }

    void Layout(ysDui::core::Rect bounds) override
    {
        lastLayoutWidth_ = bounds.Width();
        ysDui::core::Control::Layout(bounds);
    }

    void Paint(ysDui::render::Canvas& canvas, ysDui::core::Rect) const override
    {
        canvas.FillRect(Bounds(), {10, 20, 30, 255});
    }

private:
    int height_;
    int lastLayoutWidth_{};
};

int main()
{
    using namespace ysDui::core;
    AnimationClock clock;
    DuiTheme theme;
    theme.ApplyPreset(ThemePreset::Dark);
    theme.ApplyPreset(ThemePreset::HighContrast);
    theme.SetDefaultFontPointSize(1);

    ysDui::controls::input::DuiEditHost accessibleEdit;
    accessibleEdit.SetPlaceholder("Type here");
    accessibleEdit.SetText("Alice");
    accessibleEdit.SetAccessibilityName("User name");
    accessibleEdit.SetAccessibilityIdentifier("9104");
    const auto editAccessibility = accessibleEdit.Accessibility();
    assert(editAccessibility.role == DuiAccessibilityRole::Edit);
    assert(editAccessibility.name == "User name" && editAccessibility.value == "Alice");
    assert(editAccessibility.keyboardFocusable && editAccessibility.identifier == "9104");

    ysDui::controls::basic::DuiLabel label;
    label.SetBounds({0, 0, 100, 20});
    label.SetText("ysDui");
    label.SetAlignment(ysDui::render::DuiTextAlignment::Center);
    label.SetWordWrap(true);
    RecordingCanvas labelCanvas;
    label.Paint(labelCanvas, {0, 0, 100, 20});
    assert(labelCanvas.drawnText == "ysDui");
    std::string activatedLink;
    label.SetLinkTarget("https://example.test");
    label.SetLinkActivatedHandler([&activatedLink](std::string_view value) { activatedLink = value; });
    assert(label.Accessibility().role == ysDui::core::DuiAccessibilityRole::Hyperlink);
    assert(label.Accessibility().keyboardFocusable);
    assert(label.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Enter)));
    assert(activatedLink == "https://example.test" && label.Visited());
    activatedLink.clear();
    assert(label.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(label.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {10, 10})));
    assert(activatedLink == "https://example.test" && label.Visited());
    label.Paint(labelCanvas, {0, 0, 100, 20});
    assert(labelCanvas.textStyle.underline);
    label.SetSelectable(true);
    assert(!label.Selectable());
    label.SetWordWrap(false);
    label.SetSelectable(true);
    label.SetSelectionColor({255, 230, 130, 255});
    label.Paint(labelCanvas, {0, 0, 100, 20});
    assert(label.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {31, 10})));
    assert(label.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {53, 10})));
    assert(label.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {53, 10})));
    assert(label.Selected() && label.SelectedText() == "ysD");
    assert((label.SelectionColor() == Color{255, 230, 130, 255}));
    label.Paint(labelCanvas, {0, 0, 100, 20});
    assert(!labelCanvas.fills.empty() && (labelCanvas.fills.back().color == Color{255, 230, 130, 255}));
    ClipboardMock clipboard;
    label.SetClipboard(&clipboard);
    assert(label.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, 'C', modifier::Control)));
    assert(clipboard.text == "ysD");
    assert(label.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, 'A', modifier::Control)));
    assert(label.SelectedText() == "ysDui");
    label.SetSelectable(false);
    assert(!label.Selected());
    assert((labelCanvas.textBounds == Rect{0, 0, 100, 20}));
    assert(labelCanvas.textAlignment == ysDui::render::DuiTextAlignment::Center);
    assert(!labelCanvas.wrapped);

    ysDui::controls::basic::DuiBadge badge;
    badge.SetBounds({0, 0, 100, 20});
    badge.SetCount(100);
    assert(badge.Text() == "99+");
    assert(ysDui::controls::basic::DuiBadge::FormatCount(0).empty());
    assert(ysDui::controls::basic::DuiBadge::ContentWidth(-1, 3, 4, true) == 10);
    assert(ysDui::controls::basic::DuiBadge::ApplyMaxChars("a\U0001F600b", 2) == "a\U0001F600");
    badge.SetLeadingDot(ysDui::core::Color{40, 140, 80, 255});
    RecordingCanvas badgeCanvas;
    badge.Paint(badgeCanvas, {0, 0, 100, 20});
    assert(badgeCanvas.rounded.bounds.Width() == 44);
    assert(badgeCanvas.roundedRadius == 10);
    assert(badgeCanvas.ellipses.size() == 1);
    assert(badgeCanvas.drawnText == "99+");

    ysDui::controls::basic::DuiCard card;
    card.SetTitle("Account");
    card.SetSubtitle("Signed in");
    card.Layout({0, 0, 240, 120});
    auto cardLabel = std::make_unique<ysDui::controls::basic::DuiLabel>();
    cardLabel->SetText("Content");
    card.SetContent(std::move(cardLabel));
    assert((card.ContentRect() == Rect{16, 52, 224, 108}));
    RecordingCanvas cardCanvas;
    card.Paint(cardCanvas, card.Bounds());
    assert(cardCanvas.roundedRadius == 6);
    assert(std::find(cardCanvas.drawnTexts.begin(), cardCanvas.drawnTexts.end(), "Account")
           != cardCanvas.drawnTexts.end());
    assert(std::find(cardCanvas.drawnTexts.begin(), cardCanvas.drawnTexts.end(), "Content")
           != cardCanvas.drawnTexts.end());
    assert(cardCanvas.drawnTextBounds.size() >= 3
        && cardCanvas.drawnTextBounds[0].bottom <= cardCanvas.drawnTextBounds[1].top);

    ysDui::controls::feedback::DuiSkeleton skeleton;
    skeleton.SetLineCount(3);
    skeleton.SetLineHeight(10);
    skeleton.SetGap(4);
    skeleton.SetLineWidthPercent(80);
    skeleton.Layout({0, 0, 200, 38});
    assert((skeleton.DesiredSize() == Size{240, 38}) && skeleton.LineWidthPercent() == 80);
    RecordingCanvas skeletonCanvas;
    skeleton.Paint(skeletonCanvas, skeleton.Bounds());
    assert(skeletonCanvas.roundedRadius == 4);

    ysDui::controls::list::DuiPagination pagination;
    pagination.SetPageCount(10);
    pagination.SetMaxVisiblePages(5);
    pagination.SetCurrentPage(2);
    pagination.Layout({0, 0, 300, 28});
    assert(pagination.CurrentPage() == 2 && !pagination.PageRect(2).Empty()
        && pagination.PageRect(0).left < pagination.PageRect(2).left);
    int changedPage = -1;
    pagination.SetPageChangedHandler([&changedPage](int page) { changedPage = page; });
    assert(pagination.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Right)));
    assert(pagination.CurrentPage() == 3 && changedPage == 3);
    RecordingCanvas paginationCanvas;
    pagination.Paint(paginationCanvas, pagination.Bounds());
    assert(std::find(paginationCanvas.drawnTexts.begin(), paginationCanvas.drawnTexts.end(), "4")
           != paginationCanvas.drawnTexts.end());

    ysDui::controls::basic::DuiAvatar avatar;
    avatar.SetBounds({0, 0, 40, 40});
    avatar.SetName("Alice Smith");
    avatar.SetStatus(ysDui::controls::basic::DuiAvatarStatus::Online);
    assert(ysDui::controls::basic::DuiAvatar::ComputeInitials("Alice Smith") == "AS");
    assert(ysDui::controls::basic::DuiAvatar::ComputeInitials("陆星辰") == "陆");
    assert(ysDui::controls::basic::DuiAvatar::ComputeInitials("陆 星辰") == "陆星");
    RecordingCanvas avatarCanvas;
    avatar.Paint(avatarCanvas, {0, 0, 40, 40});
    assert(avatarCanvas.ellipses.size() == 3);
    assert(avatarCanvas.drawnText == "AS");
    avatar.SetShape(ysDui::controls::basic::DuiAvatarShape::RoundedRectangle);
    avatar.SetCornerRadius(999);
    avatar.SetStatus(ysDui::controls::basic::DuiAvatarStatus::None);
    avatarCanvas = {};
    avatar.Paint(avatarCanvas, {0, 0, 40, 40});
    assert(avatarCanvas.roundedRadius == 20);
    const auto avatarImage = ysDui::render::DuiImage::CreateBgra8Premultiplied({2, 2}, std::vector<unsigned char>(16, 255));
    avatar.SetImage(avatarImage);
    avatarCanvas = {};
    avatar.Paint(avatarCanvas, {0, 0, 40, 40});
    assert(avatarCanvas.images == 1 && (avatarCanvas.imageDestination == Rect{0, 0, 40, 40})
        && avatarCanvas.imageCornerRadius == 20);

    ysDui::controls::feedback::DuiEmojiPanel emojiPanel;
    emojiPanel.SetBounds({0, 0, 64, 64});
    emojiPanel.SetColumns(2);
    emojiPanel.SetCellSize(24);
    emojiPanel.AddEmojiSet({"\U0001F600", "\U0001F44D", "\u2764\uFE0F"});
    emojiPanel.AddEmoji("");
    emojiPanel.AddEmojiImage("", {});
    assert(emojiPanel.Count() == 3 && emojiPanel.RowCount() == 2);
    assert((emojiPanel.DesiredSize() == Size{48, 48}));
    assert((emojiPanel.CellRect(2) == Rect{0, 24, 24, 48}));
    assert(emojiPanel.HitTestIndex({30, 5}) == 1);
    std::string pickedEmoji;
    emojiPanel.SetPickHandler([&pickedEmoji](std::string_view sequence, int index) {
        if (index == 1) pickedEmoji = sequence;
    });
    assert(emojiPanel.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {30, 5})));
    assert(emojiPanel.HoveredIndex() == 1);
    assert(emojiPanel.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {30, 5})));
    RecordingCanvas pressedEmojiCanvas;
    emojiPanel.Paint(pressedEmojiCanvas, {0, 0, 64, 64});
    assert((pressedEmojiCanvas.rounded.color == Color{200, 220, 250, 255}));
    assert(emojiPanel.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {30, 5})));
    assert(pickedEmoji == "\U0001F44D");
    RecordingCanvas emojiCanvas;
    emojiPanel.Paint(emojiCanvas, {0, 0, 64, 64});
    assert((emojiCanvas.rounded.bounds == Rect{24, 0, 48, 24}));
    assert((emojiCanvas.rounded.color == Color{232, 240, 252, 255}));
    assert(emojiCanvas.drawnText == "\u2764\uFE0F");
    assert(emojiCanvas.textStyle.pointSize == 14);
    emojiPanel.SetCellSize(40);
    RecordingCanvas largeEmojiCanvas;
    emojiPanel.Paint(largeEmojiCanvas, {0, 0, 64, 64});
    assert(largeEmojiCanvas.textStyle.pointSize == 26);

    const int animationDelays[]{50, 100, 150};
    assert(ysDui::render::DuiAnimatedImage::FrameAt(0, animationDelays, 3) == 0);
    assert(ysDui::render::DuiAnimatedImage::FrameAt(50, animationDelays, 3) == 1);
    assert(ysDui::render::DuiAnimatedImage::FrameAt(150, animationDelays, 3) == 2);
    assert(ysDui::render::DuiAnimatedImage::FrameAt(300, animationDelays, 3) == 0);
    ysDui::controls::media::DuiGif gif;
    assert((gif.DesiredSize() == Size{}));
    gif.SetStretch(false);
    assert(!gif.Stretch());
    const auto gifFrame = ysDui::render::DuiImage::CreateBgra8Premultiplied(
        {2, 1}, {0, 0, 255, 255, 255, 0, 0, 255});
    const auto animation = ysDui::render::DuiAnimatedImage::Create({gifFrame, gifFrame}, {20, 30});
    assert((animation && animation->FrameCount() == 2 && animation->Size() == Size{2, 1}));
    assert(!ysDui::render::DuiAnimatedImage::Create({gifFrame}, {20, 30}));
    AnimationClock gifClock;
    gif.SetImage(animation);
    gif.Start();
    assert(gif.Running() && !gifClock.HasScheduledTasks());
    gif.SetAnimationClock(&gifClock);
    assert(gifClock.HasScheduledTasks());
    gifClock.Advance(20);
    assert(gif.FrameIndex() == 1 && gifClock.HasScheduledTasks());
    gif.Stop();
    assert(!gifClock.HasScheduledTasks());

    ysDui::controls::list::DuiTab tabs;
    tabs.Layout({0, 0, 100, 28});
    tabs.SetMinTabWidth(40);
    tabs.AddTab("One");
    tabs.AddTab("Two", true);
    tabs.AddTab("Three", false, true, 7);
    assert(tabs.Count() == 3 && tabs.SelectedIndex() == 0 && tabs.FindByValue(7) == 2);
    assert(tabs.NeedsScroll());
    int selectedTab = -1;
    int closedTab = -1;
    int dropdownTab = -1;
    tabs.SetSelectionChangedHandler([&selectedTab](int index) { selectedTab = index; });
    tabs.SetCloseHandler([&closedTab](int index) { closedTab = index; });
    tabs.SetDropdownHandler([&dropdownTab](int index) { dropdownTab = index; });
    const auto second = tabs.TabRect(1);
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {second.left + 3, second.top + 3})));
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {second.left + 3, second.top + 3})));
    assert(tabs.SelectedIndex() == 1 && selectedTab == 1);
    const auto close = tabs.CloseRect(1);
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {close.left + 1, close.top + 1})));
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {close.left + 1, close.top + 1})));
    assert(closedTab == 1);
    const auto dropdown = tabs.DropdownRect(2);
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {dropdown.left + 1, dropdown.top + 1})));
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {dropdown.left + 1, dropdown.top + 1})));
    assert(dropdownTab == 2);
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::End)));
    assert(tabs.SelectedIndex() == 2);
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerWheel, {}, 0, 0, 0, 120)));
    assert(tabs.SelectedIndex() == 1);
    tabs.SetSelectedIndex(0, false);
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Left)));
    assert(tabs.SelectedIndex() == 2);
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Down)));
    assert(tabs.SelectedIndex() == 0);
    tabs.SetSelectedIndex(-1, false);
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Left)));
    assert(tabs.SelectedIndex() == 2);
    tabs.SetSelectedIndex(0, false);
    const auto tabAccessibility = tabs.Accessibility();
    assert(tabAccessibility.role == ysDui::core::DuiAccessibilityRole::Tab);
    assert(tabAccessibility.value == "One" && tabAccessibility.keyboardFocusable);

    tabs.Layout({0, 0, 300, 28});
    tabs.SetEnabled(false);
    RecordingCanvas disabledTabCanvas;
    tabs.Paint(disabledTabCanvas, {0, 0, 300, 28});
    assert(disabledTabCanvas.roundedStrokes == tabs.Count());
    assert(disabledTabCanvas.paths >= 1);
    assert((disabledTabCanvas.textStyle.color == Color{170, 170, 170, 255}));
    tabs.SetEnabled(true);
    const auto hoveredClose = tabs.CloseRect(1);
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove,
                                               {(hoveredClose.left + hoveredClose.right) / 2,
                                                (hoveredClose.top + hoveredClose.bottom) / 2})));
    RecordingCanvas hoveredTabCanvas;
    tabs.Paint(hoveredTabCanvas, {0, 0, 300, 28});
    assert(std::any_of(hoveredTabCanvas.fills.begin(), hoveredTabCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{220, 100, 100, 255};
    }));
    // 焦点线框默认不绘制，须由主题显式开启（见 DuiTheme::SetFocusRingVisible）
    DuiTheme focusRingTheme;
    focusRingTheme.SetFocusRingVisible(true);
    tabs.SetTheme(&focusRingTheme);
    tabs.SetFocused(true);
    RecordingCanvas focusedTabCanvas;
    tabs.Paint(focusedTabCanvas, {0, 0, 300, 28});
    bool tabFocusRing{};
    for (std::size_t index = 0; index < focusedTabCanvas.strokeBoundsList.size(); ++index)
    {
        if (focusedTabCanvas.strokeBoundsList[index] == Rect{0, 0, 300, 28}
            && focusedTabCanvas.strokeColors[index] == Color{45, 108, 223, 255})
        {
            tabFocusRing = true;
            break;
        }
    }
    assert(tabFocusRing);
    tabs.SetFocused(false);

    DuiTheme tabTheme;
    tabTheme.Set(ThemeSlot::TabBackground, {11, 21, 31, 255});
    tabTheme.Set(ThemeSlot::TabSelected, {12, 22, 32, 255});
    ysDui::controls::list::DuiTab themedTab;
    themedTab.SetTheme(&tabTheme);
    themedTab.AddTab("Theme");
    themedTab.Layout({0, 0, 120, 28});
    RecordingCanvas themedTabCanvas;
    themedTab.Paint(themedTabCanvas, themedTab.Bounds());
    assert((themedTabCanvas.fills.front().color == Color{11, 21, 31, 255}));
    assert(std::any_of(themedTabCanvas.fills.begin(), themedTabCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{12, 22, 32, 255};
    }));
    themedTab.SetSelectedColor({13, 23, 33, 255});
    themedTabCanvas = {};
    themedTab.Paint(themedTabCanvas, themedTab.Bounds());
    assert(std::any_of(themedTabCanvas.fills.begin(), themedTabCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{13, 23, 33, 255};
    }));

    tabs.SetReorderEnabled(true);
    const auto first = tabs.TabRect(0);
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {first.left + 3, first.top + 3})));
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {tabs.TabRect(2).right - 2, first.top + 3})));
    RecordingCanvas draggingTabCanvas;
    tabs.Paint(draggingTabCanvas, {0, 0, 300, 28});
    assert(std::any_of(draggingTabCanvas.fills.begin(), draggingTabCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{45, 108, 223, 255} && fill.bounds.Width() == 2;
    }));
    assert(tabs.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {tabs.TabRect(2).right - 2, first.top + 3})));
    assert(tabs.TextAt(2) == "One");

    ysDui::controls::list::DuiTabPage tabPage;
    auto firstPage = std::make_unique<ysDui::controls::basic::DuiLabel>();
    auto* firstPageRaw = firstPage.get();
    auto secondPage = std::make_unique<ysDui::controls::basic::DuiLabel>();
    auto* secondPageRaw = secondPage.get();
    tabPage.AddPage("First", std::move(firstPage));
    tabPage.AddPage("Second", std::move(secondPage));
    const auto tabPageIcon = ysDui::render::DuiImage::CreateBgra8Premultiplied({1, 1}, {255, 0, 0, 255});
    tabPage.SetPageIcon(0, tabPageIcon);
    tabPage.SetIconSize(16);
    tabPage.SetIconGap(6);
    tabPage.SetAutoFitTabWidth(true);
    tabPage.Layout({0, 0, 120, 80});
    assert(tabPage.SelectedIndex() == 0 && firstPageRaw->Visible() && !secondPageRaw->Visible());
    assert(tabPage.PageIcon(0) == tabPageIcon && tabPage.Header().IconAt(0) == tabPageIcon);
    int selectedPage = -1;
    tabPage.SetSelectionChangedHandler([&selectedPage](int index) { selectedPage = index; });
    tabPage.Header().SetSelectedIndex(1);
    assert(tabPage.SelectedIndex() == 1 && selectedPage == 1);
    assert(!firstPageRaw->Visible() && secondPageRaw->Visible());
    assert((secondPageRaw->Bounds() == Rect{0, 32, 120, 80}));

    ysDui::controls::list::DuiTabPage paintedTabPage;
    paintedTabPage.AddPage("First", std::make_unique<UnconditionalPane>(Color{45, 108, 223, 255}));
    paintedTabPage.AddPage("Second", std::make_unique<UnconditionalPane>(Color{220, 110, 60, 255}));
    paintedTabPage.Layout({0, 0, 120, 80});
    RecordingCanvas selectedPageCanvas;
    paintedTabPage.Paint(selectedPageCanvas, paintedTabPage.Bounds());
    assert(std::any_of(selectedPageCanvas.fills.begin(), selectedPageCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{45, 108, 223, 255};
    }));
    assert(std::none_of(selectedPageCanvas.fills.begin(), selectedPageCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{220, 110, 60, 255};
    }));
    paintedTabPage.SetSelectedIndex(1);
    RecordingCanvas switchedPageCanvas;
    paintedTabPage.Paint(switchedPageCanvas, paintedTabPage.Bounds());
    assert(std::any_of(switchedPageCanvas.fills.begin(), switchedPageCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{220, 110, 60, 255};
    }));
    assert(std::none_of(switchedPageCanvas.fills.begin(), switchedPageCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{45, 108, 223, 255};
    }));

    ysDui::controls::list::DuiVirtualList virtualList;
    virtualList.SetRowCount(10);
    virtualList.SetRowHeight(20);
    virtualList.Layout({0, 0, 80, 60});
    int renderedRow = -1;
    virtualList.SetRowRenderer([&renderedRow](ysDui::render::Canvas&, int index, Rect, bool, bool) { renderedRow = index; });
    assert(virtualList.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {5, 45})));
    assert(virtualList.SelectedIndex() == 2);
    assert(virtualList.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::PageDown)));
    assert(virtualList.SelectedIndex() == 5);
    RecordingCanvas virtualCanvas;
    virtualList.Paint(virtualCanvas, {0, 0, 80, 60});
    assert(renderedRow >= 3);

    ysDui::controls::list::DuiVirtualList preLayoutVirtualList;
    preLayoutVirtualList.SetRowCount(10000);
    preLayoutVirtualList.SetSelectedIndex(0, false);
    preLayoutVirtualList.Layout({0, 0, 80, 60});
    assert(preLayoutVirtualList.ScrollPosition() == 0 && preLayoutVirtualList.RowRect(0).top == 0);

    ysDui::controls::list::DuiListBox listBox;
    listBox.Layout({0, 0, 100, 60});
    listBox.AddItem("First", 10);
    listBox.AddItem("Second", 20);
    listBox.AddItem("Third", 30);
    listBox.AddItem("Fourth", 40);
    const auto listIcon = ysDui::render::DuiImage::CreateBgra8Premultiplied(
        {2, 2}, std::vector<unsigned char>(16, 255));
    const auto listItemIcon = ysDui::render::DuiImage::CreateBgra8Premultiplied(
        {2, 2}, std::vector<unsigned char>(16, 128));
    listBox.SetLeadingIcon(listIcon);
    listBox.SetIconAt(1, listItemIcon);
    assert(listBox.IconAt(1) == listItemIcon && !listBox.IconAt(-1));
    RecordingCanvas listIconCanvas;
    listBox.Paint(listIconCanvas, listBox.Bounds());
    assert(listIconCanvas.drawnImages.size() == 3 && listIconCanvas.drawnImages[0] == listIcon.get()
        && listIconCanvas.drawnImages[1] == listItemIcon.get() && listIconCanvas.drawnImages[2] == listIcon.get());
    int selectedListItem = -2;
    listBox.SetSelectionChangedHandler([&selectedListItem](int index) { selectedListItem = index; });
    assert(listBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 25})));
    assert(listBox.SelectedIndex() == 1 && selectedListItem == 1 && listBox.ValueAt(1) == 20);
    listBox.SetMultiSelect(true);
    assert(listBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 45}, 0, modifier::Control)));
    assert(listBox.SelectionCount() == 2 && listBox.IsSelected(1) && listBox.IsSelected(2));
    listBox.SetCheckboxesVisible(true);
    assert(listBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {5, 5})));
    assert(listBox.IsChecked(0));
    listBox.SetReorderEnabled(true);
    assert(listBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {40, 5})));
    assert(listBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {40, 59})));
    assert(listBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {40, 59})));
    assert(listBox.TextAt(2) == "First");

    DuiTheme listTheme;
    listTheme.Set(ThemeSlot::ListBackground, {14, 24, 34, 255});
    listTheme.Set(ThemeSlot::ListSelection, {15, 25, 35, 255});
    ysDui::controls::list::DuiListBox themedListBox;
    themedListBox.SetTheme(&listTheme);
    themedListBox.AddItem("Selected");
    themedListBox.SetSelectedIndex(0, false);
    themedListBox.Layout({0, 0, 100, 30});
    RecordingCanvas themedListCanvas;
    themedListBox.Paint(themedListCanvas, themedListBox.Bounds());
    assert((themedListCanvas.fills.front().color == Color{14, 24, 34, 255}));
    assert((themedListCanvas.fills[1].color == Color{15, 25, 35, 255}));
    themedListBox.SetBackgroundColor({16, 26, 36, 255});
    themedListCanvas = {};
    themedListBox.Paint(themedListCanvas, themedListBox.Bounds());
    assert((themedListCanvas.fills.front().color == Color{16, 26, 36, 255}));

    ysDui::controls::list::DuiListBox preLayoutListBox;
    for (int index = 0; index < 50; ++index)
        preLayoutListBox.AddItem("Row " + std::to_string(index + 1));
    preLayoutListBox.SetSelectedIndex(0, false);
    preLayoutListBox.Layout({0, 0, 100, 60});
    assert(preLayoutListBox.ScrollPosition() == 0 && preLayoutListBox.TextAt(0) == "Row 1");

    ysDui::controls::list::DuiDataGrid dataGrid;
    dataGrid.Layout({0, 0, 180, 100});
    dataGrid.AddColumn({"Name", 100});
    dataGrid.AddColumn({"Age", 60, 40, ysDui::render::DuiTextAlignment::End});
    dataGrid.AddRow();
    dataGrid.AddRow();
    dataGrid.SetCellText(0, 0, "Ada");
    dataGrid.SetCellText(1, 0, "Lin");
    int changedRow = -2;
    dataGrid.SetSelectionChangedHandler([&changedRow](int row) { changedRow = row; });
    assert(dataGrid.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 55})));
    assert(dataGrid.SelectedRow() == 1 && changedRow == 1);
    dataGrid.SetCheckboxesVisible(true);
    assert(dataGrid.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 35})));
    assert(dataGrid.RowChecked(0));
    int sortColumn = -1;
    dataGrid.SetColumnClickedHandler([&sortColumn](int column, int direction) { sortColumn = column * direction; });
    assert(dataGrid.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {30, 10})));
    assert(dataGrid.SortColumn() == 0 && dataGrid.SortDirection() == 1 && sortColumn == 0);
    TextInputMock gridInput;
    dataGrid.SetEditable(true);
    dataGrid.SetTextInput(&gridInput);
    assert(dataGrid.BeginEdit(0, 0));
    assert(gridInput.visible && gridInput.focused && gridInput.text == "Ada");
    gridInput.SetText("Grace");
    RecordingCanvas proxyGridCanvas;
    dataGrid.Paint(proxyGridCanvas, dataGrid.Bounds());
    assert(std::find(proxyGridCanvas.drawnTexts.begin(), proxyGridCanvas.drawnTexts.end(), "Grace")
           != proxyGridCanvas.drawnTexts.end());
    dataGrid.CommitEdit();
    assert(!gridInput.visible && dataGrid.CellText(0, 0) == "Grace");
    assert(dataGrid.BeginEdit(1, 0));
    gridInput.SetText("Katherine");
    gridInput.LoseFocus();
    assert(!dataGrid.Editing() && dataGrid.CellText(1, 0) == "Katherine");

    ysDui::core::AnimationClock toastClock;
    ysDui::controls::basic::DuiToast toast;
    toast.SetAnimationClock(&toastClock);
    toast.SetTextMeasurer(&labelCanvas);
    toast.SetDurationMilliseconds(20);
    toast.SetFadeMilliseconds(10);
    toast.Layout({0, 0, 200, 100});
    toast.Show("Saved");
    assert(toast.Active() && toast.Opacity() == 0.0 && toast.ContentRect().top == 40);
    assert(toast.HitTest({100, 45}) == nullptr);
    assert(ysDui::controls::basic::DuiToast::ApplyEllipsis("通知消息", 2) == "通知");
    assert(ysDui::controls::basic::DuiToast::ApplyEllipsis("通知消息已送达", 4) == "通...");
    toastClock.Advance(10);
    assert(toast.Opacity() == 1.0);
    toastClock.Advance(20);
    assert(toast.Active());
    toastClock.Advance(10);
    assert(!toast.Active() && toast.Opacity() == 0.0 && !toastClock.HasScheduledTasks());
    toast.Show("First notification");
    toastClock.Advance(5);
    assert(toast.Opacity() == 0.5);
    toast.Show("Replacement notification");
    assert(toast.Text() == "Replacement notification" && toast.Opacity() == 0.5 && toastClock.HasScheduledTasks());
    toastClock.Advance(5);
    assert(toast.Opacity() == 0.75);
    toast.HideNow();
    assert(!toast.Active() && !toastClock.HasScheduledTasks());
    {
        ysDui::controls::basic::DuiToast transientToast;
        transientToast.SetAnimationClock(&toastClock);
        transientToast.SetFadeMilliseconds(10);
        transientToast.Show("Transient");
        assert(toastClock.HasScheduledTasks());
    }
    assert(!toastClock.HasScheduledTasks());

    // DuiToast 扩展：类型 / 外观 / 关闭按钮 / 停靠边 / duration=0
    {
        ysDui::core::AnimationClock extendedClock;
        ysDui::controls::basic::DuiToast typed;
        typed.SetAnimationClock(&extendedClock);
        typed.SetTextMeasurer(&labelCanvas);
        assert(typed.Type() == ysDui::controls::basic::DuiToastType::Information);
        assert(typed.Appearance() == ysDui::controls::basic::DuiToastAppearance::Dark);
        assert(typed.Placement() == ysDui::controls::basic::DuiToastPlacement::Top);
        assert(!typed.ShowClose());
        typed.SetType(ysDui::controls::basic::DuiToastType::Error);
        typed.SetAppearance(ysDui::controls::basic::DuiToastAppearance::Light);
        typed.SetShowClose(true);
        typed.SetEdgeOffset(12);
        assert(typed.Type() == ysDui::controls::basic::DuiToastType::Error
            && typed.Appearance() == ysDui::controls::basic::DuiToastAppearance::Light
            && typed.EdgeOffset() == 12);

        typed.Layout({0, 0, 300, 200});
        typed.Show("Failed");
        // 未启用关闭按钮时覆盖层完全穿透
        assert(typed.HitTest({150, 30}) == nullptr);

        // 关闭按钮只认领自身矩形，其余位置仍穿透
        const Rect closeRect = typed.CloseRect();
        assert(!closeRect.Empty());
        const Point closeCenter{(closeRect.left + closeRect.right) / 2,
                                (closeRect.top + closeRect.bottom) / 2};
        assert(typed.HitTest(closeCenter) == &typed);
        assert(typed.HitTest({closeRect.left - 30, closeCenter.y}) == nullptr);

        // 点击关闭按钮立即结束并通知
        int closeNotifications{};
        typed.SetClosedHandler([&closeNotifications] { ++closeNotifications; });
        assert(typed.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, closeCenter)));
        assert(typed.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, closeCenter)));
        assert(closeNotifications == 1 && !typed.Active());

        // 自动消失同样通知
        typed.SetShowClose(false);
        typed.SetDurationMilliseconds(10);
        typed.SetFadeMilliseconds(0);
        typed.Show("Auto");
        assert(typed.Active());
        extendedClock.Advance(20);
        assert(!typed.Active() && closeNotifications == 2);

        // duration = 0：不自动关闭
        typed.SetDurationMilliseconds(0);
        typed.Show("Sticky");
        assert(typed.Active());
        extendedClock.Advance(100000);
        assert(typed.Active() && !extendedClock.HasScheduledTasks());
        typed.HideNow();

        // Bottom 停靠：从可用区域底边向上量取偏移
        ysDui::controls::basic::DuiToast bottom;
        bottom.SetTextMeasurer(&labelCanvas);
        bottom.SetPlacement(ysDui::controls::basic::DuiToastPlacement::Bottom);
        bottom.SetEdgeOffset(12);
        bottom.SetDurationMilliseconds(0);
        bottom.Layout({0, 0, 300, 200});
        bottom.Show("Bottom");
        const Rect bottomRect = bottom.ContentRect();
        assert(bottomRect.bottom == 200 - 12);
        assert(bottomRect.top < bottomRect.bottom);

        // 类型图标为矢量绘制：一个底色圆 + 符号（不依赖字体）
        const ysDui::controls::basic::DuiToastType iconTypes[]{
            ysDui::controls::basic::DuiToastType::Information,
            ysDui::controls::basic::DuiToastType::Success,
            ysDui::controls::basic::DuiToastType::Warning,
            ysDui::controls::basic::DuiToastType::Error,
        };
        for (const auto type : iconTypes)
        {
            ysDui::controls::basic::DuiToast iconToast;
            iconToast.SetTextMeasurer(&labelCanvas);
            iconToast.SetDurationMilliseconds(0);
            iconToast.SetType(type);
            iconToast.SetIconSize(16);
            iconToast.Layout({0, 0, 300, 100});
            iconToast.Show("Icon");
            RecordingCanvas iconCanvas;
            iconToast.Paint(iconCanvas, iconToast.Bounds());
            // 底色圆
            assert(iconCanvas.ellipses.size() == 1);
            // 符号：Information/Warning 用圆角矩形，Success/Error 用描边路径
            const bool vectorSymbol = !iconCanvas.filledPaths.empty()
                || iconCanvas.rounded.bounds.Width() > 0 || !iconCanvas.strokedPaths.empty();
            assert(vectorSymbol);
            // 文本不应包含任何字母字形充当图标（仅消息正文一次绘制）
            assert(iconCanvas.drawnTexts.size() == 1 && iconCanvas.drawnTexts.front() == "Icon");
        }

        // 默认直角（无圆角）；SetCornerRadius 可恢复圆角外观。
        // 需提供自定义图标以跳过内置矢量符号——符号本身也用圆角矩形绘制，会覆盖
        // 录制画布的单个 rounded 槽位，导致读到的不是气泡背景。
        ysDui::controls::basic::DuiToast squareToast;
        squareToast.SetTextMeasurer(&labelCanvas);
        squareToast.SetDurationMilliseconds(0);
        squareToast.SetIcon(ysDui::render::DuiImage::CreateBgra8Premultiplied({2, 2}, std::vector<unsigned char>(16, 255)));
        squareToast.Layout({0, 0, 300, 60});
        squareToast.Show("Square");
        RecordingCanvas squareCanvas;
        squareToast.Paint(squareCanvas, squareToast.Bounds());
        assert(squareCanvas.roundedRadius == 0);
        squareToast.SetCornerRadius(12);
        RecordingCanvas roundedToastCanvas;
        squareToast.Paint(roundedToastCanvas, squareToast.Bounds());
        assert(roundedToastCanvas.roundedRadius == 12);
    }

    // DuiToastCenter：堆叠、分组、重复计数与生命周期
    {
        ysDui::core::AnimationClock centerClock;
        ysDui::controls::basic::DuiToastCenter center;
        center.SetAnimationClock(&centerClock);
        center.SetTextMeasurer(&labelCanvas);
        center.SetEdgeOffset(16);
        center.SetGap(8);
        assert(center.Count() == 0 && !center.Grouping());
        assert((center.DesiredSize() == Size{0, 0}));
        center.Layout({0, 0, 400, 300});

        // 空文本忽略
        assert(!center.Show(""));
        assert(center.Count() == 0);

        // 两条消息自上而下堆叠，间距为 gap
        ysDui::controls::basic::DuiToastOptions options;
        options.appearance = ysDui::controls::basic::DuiToastAppearance::Dark;
        options.durationMilliseconds = 0;
        assert(center.Show("First", options));
        assert(center.Show("Second", options));
        assert(center.Count() == 2);
        assert(center.MessageAt(0) == "First" && center.MessageAt(1) == "Second");
        assert(center.RepeatNumAt(0) == 1 && center.RepeatNumAt(1) == 1);
        center.Layout({0, 0, 400, 300});

        const Rect firstBounds = center.Children()[0]->Bounds();
        const Rect secondBounds = center.Children()[1]->Bounds();
        assert(firstBounds.top == 16);
        assert(secondBounds.top == firstBounds.bottom + 8);
        assert((center.BadgeRectAt(0).Empty()));

        // 关闭按钮之外的区域必须穿透，覆盖层不遮挡下层
        assert(center.HitTest({5, 5}) == nullptr);

        // 启用关闭按钮后，仅按钮矩形被认领，其余位置仍穿透
        center.Clear();
        ysDui::controls::basic::DuiToastOptions closable = options;
        closable.showClose = true;
        assert(center.Show("Closable", closable));
        center.Layout({0, 0, 400, 300});
        auto* closableToast = dynamic_cast<ysDui::controls::basic::DuiToast*>(center.Children()[0].get());
        assert(closableToast != nullptr && closableToast->ShowClose());
        const Rect closableClose = closableToast->CloseRect();
        assert(!closableClose.Empty());
        const Point closableCenter{(closableClose.left + closableClose.right) / 2,
                                   (closableClose.top + closableClose.bottom) / 2};
        assert(center.HitTest(closableCenter) == closableToast);
        assert(center.HitTest({5, 5}) == nullptr);
        assert(center.HitTest({closableClose.left - 40, closableCenter.y}) == nullptr);

        // 点击关闭按钮结束该条消息；同样先标记、后经 Layout 移除
        assert(closableToast->OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, closableCenter)));
        assert(closableToast->OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, closableCenter)));
        assert(!closableToast->Active() && center.Count() == 1);
        center.Layout({0, 0, 400, 300});
        assert(center.Count() == 0);

        // 分组：相同文字合并为一条并累加重复计数，且出现徽标
        center.Clear();
        assert(center.Count() == 0);
        center.SetGrouping(true);
        assert(center.Grouping());
        assert(center.Show("Merged", options));
        assert(!center.Show("Merged", options)); // 命中分组，不新建
        assert(center.Count() == 1 && center.RepeatNumAt(0) == 2);
        assert(center.MessageAt(0) == "Merged");
        center.Layout({0, 0, 400, 300});
        assert(!center.BadgeRectAt(0).Empty());

        // 关闭分组后相同文字各自成条
        center.SetGrouping(false);
        assert(center.Show("Merged", options));
        assert(center.Count() == 2);

        // Bottom 停靠时向下堆叠方向反转
        center.Clear();
        center.SetPlacement(ysDui::controls::basic::DuiToastPlacement::Bottom);
        assert(center.Show("Low1", options));
        assert(center.Show("Low2", options));
        center.Layout({0, 0, 400, 300});
        const Rect low1 = center.Children()[0]->Bounds();
        const Rect low2 = center.Children()[1]->Bounds();
        assert(low1.bottom == 300 - 16);
        assert(low2.bottom == low1.top - 8);

        // 生命周期：自动消失后经 Layout 的 Purge 移除，不在回调栈内销毁
        center.Clear();
        center.SetPlacement(ysDui::controls::basic::DuiToastPlacement::Top);
        ysDui::controls::basic::DuiToastOptions shortLived;
        shortLived.appearance = ysDui::controls::basic::DuiToastAppearance::Dark;
        shortLived.durationMilliseconds = 10;
        assert(center.Show("Soon", shortLived));
        assert(center.Count() == 1);
        // 一条消息要依次走完淡入(默认 200ms) -> 停留(10ms) -> 淡出(默认 200ms)，
        // 每次 Advance 只推进当次已存在的任务，故需分步推进
        for (int step = 0; step < 4; ++step)
            centerClock.Advance(1000);
        // 结束时只在回调内做标记，控件尚未销毁（Count 仍为 1）
        assert(center.Count() == 1);
        assert(!center.Children().empty());
        // 到 Layout 这一安全时机才真正移除
        center.Layout({0, 0, 400, 300});
        assert(center.Count() == 0);
        assert(center.Children().empty());

        // Clear 立即清空
        assert(center.Show("A", options));
        assert(center.Show("B", options));
        assert(center.Count() == 2);
        center.Clear();
        assert(center.Count() == 0 && center.Children().empty() && !centerClock.HasScheduledTasks());
    }

    // DuiButton 下拉 / 分裂：窄条承担下拉动作，主区域承担主操作
    {
        using ysDui::controls::basic::DuiButton;
        using ysDui::controls::basic::DuiButtonKind;

        DuiButton dropDown;
        dropDown.SetText("More");
        dropDown.SetKind(DuiButtonKind::DropDown);
        dropDown.SetBounds({0, 0, 120, 32});
        int dropDownCalls{};
        dropDown.SetDropDownHandler([&dropDownCalls] { ++dropDownCalls; });
        // 窄条位于右侧 22px 内，主区域为其左侧
        assert((dropDown.DropDownRect() == Rect{98, 0, 120, 32}));
        assert((dropDown.MainRect() == Rect{0, 0, 98, 32}));
        // 点击任意位置都触发下拉回调
        assert(dropDown.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 16})));
        assert(dropDown.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {10, 16})));
        assert(dropDownCalls == 1);
        assert(dropDown.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {110, 16})));
        assert(dropDown.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {110, 16})));
        assert(dropDownCalls == 2);

        // 未单独设置下拉回调时回退到点击回调，便于按普通按钮方式打开菜单
        DuiButton fallback;
        fallback.SetKind(DuiButtonKind::DropDown);
        fallback.SetBounds({0, 0, 120, 32});
        int fallbackCalls{};
        fallback.SetClickHandler([&fallbackCalls] { ++fallbackCalls; });
        assert(fallback.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {60, 16})));
        assert(fallback.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {60, 16})));
        assert(fallbackCalls == 1);

        DuiButton split;
        split.SetText("Run");
        split.SetKind(DuiButtonKind::Split);
        split.SetBounds({0, 0, 140, 32});
        int primaryCalls{};
        int splitDropDownCalls{};
        split.SetClickHandler([&primaryCalls] { ++primaryCalls; });
        split.SetDropDownHandler([&splitDropDownCalls] { ++splitDropDownCalls; });
        assert((split.MainRect() == Rect{0, 0, 118, 32}));
        // 主区域 -> 主操作
        assert(split.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {40, 16})));
        assert(split.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {40, 16})));
        assert(primaryCalls == 1 && splitDropDownCalls == 0);
        // 窄条 -> 下拉
        assert(split.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {130, 16})));
        assert(split.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {130, 16})));
        assert(primaryCalls == 1 && splitDropDownCalls == 1);
        // 在窄条按下、移出后抬起不应触发任何回调
        assert(split.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {130, 16})));
        assert(split.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {-50, 16})));
        assert(primaryCalls == 1 && splitDropDownCalls == 1);

        // 绘制：窄条内出现箭头三角形，分裂按钮另有分隔线
        RecordingCanvas splitCanvas;
        split.Paint(splitCanvas, split.Bounds());
        assert(!splitCanvas.filledPaths.empty());
        RecordingCanvas dropCanvas;
        dropDown.Paint(dropCanvas, dropDown.Bounds());
        assert(!dropCanvas.filledPaths.empty());
        // 普通按钮不应有箭头
        DuiButton plain;
        plain.SetText("Plain");
        plain.SetBounds({0, 0, 120, 32});
        RecordingCanvas plainCanvas;
        plain.Paint(plainCanvas, plain.Bounds());
        assert(plainCanvas.filledPaths.empty());
    }

    using ysDui::controls::media::DuiImage;
    using ysDui::controls::media::DuiImageScaleMode;    assert((DuiImage::ComputeDrawRects(DuiImageScaleMode::None, {100, 50}, {0, 0, 60, 60}).source ==
            Rect{0, 0, 60, 50}));
    assert((DuiImage::ComputeDrawRects(DuiImageScaleMode::Stretch, {100, 50}, {0, 0, 60, 60}).destination ==
            Rect{0, 0, 60, 60}));
    assert((DuiImage::ComputeDrawRects(DuiImageScaleMode::Fit, {100, 50}, {0, 0, 60, 60}).destination ==
            Rect{0, 15, 60, 45}));
    assert((DuiImage::ComputeDrawRects(DuiImageScaleMode::Fill, {100, 50}, {0, 0, 60, 60}).source ==
            Rect{25, 0, 75, 50}));
    ysDui::render::DuiImage emptyImage;
    RecordingCanvas imageCanvas;
    imageCanvas.DrawImage(emptyImage, {1, 2, 3, 4}, {5, 6, 7, 8});
    assert(imageCanvas.images == 1);
    assert((imageCanvas.imageSource == Rect{1, 2, 3, 4}));
    assert((imageCanvas.imageDestination == Rect{5, 6, 7, 8}));
    ysDui::controls::basic::DuiBreadcrumb breadcrumb;
    breadcrumb.SetBounds({0, 0, 200, 20});
    breadcrumb.SetItems({"Home", "Settings"});
    int breadcrumbClicked = -1;
    breadcrumb.SetItemClickedHandler([&breadcrumbClicked](int index) { breadcrumbClicked = index; });
    RecordingCanvas breadcrumbCanvas;
    breadcrumb.Paint(breadcrumbCanvas, {0, 0, 200, 20});
    assert(breadcrumb.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {4, 4})));
    assert(breadcrumbClicked == 0);
    breadcrumb.SetBounds({0, 0, 90, 20});
    breadcrumb.SetItems({"Workspace", "Project", "Release", "Assets", "Icons"});
    breadcrumbCanvas.drawnTexts.clear();
    breadcrumbCanvas.drawnTextBounds.clear();
    breadcrumb.Paint(breadcrumbCanvas, {0, 0, 90, 20});
    const auto icons = std::find(breadcrumbCanvas.drawnTexts.begin(), breadcrumbCanvas.drawnTexts.end(), "Icons");
    assert(std::find(breadcrumbCanvas.drawnTexts.begin(), breadcrumbCanvas.drawnTexts.end(), "\xE2\x80\xA6")
        != breadcrumbCanvas.drawnTexts.end() && icons != breadcrumbCanvas.drawnTexts.end());
    const auto iconsIndex = static_cast<std::size_t>(icons - breadcrumbCanvas.drawnTexts.begin());
    const auto iconsBounds = breadcrumbCanvas.drawnTextBounds[iconsIndex];
    assert(breadcrumb.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {(iconsBounds.left + iconsBounds.right) / 2, 4})));
    assert(breadcrumbClicked == 4);

    {
        ysDui::controls::basic::DuiAnchor anchor;
        anchor.SetSize(ysDui::controls::basic::DuiAnchorSize::Small);
        assert(anchor.PreferredWidth() == 120);
        anchor.SetSize(ysDui::controls::basic::DuiAnchorSize::Large);
        assert(anchor.PreferredWidth() == 320);
        anchor.SetSize(ysDui::controls::basic::DuiAnchorSize::Medium);
        assert(anchor.PreferredWidth() == 200);
        anchor.SetItems({
            {"#a", "基础锚点", 1, 0},
            {"#b", "多级锚点", 2, 80},
            {"#c", "指定容器", 1, 200},
        });
        anchor.Layout({0, 0, 200, 120});
        assert(anchor.ActiveIndex() == 0);
        assert(anchor.ActiveHref() == "#a");
        int navY = -1;
        std::string clickHref;
        std::string changeCurrent;
        anchor.SetTargetOffset(5);
        anchor.SetBoundsTolerance(5);
        anchor.SetNavigateHandler([&navY](int y, std::string_view) { navY = y; });
        anchor.SetClickHandler([&clickHref](std::string_view href, std::string_view, int) {
            clickHref = std::string(href);
        });
        anchor.SetChangeHandler([&changeCurrent](std::string_view current, std::string_view) {
            changeCurrent = std::string(current);
        });
        anchor.SyncActiveFromScroll(100);
        assert(anchor.ActiveHref() == "#b");
        assert(changeCurrent == "#b");
        assert(anchor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {24, 70})));
        assert(clickHref == "#c");
        assert(navY == 195);
        assert(anchor.ActiveHref() == "#c");
        RecordingCanvas anchorCanvas;
        anchor.Paint(anchorCanvas, {0, 0, 200, 120});
        assert(std::find(anchorCanvas.drawnTexts.begin(), anchorCanvas.drawnTexts.end(), "基础锚点")
            != anchorCanvas.drawnTexts.end());
        assert(anchor.Accessibility().role == ysDui::core::DuiAccessibilityRole::List);
    }

    ysDui::controls::basic::DuiGroupBox groupBox;
    groupBox.SetTitle("Network");
    groupBox.SetPadding({2, 3, 4, 5});
    auto groupContent = std::make_unique<ysDui::controls::basic::DuiLabel>();
    auto* groupContentRaw = groupContent.get();
    groupBox.SetContent(std::move(groupContent));
    groupBox.Layout({0, 0, 100, 80});
    assert((groupContentRaw->Bounds() == Rect{2, 27, 96, 75}));
    RecordingCanvas groupCanvas;
    groupBox.Paint(groupCanvas, {0, 0, 100, 80});
    assert(groupCanvas.roundedStrokes == 1);
    assert(groupCanvas.drawnText == "Network");
    groupBox.SetCornerRadius(5);
    groupBox.Paint(groupCanvas, {0, 0, 100, 80});
    assert(groupCanvas.roundedStrokes == 2);

    // 可勾选分组框：未勾选时内容整棵子树变灰且不可交互
    {
        ysDui::controls::basic::DuiGroupBox checkable;
        checkable.SetTitle("Network");
        // 内容里再嵌一层：验证禁用会沿父链覆盖整棵子树，而不是只作用于直接子节点
        auto inner = std::make_unique<ysDui::controls::layout::DuiHBox>();
        auto innerLabel = std::make_unique<ysDui::controls::basic::DuiLabel>();
        auto innerButton = std::make_unique<ysDui::controls::basic::DuiButton>();
        auto* innerLabelRaw = innerLabel.get();
        auto* innerButtonRaw = innerButton.get();
        innerButton->SetText("Apply");
        inner->AddChild(std::move(innerLabel), ysDui::controls::layout::DuiLayoutHint{}.Fixed(40));
        inner->AddChild(std::move(innerButton), ysDui::controls::layout::DuiLayoutHint{}.Fixed(60));
        auto* innerRaw = inner.get();
        checkable.SetContent(std::move(inner));
        checkable.SetCheckable(true);

        // 默认勾选：内容可用，且分组框不进 Tab 焦点序列以外的影响
        assert(checkable.Checkable() && checkable.Checked());
        assert(innerRaw->Enabled() && innerLabelRaw->Enabled() && innerButtonRaw->Enabled());
        assert(checkable.Accessibility().role == ysDui::core::DuiAccessibilityRole::CheckBox);
        assert(checkable.Accessibility().keyboardFocusable);
        assert(checkable.Accessibility().value == "true");

        // 复选框位于标题条上、左对齐
        checkable.Layout({0, 0, 200, 100});
        const Rect checkRect = checkable.CheckBoxRect();
        assert(!checkRect.Empty());
        assert(checkRect.left == 10 && checkRect.Width() == 14 && checkRect.Height() == 14);
        assert(checkRect.top == 12 - 7);
        // 内容矩形不因复选框变化（复选框在标题条内）
        assert((innerRaw->Bounds() == Rect{12, 36, 188, 88}));

        // 点击复选框取消勾选：内容变灰
        bool lastChecked = true;
        int changes = 0;
        checkable.SetCheckChangedHandler([&lastChecked, &changes](bool checked)
        {
            lastChecked = checked;
            ++changes;
        });
        const Point checkCentre{(checkRect.left + checkRect.right) / 2, (checkRect.top + checkRect.bottom) / 2};
        // 悬停移动不声明已处理（与 DuiCheckBox 一致），但仍会更新悬停高亮
        assert(!checkable.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, checkCentre)));
        assert(checkable.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, checkCentre)));
        assert(checkable.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, checkCentre)));
        assert(!checkable.Checked() && changes == 1 && !lastChecked);
        assert(checkable.Accessibility().value == "false");
        // 禁用沿父链覆盖整棵子树
        assert(!innerRaw->Enabled() && !innerLabelRaw->Enabled() && !innerButtonRaw->Enabled());
        assert(innerRaw->GetVisualState() == VisualState::Disabled);
        // 标签没有交互，但也要随容器变灰
        RecordingCanvas disabledLabelCanvas;
        innerLabelRaw->SetBounds({0, 0, 40, 20});
        innerLabelRaw->SetText("Greyed");
        innerLabelRaw->Paint(disabledLabelCanvas, {0, 0, 40, 20});
        assert((disabledLabelCanvas.textStyle.color
                == ysDui::core::DuiTheme{}.Get(ThemeSlot::ButtonChoiceDisabledText)));

        // 命中测试在内容区直接落空，内部按钮收不到事件
        RecordingCanvas disabledCanvas;
        checkable.Paint(disabledCanvas, {0, 0, 200, 100});
        assert(disabledCanvas.roundedStrokes >= 2);
        assert(checkable.HitTest(checkCentre) == &checkable);
        assert(checkable.HitTest({(innerButtonRaw->Bounds().left + innerButtonRaw->Bounds().right) / 2,
                                 (innerButtonRaw->Bounds().top + innerButtonRaw->Bounds().bottom) / 2}) == &checkable);

        // 点击内容区不切换勾选
        assert(!checkable.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {100, 60})));
        assert(!checkable.Checked() && changes == 1);

        // 键盘：焦点在本控件时空格切换
        assert(!checkable.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Space)));
        checkable.SetFocused(true);
        assert(checkable.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Space)));
        assert(checkable.Checked() && changes == 2 && lastChecked);
        assert(innerRaw->Enabled() && innerButtonRaw->Enabled());
        checkable.SetFocused(false);

        // 无障碍动作可直接切换（经基类入口，与 UIA 提供方一致）
        assert(static_cast<ysDui::core::Control&>(checkable)
                   .PerformAccessibilityAction(ysDui::core::DuiAccessibilityAction::Invoke));
        assert(!checkable.Checked() && changes == 3);

        // 关闭可勾选：恢复内容可用，且不再暴露复选框
        checkable.SetCheckable(false);
        assert(checkable.CheckBoxRect().Empty() && innerRaw->Enabled());
        assert(checkable.Accessibility().role == ysDui::core::DuiAccessibilityRole::None);
        // 非可勾选时事件不再被接管
        assert(!checkable.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, checkCentre)));
    }

    // 祖先禁用 → 后代沿父链求值（Enabled 语义）
    {
        ysDui::controls::layout::DuiVBox outer;
        auto innerBox = std::make_unique<ysDui::controls::layout::DuiVBox>();
        auto leaf = std::make_unique<ysDui::controls::basic::DuiButton>();
        auto* leafRaw = leaf.get();
        auto* innerBoxRaw = innerBox.get();
        innerBox->AddChild(std::move(leaf), ysDui::controls::layout::DuiLayoutHint{}.Fixed(20));
        outer.AddChild(std::move(innerBox), ysDui::controls::layout::DuiLayoutHint{}.Fixed(20));
        assert(leafRaw->Enabled());
        // 禁用中间层：叶子随之禁用
        innerBoxRaw->SetEnabled(false);
        assert(!innerBoxRaw->Enabled() && !leafRaw->Enabled());
        // 只启用叶子本身不足以恢复（祖先仍禁用）
        leafRaw->SetEnabled(true);
        assert(!leafRaw->Enabled());
        // 恢复祖先即恢复
        innerBoxRaw->SetEnabled(true);
        assert(leafRaw->Enabled());
        // 叶子自身的禁用独立生效
        leafRaw->SetEnabled(false);
        assert(!leafRaw->Enabled());
        innerBoxRaw->SetEnabled(false);
        innerBoxRaw->SetEnabled(true);
        assert(!leafRaw->Enabled());
    }

    ysDui::controls::basic::DuiExpander expander;
    expander.SetContentSize({80, 40});
    expander.Layout({0, 0, 100, 100});
    assert((expander.DesiredSize() == Size{120, 80}));
    bool expandedChanged{};
    int expanderSizeChanges{};
    expander.SetExpandedChangedHandler([&expandedChanged](bool expanded) { expandedChanged = !expanded; });
    expander.SetDesiredSizeChangedHandler([&expanderSizeChanges] { ++expanderSizeChanges; });
    expander.SetAnimationClock(&clock);
    expander.SetExpanded(false, true);
    clock.Advance(100);
    assert(expander.DesiredSize().height == 35);
    clock.Advance(100);
    assert(!expander.Expanded());
    assert(expander.DesiredSize().height == 28);
    assert(expandedChanged);
    assert(expanderSizeChanges == 2);
    assert(expander.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(expander.Expanded());
    expander.SetExpanded(false, false);
    expander.SetExpanded(true, false);
    expander.Layout({0, 0, 100, 100});
    RecordingCanvas expanderCanvas;
    expander.Paint(expanderCanvas, {0, 0, 100, 100});
    assert(expanderCanvas.fills.size() == 6 && expanderCanvas.pathFillColors.size() == 1);
    assert((expanderCanvas.fills[0].color == Color{255, 255, 255, 255}));
    assert(expanderCanvas.fills[1].color == expanderCanvas.fills[0].color);
    assert((expanderCanvas.textStyle.color == Color{0, 100, 135, 255}) && expanderCanvas.textStyle.bold);
    assert((expanderCanvas.pathFillColors[0] == Color{0, 100, 135, 255}));
    for (auto iterator = expanderCanvas.fills.end() - 4; iterator != expanderCanvas.fills.end(); ++iterator)
        assert(iterator->bounds.Width() == 1 || iterator->bounds.Height() == 1);
    assert(!expander.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {10, 10})));
    assert(expander.Hovered());
    RecordingCanvas hoveredExpanderCanvas;
    expander.Paint(hoveredExpanderCanvas, {0, 0, 100, 100});
    assert((hoveredExpanderCanvas.fills[0].color == Color{218, 236, 240, 255}));
    assert((hoveredExpanderCanvas.textStyle.color == Color{0, 0, 0, 255}));
    assert((hoveredExpanderCanvas.pathFillColors[0] == Color{0, 0, 0, 255}));
    assert(!expander.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {10, 40})));
    assert(!expander.Hovered());

    AnimationClock expanderLifetimeClock;
    {
        ysDui::controls::basic::DuiExpander expanderWithActiveAnimation;
        expanderWithActiveAnimation.SetAnimationClock(&expanderLifetimeClock);
        expanderWithActiveAnimation.SetExpanded(false, true);
        assert(expanderLifetimeClock.HasScheduledTasks());
    }
    assert(!expanderLifetimeClock.HasScheduledTasks());

    ysDui::controls::basic::DuiSegmentedControl segmented;
    segmented.AddSegment("Day");
    segmented.AddSegment("Week");
    segmented.Layout({0, 0, 120, 28});
    assert((segmented.DesiredSize() == Size{120, 28}));
    assert((segmented.SegmentRect(0) == Rect{0, 0, 60, 28}));
    int selectedIndex = -1;
    segmented.SetSelectionChangedHandler([&selectedIndex](int index) { selectedIndex = index; });
    assert(segmented.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {80, 10})));
    assert(segmented.SelectedIndex() == 1 && selectedIndex == 1);
    assert(segmented.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Right)));
    assert(segmented.SelectedIndex() == 0 && selectedIndex == 0);
    assert(segmented.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Left)));
    assert(segmented.SelectedIndex() == 1 && selectedIndex == 1);
    segmented.SetEnabled(false);
    assert(!segmented.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Right)));
    segmented.SetEnabled(true);
    segmented.Layout({0, 0, 60, 28});
    assert((segmented.SegmentRect(0) == Rect{0, 0, 48, 28}));
    assert((segmented.SegmentRect(1) == Rect{48, 0, 96, 28}));
    // 焦点线框默认关闭：先验证关掉时不画，再开启后确认按主题色描出
    RecordingCanvas unfocusedRingSegmented;
    segmented.SetFocused(true);
    segmented.Paint(unfocusedRingSegmented, {0, 0, 60, 28});
    assert(unfocusedRingSegmented.roundedStrokes == 0);
    DuiTheme focusRingSegmentedTheme;
    focusRingSegmentedTheme.SetFocusRingVisible(true);
    segmented.SetTheme(&focusRingSegmentedTheme);
    RecordingCanvas segmentedCanvas;
    segmented.Paint(segmentedCanvas, {0, 0, 60, 28});
    assert((segmentedCanvas.rounded.bounds == Rect{48, 0, 60, 28}));
    assert(segmentedCanvas.roundedStrokes == 2);

    ysDui::controls::basic::DuiToolBar toolBar;
    toolBar.SetTextMeasurer(&labelCanvas);
    toolBar.AddButton("New", 1);
    toolBar.AddSeparator();
    toolBar.AddButton("Open", 2);
    toolBar.AddButton("Settings", 3);
    toolBar.Layout({0, 0, 90, 32});
    assert(toolBar.HasOverflow());
    RecordingCanvas toolBarCanvas;
    toolBar.Paint(toolBarCanvas, {0, 0, 90, 32});
    assert(std::find(toolBarCanvas.drawnTexts.begin(), toolBarCanvas.drawnTexts.end(), "\xE2\x80\xA6")
        != toolBarCanvas.drawnTexts.end());
    std::uint32_t command{};
    toolBar.SetCommandHandler([&command](std::uint32_t id) { command = id; });
    assert(toolBar.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(command == 1);
    std::vector<ysDui::controls::basic::DuiToolBarCommand> overflowed;
    toolBar.SetOverflowRequestedHandler([&overflowed](auto commands) { overflowed = std::move(commands); });
    assert(toolBar.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {85, 10})));
    assert(overflowed.size() == 2 && overflowed.front().id == 2);

    bool clicked{};
    auto button = std::make_unique<ysDui::controls::basic::DuiButton>();
    button->SetBounds({0, 0, 80, 30});
    button->SetText("Run");
    button->SetTheme(&theme);
    button->SetClickHandler([&clicked] { clicked = true; });
    ysDui::controls::basic::DuiButton* buttonRaw = button.get();
    Host buttonHost;
    buttonHost.SetRoot(std::move(button));
    assert(buttonHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(buttonRaw->Captured());
    assert(buttonHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {100, 10})));
    assert(!buttonRaw->Hovered());
    assert(buttonHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerUp, {100, 10})));
    assert(!clicked);
    assert(buttonHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(buttonHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerUp, {10, 10})));
    assert(clicked);
    clicked = false;
    assert(buttonHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(buttonHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerCancel, {10, 10})));
    assert(!buttonRaw->Captured() && !clicked);
    (void)buttonHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {10, 10}));
    assert(buttonRaw->Hovered());
    assert(!buttonHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {100, 10})));
    assert(!buttonRaw->Hovered());
    auto hoverRoot = std::make_unique<Control>();
    hoverRoot->SetBounds({0, 0, 100, 40});
    auto removedButton = std::make_unique<ysDui::controls::basic::DuiButton>();
    removedButton->SetBounds({0, 0, 40, 30});
    auto* removedButtonRaw = removedButton.get();
    hoverRoot->AddChild(std::move(removedButton));
    auto* hoverRootRaw = hoverRoot.get();
    Host removedHoverHost;
    removedHoverHost.SetRoot(std::move(hoverRoot));
    (void)removedHoverHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {10, 10}));
    (void)hoverRootRaw->RemoveChild(removedButtonRaw);
    assert(!removedHoverHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {80, 10})));
    RecordingCanvas buttonCanvas;
    buttonRaw->Paint(buttonCanvas, {0, 0, 80, 30});
    assert((buttonCanvas.rounded.bounds == Rect{0, 0, 80, 30}));
    assert(buttonCanvas.drawnText == "Run");
    auto buttonIcon = ysDui::render::DuiImage::CreateBgra8Premultiplied({2, 2}, std::vector<unsigned char>(16, 255));
    buttonRaw->SetLeadingIcon(buttonIcon);
    buttonRaw->SetLeadingIconSize({12, 12});
    buttonRaw->SetLeadingIconGap(4);
    buttonCanvas = {};
    buttonRaw->Paint(buttonCanvas, {0, 0, 80, 30});
    assert(buttonCanvas.images == 1 && buttonCanvas.textBounds.left == 26);
    auto buttonSkinImage = ysDui::render::DuiImage::CreateBgra8Premultiplied({12, 12}, std::vector<unsigned char>(576, 255));
    ysDui::controls::basic::DuiButtonSkin buttonSkin;
    buttonSkin.normal = buttonSkinImage;
    buttonSkin.hovered = buttonSkinImage;
    buttonSkin.pressed = buttonSkinImage;
    buttonSkin.disabled = buttonSkinImage;
    buttonSkin.insets = {3, 3, 3, 3};
    buttonRaw->SetLeadingIcon({});
    buttonRaw->SetSkin(buttonSkin);
    buttonCanvas = {};
    buttonRaw->Paint(buttonCanvas, {0, 0, 80, 30});
    assert(buttonRaw->Skin().normal == buttonSkinImage && buttonCanvas.images == 9);

    ysDui::controls::basic::DuiButton ghostButton;
    ghostButton.SetBounds({0, 0, 80, 30});
    ghostButton.SetText("More");
    ghostButton.SetVariant(ysDui::controls::basic::DuiButtonVariant::Ghost);
    ghostButton.SetHovered(true);
    RecordingCanvas ghostCanvas;
    ghostButton.Paint(ghostCanvas, {0, 0, 80, 30});
    assert((ghostCanvas.rounded.color == Color{226, 236, 253, 255}));
    ghostButton.SetCaptured(true);
    ghostCanvas = {};
    ghostButton.Paint(ghostCanvas, {0, 0, 80, 30});
    assert((ghostCanvas.rounded.color == Color{204, 220, 248, 255}));
    DuiTheme customButtonTheme;
    customButtonTheme.Set(ThemeSlot::ButtonGhostPressed, {12, 34, 56, 255});
    ghostButton.SetTheme(&customButtonTheme);
    ghostCanvas = {};
    ghostButton.Paint(ghostCanvas, {0, 0, 80, 30});
    assert((ghostCanvas.rounded.color == Color{12, 34, 56, 255}));

    ysDui::controls::basic::DuiCheckBox checkBox;
    checkBox.SetBounds({0, 0, 120, 32});
    assert(checkBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(checkBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {10, 10})));
    assert(checkBox.Checked());
    assert(checkBox.Accessibility().role == ysDui::core::DuiAccessibilityRole::CheckBox);
    assert(checkBox.Accessibility().keyboardFocusable);
    assert(checkBox.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Space)));
    assert(!checkBox.Checked());
    checkBox.SetVariant(ysDui::controls::basic::DuiButtonVariant::Ghost);
    RecordingCanvas choiceCanvas;
    checkBox.Paint(choiceCanvas, {0, 0, 120, 32});
    assert(choiceCanvas.rounded.bounds.Empty() && choiceCanvas.roundedStrokes == 1);
    checkBox.SetVariant(ysDui::controls::basic::DuiButtonVariant::Default);
    checkBox.SetChecked(true);
    choiceCanvas = {};
    checkBox.Paint(choiceCanvas, {0, 0, 120, 32});
    assert(choiceCanvas.roundedRadius == 0 && choiceCanvas.strokedPaths.size() == 1);
    const auto& checkCommands = choiceCanvas.strokedPaths.front().Commands();
    assert(checkCommands.size() == 3
        && checkCommands[0].type == ysDui::render::DuiPathCommandType::MoveTo
        && checkCommands[1].type == ysDui::render::DuiPathCommandType::LineTo
        && checkCommands[2].type == ysDui::render::DuiPathCommandType::LineTo);
    assert(std::find(choiceCanvas.drawnTexts.begin(), choiceCanvas.drawnTexts.end(), "v")
        == choiceCanvas.drawnTexts.end());

    ysDui::core::Control radioParent;
    auto radioA = std::make_unique<ysDui::controls::basic::DuiRadio>();
    auto radioB = std::make_unique<ysDui::controls::basic::DuiRadio>();
    auto* radioARaw = radioA.get();
    auto* radioBRaw = radioB.get();
    radioARaw->SetText("A");
    radioBRaw->SetText("B");
    radioARaw->SetRadioGroup(3);
    radioBRaw->SetRadioGroup(3);
    radioParent.AddChild(std::make_unique<ysDui::controls::basic::DuiLabel>());
    radioParent.AddChild(std::move(radioA));
    radioParent.AddChild(std::move(radioB));
    radioARaw->SetChecked(true);
    radioBRaw->SetChecked(true);
    assert(!radioARaw->Checked() && radioBRaw->Checked());
    radioARaw->SetBounds({0, 0, 80, 32});
    assert(radioARaw->OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(radioARaw->OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {10, 10})));
    assert(radioARaw->Checked() && !radioBRaw->Checked());
    assert(radioARaw->Accessibility().role == ysDui::core::DuiAccessibilityRole::RadioButton);

    // 标签禁用时取禁用文字色
    ysDui::controls::basic::DuiLabel disabledLabel;
    disabledLabel.SetBounds({0, 0, 100, 20});
    disabledLabel.SetText("Disabled");
    RecordingCanvas disabledLabelCanvas;
    disabledLabel.Paint(disabledLabelCanvas, {0, 0, 100, 20});
    const auto enabledLabelColor = disabledLabelCanvas.textStyle.color;
    disabledLabel.SetEnabled(false);
    disabledLabelCanvas = {};
    disabledLabel.Paint(disabledLabelCanvas, {0, 0, 100, 20});
    assert(disabledLabelCanvas.textStyle.color != enabledLabelColor);
    assert((disabledLabelCanvas.textStyle.color
            == ysDui::core::DuiTheme{}.Get(ThemeSlot::ButtonChoiceDisabledText)));

    ysDui::controls::basic::DuiButton focusButton;
    focusButton.SetBounds({0, 0, 120, 32});
    focusButton.SetFocused(true);
    focusButton.SetText("Save");
    focusButton.SetTextStyle({{255, 255, 255, 255}, {}, 14, true});
    RecordingCanvas buttonFocusCanvas;
    focusButton.Paint(buttonFocusCanvas, {0, 0, 120, 32});
    assert(buttonFocusCanvas.textStyle.pointSize == 14 && buttonFocusCanvas.textStyle.bold);
    // 默认无焦点线框：主色按钮只有填充，没有描边
    assert(buttonFocusCanvas.roundedStrokes == 0);
    DuiTheme focusRingButtonTheme;
    focusRingButtonTheme.SetFocusRingVisible(true);
    focusButton.SetTheme(&focusRingButtonTheme);
    RecordingCanvas themedButtonFocusCanvas;
    focusButton.Paint(themedButtonFocusCanvas, {0, 0, 120, 32});
    assert(themedButtonFocusCanvas.roundedStrokes == 2);

    DuiTheme elementButtonTheme;
    elementButtonTheme.ApplyPreset(ThemePreset::ElementLight);
    ysDui::controls::basic::DuiButton elementButton;
    elementButton.SetBounds({0, 0, 120, 32});
    elementButton.SetTheme(&elementButtonTheme);
    RecordingCanvas elementButtonCanvas;
    elementButton.Paint(elementButtonCanvas, {0, 0, 120, 32});
    assert((elementButtonCanvas.rounded.color == Color{64, 158, 255, 255}));
    elementButton.SetHovered(true);
    elementButtonCanvas = {};
    elementButton.Paint(elementButtonCanvas, {0, 0, 120, 32});
    assert((elementButtonCanvas.rounded.color == Color{102, 177, 255, 255}));
    elementButton.SetCaptured(true);
    elementButtonCanvas = {};
    elementButton.Paint(elementButtonCanvas, {0, 0, 120, 32});
    assert((elementButtonCanvas.rounded.color == Color{58, 142, 230, 255}));
    elementButton.SetEnabled(false);
    elementButtonCanvas = {};
    elementButton.Paint(elementButtonCanvas, {0, 0, 120, 32});
    assert((elementButtonCanvas.rounded.color == Color{244, 244, 245, 255}));

    // DuiChip：首选尺寸、绘制、关闭按钮与命中优先级
    {
        ysDui::controls::basic::DuiChip chip;
        chip.SetText("stl_model.step");
        chip.SetClosable(true);
        const Size chipDesired = chip.DesiredSize();
        // 左右内边距 10*2 + 关闭按钮 14 + 间距 6 + 文本估算宽度
        assert(chipDesired.height == 15 + 4 * 2);
        assert(chipDesired.width > 20 + 14 + 6);
        chip.Layout({0, 0, chipDesired.width, chipDesired.height});

        RecordingCanvas chipCanvas;
        chip.Paint(chipCanvas, {0, 0, chipDesired.width, chipDesired.height});
        // 实心变体：一次圆角填充 + 一次描边 + 关闭叉两条描边（同一个 DuiPath）
        assert((chipCanvas.rounded.bounds == Rect{0, 0, chipDesired.width, chipDesired.height}));
        // 胶囊圆角：取高度一半
        assert(chipCanvas.roundedRadius == chipDesired.height / 2);
        assert(chipCanvas.strokedPaths.size() == 1);
        assert(chip.Variant() == ysDui::controls::basic::DuiChipVariant::Filled);
        assert(chip.FillColor() == DuiTheme{}.Get(ThemeSlot::ChipFill));

        // 关闭按钮矩形紧贴右侧内边距
        const Rect chipClose = chip.CloseRect();
        assert(chipClose.right == chipDesired.width - 10 && chipClose.Height() == 14);
        assert(chip.ContentRect().right == chipClose.left - 6);

        int clicks{};
        int closes{};
        chip.SetClickHandler([&clicks] { ++clicks; });
        chip.SetCloseHandler([&closes] { ++closes; });
        const Point bodyCentre{chipDesired.width / 2, chipDesired.height / 2};
        const Point closeCentre{(chipClose.left + chipClose.right) / 2,
                                (chipClose.top + chipClose.bottom) / 2};
        assert(chip.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, bodyCentre)));
        assert(chip.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, bodyCentre)));
        assert(clicks == 1 && closes == 0);
        // 落在关闭按钮上：只触发关闭
        assert(chip.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, closeCentre)));
        assert(chip.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, closeCentre)));
        assert(clicks == 1 && closes == 1);
        // 抬起点落在控件外：不触发任何回调
        assert(chip.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, bodyCentre)));
        assert(chip.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {chipDesired.width + 40, 4})));
        assert(clicks == 1 && closes == 1);

        // Outlined：透明底 + 主色描边与文字
        chip.SetVariant(ysDui::controls::basic::DuiChipVariant::Outlined);
        assert(chip.FillColor().alpha == 0);
        assert(chip.BorderColor() == DuiTheme{}.Get(ThemeSlot::BrandPrimary));
        assert(chip.TextColor() == DuiTheme{}.Get(ThemeSlot::BrandPrimary));
        // 显式颜色覆盖变体默认值
        chip.SetFillColor({1, 2, 3, 255});
        assert((chip.FillColor() == Color{1, 2, 3, 255}));
    }

    // DuiTypingIndicator：首选尺寸、相位与脉冲绘制
    {
        ysDui::controls::feedback::DuiTypingIndicator typing;
        assert(typing.DotCount() == 3 && typing.DotRadius() == 3 && typing.Gap() == 5);
        assert((typing.DesiredSize() == Size{3 * 6 + 2 * 5, 6}));
        assert(typing.Color() == DuiTheme{}.Get(ThemeSlot::TextSubtle));

        typing.Layout({0, 0, 28, 6});
        RecordingCanvas idleCanvas;
        typing.Paint(idleCanvas, {0, 0, 28, 6});
        // 停用动画时三点同亮度
        assert(idleCanvas.ellipses.size() == 3);
        assert(idleCanvas.ellipses[0].color == idleCanvas.ellipses[2].color);
        assert((idleCanvas.ellipses[0].bounds == Rect{0, 0, 6, 6}));
        assert((idleCanvas.ellipses[2].bounds == Rect{22, 0, 28, 6}));

        // 启用动画并推进相位：各点亮度应出现差异
        AnimationClock typingClock;
        typing.SetAnimationClock(&typingClock);
        typing.SetActive(true);
        assert(typing.Active() && typingClock.HasScheduledTasks());
        typingClock.Advance(300);
        assert(typing.Phase() == 300);
        RecordingCanvas pulsingCanvas;
        typing.Paint(pulsingCanvas, {0, 0, 28, 6});
        assert(pulsingCanvas.ellipses.size() == 3);
        assert(pulsingCanvas.ellipses[0].color.alpha != pulsingCanvas.ellipses[1].color.alpha);
        // 周期推进会续排下一次动画
        typingClock.Advance(900);
        assert(typing.Phase() == 0 && typingClock.HasScheduledTasks());
        // 停用后不再排新任务
        typing.SetActive(false);
        assert(!typing.Active() && !typingClock.HasScheduledTasks());
        // 相位取模
        typing.SetPhase(-1);
        assert(typing.Phase() == 1199);
        typing.SetPhase(1300);
        assert(typing.Phase() == 100);
    }

    // DuiChatBubble：分侧、限宽、内容自适应高度与元信息行
    {
        using ysDui::controls::chat::DuiChatBubble;
        using ysDui::controls::chat::DuiChatRole;
        using ysDui::controls::chat::DuiChatStatus;

        const auto bubbleAvatar = ysDui::render::DuiImage::CreateBgra8Premultiplied(
            {2, 2}, std::vector<unsigned char>(16, 255));

        // 助手消息：靠左，头像在左，气泡取助手配色
        DuiChatBubble assistant;
        assistant.SetRole(DuiChatRole::Assistant);
        assistant.SetName("AI");
        assistant.SetAvatar(bubbleAvatar);
        assistant.SetAvatarSize(24);
        assistant.SetContent(std::make_unique<FixedSizePane>(Size{0, 40}));
        assert(assistant.Role() == DuiChatRole::Assistant);
        assert(assistant.Status() == DuiChatStatus::Complete && assistant.StatusText().empty());
        assistant.Layout({0, 0, 400, 300});

        // 头像列 24 + 间距 8；气泡占剩余宽度
        assert((assistant.AvatarRect() == Rect{0, 0, 24, 24}));
        const Rect assistantBubble = assistant.BubbleRect();
        assert(assistantBubble.left == 32 && assistantBubble.right == 400);
        // 高度 = 内容 40 + 上下内边距 8*2，再叠加元信息行 4+16
        assert(assistantBubble.Height() == 40 + 16);
        // 控件高度收缩到内容高度
        assert(assistant.Bounds().Height() == assistantBubble.Height() + 4 + 16);
        assert(assistant.MetaRect().top == assistantBubble.bottom + 4);
        // 正文内缩于气泡
        assert((assistant.ContentRect() == Rect{assistantBubble.left + 12, 8, 400 - 12, 48}));
        assert((assistant.FillColor() == DuiTheme{}.Get(ThemeSlot::ChatAssistantBubbleFill)));
        assert((assistant.BorderColor() == DuiTheme{}.Get(ThemeSlot::BorderLight)));
        assert(assistant.Accessibility().role == DuiAccessibilityRole::Group);
        assert(assistant.Accessibility().name == "AI");

        RecordingCanvas assistantCanvas;
        assistant.Paint(assistantCanvas, assistant.Bounds());
        // 头像绘制 + 气泡圆角填充 + 描边
        assert(assistantCanvas.drawnImages.size() == 1);
        assert((assistantCanvas.rounded.bounds == assistantBubble));

        // 用户消息：靠右，头像在右，元信息右对齐
        DuiChatBubble user;
        user.SetRole(DuiChatRole::User);
        user.SetAvatar(bubbleAvatar);
        user.SetAvatarSize(24);
        user.SetTimestamp("10:04");
        user.SetContent(std::make_unique<FixedSizePane>(Size{0, 20}));
        user.Layout({0, 0, 400, 300});
        assert((user.AvatarRect() == Rect{376, 0, 400, 24}));
        const Rect userBubble = user.BubbleRect();
        assert(userBubble.right == 376 - 8 && userBubble.left == 0);
        assert((user.FillColor() == DuiTheme{}.Get(ThemeSlot::ChatUserBubbleFill)));
        assert(user.StatusText().empty() && !user.MetaRect().Empty());
        // 元信息与气泡右边界对齐
        assert(user.MetaRect().right == userBubble.right);
        RecordingCanvas userCanvas;
        user.Paint(userCanvas, user.Bounds());
        // 时间戳右对齐绘制
        assert(userCanvas.textAlignment == ysDui::render::DuiTextAlignment::End);

        // 限宽：气泡宽度取 min(可用, 上限)
        user.SetMaxWidth(200);
        assert(user.MaxWidth() == 200);
        user.Layout({0, 0, 400, 300});
        assert(user.BubbleRect().Width() == 200);
        assert(user.BubbleRect().left == 376 - 8 - 200);

        // 状态文字：默认按状态给出，可被覆盖
        user.SetStatus(DuiChatStatus::Streaming);
        assert(user.StatusText() == "生成中…");
        user.SetStatus(DuiChatStatus::Failed);
        assert(user.StatusText() == "发送失败");
        user.SetStatusText("重试中");
        assert(user.StatusText() == "重试中");
        user.SetStatusText({});
        assert(user.StatusText() == "发送失败");
        RecordingCanvas failedCanvas;
        user.Layout({0, 0, 400, 300});
        user.Paint(failedCanvas, user.Bounds());
        assert(std::find(failedCanvas.drawnTexts.begin(), failedCanvas.drawnTexts.end(), "10:04 · 发送失败")
               != failedCanvas.drawnTexts.end());

        // 无头像、无元信息：气泡占满可用宽度且没有元信息行
        DuiChatBubble plain;
        plain.SetContent(std::make_unique<FixedSizePane>(Size{0, 30}));
        plain.Layout({0, 0, 200, 200});
        assert(plain.AvatarRect().Empty() && plain.MetaRect().Empty());
        assert((plain.BubbleRect() == Rect{0, 0, 200, 46}));
        assert(plain.Bounds().Height() == 46);

        // 系统提示：浅灰底 + 描边，避免在白色页面上与助手气泡一样"消失"
        DuiChatBubble system;
        system.SetRole(DuiChatRole::System);
        assert(system.BorderColor() == DuiTheme{}.Get(ThemeSlot::BorderLight));
        assert(system.FillColor() == DuiTheme{}.Get(ThemeSlot::SurfaceAlternateBackground));
        RecordingCanvas systemCanvas;
        system.Layout({0, 0, 200, 40});
        system.Paint(systemCanvas, system.Bounds());
        // 底色必须与页面底色不同，否则气泡不可见
        assert(systemCanvas.rounded.color != DuiTheme{}.Get(ThemeSlot::ChatAssistantBubbleFill));
        // 显式颜色优先于角色配色
        system.SetFillColor({9, 9, 9, 255});
        assert((system.FillColor() == Color{9, 9, 9, 255}));
        // 换内容后重新测量
        system.SetContent(std::make_unique<FixedSizePane>(Size{0, 12}));
        system.Layout({0, 0, 200, 200});
        assert(system.BubbleRect().Height() == 12 + 16);

        // 正文不报告首选高度（DuiLabel 即为 0）：沿用给定高度，不收缩成空气泡
        DuiChatBubble fallback;
        fallback.SetContent(std::make_unique<FixedSizePane>(Size{0, 0}));
        fallback.Layout({0, 0, 200, 100});
        assert(fallback.BubbleRect().Height() == 100);
        assert(fallback.Bounds().Height() == 100);
        assert(fallback.DesiredSize().height == 100);
        // 带元信息行时从给定高度里让出元信息占用的部分
        fallback.SetTimestamp("09:00");
        fallback.Layout({0, 0, 200, 100});
        assert(fallback.BubbleRect().Height() == 80);
        assert(fallback.Bounds().Height() == 100);
    }

    // DuiChatList：变高虚拟列表的测量、虚拟化、贴底跟随与滚动
    {
        using ysDui::controls::chat::DuiChatList;

        // 基础：条目高度按给定宽度测量，不溢出时不显示滚动条
        DuiChatList basic;
        basic.SetOverscan(0);
        for (int index = 0; index < 3; ++index)
            basic.AppendItem(std::make_unique<MutableHeightPane>(24));
        assert(basic.ItemCount() == 3);
        assert(basic.EstimatedItemHeight() == 72);
        basic.Layout({0, 0, 200, 100});
        // 高度取自 DesiredSize，与宽度无关；三条 24 高，不溢出
        assert(basic.ContentHeight() == 72);
        assert(basic.MeasuredItemCount() == 3);
        assert((basic.ItemRect(0) == Rect{0, 0, 200, 24}));
        assert((basic.ItemRect(2) == Rect{0, 48, 200, 72}));
        assert(basic.ScrollOffset() == 0 && basic.AtBottom());
        // 无溢出：滚动条隐藏，条目拿到完整宽度
        assert(dynamic_cast<MutableHeightPane*>(basic.Item(0))->LastLayoutWidth() == 200);
        assert(basic.IndexFromPoint({10, 10}) == 0);
        assert(basic.IndexFromPoint({10, 40}) == 1);
        assert(basic.IndexFromPoint({10, 90}) == -1);
        assert(basic.IndexFromPoint({300, 10}) == -1);

        // 虚拟化：窗口外条目不测量、不排版
        DuiChatList chatList;
        chatList.SetOverscan(0);
        // 估值取与真实高度相同，便于断言精确偏移；虚拟化本身不依赖二者的差异
        chatList.SetEstimatedItemHeight(20);
        for (int index = 0; index < 20; ++index)
            chatList.AppendItem(std::make_unique<MutableHeightPane>(20));
        chatList.Layout({0, 0, 200, 100});
        // 20 * 20 = 400 高于视口 100，出现滚动条
        assert(chatList.ContentHeight() == 400);
        assert(chatList.ScrollOffset() == 0);
        const int measuredAtTop = chatList.MeasuredItemCount();
        assert(measuredAtTop >= 6 && measuredAtTop < 20);
        // 窗口内的条目有几何，窗口外的没有
        assert((chatList.ItemRect(0) == Rect{0, 0, 183, 20}));
        assert(chatList.Item(0)->Bounds().Height() == 20);
        assert(chatList.Item(19)->Bounds().Empty());
        assert(chatList.FirstVisibleIndex() == 0);
        assert(chatList.LastVisibleIndex() >= 5);

        // 滚动到中段：只测量该段落，顶部条目几何不再更新
        chatList.SetScrollOffset(200);
        assert(chatList.ScrollOffset() == 200);
        assert(!chatList.FollowBottom() && !chatList.AtBottom());
        assert(chatList.ItemRect(10).top == 0);
        assert(chatList.Item(10)->Bounds().Height() == 20);
        assert(chatList.MeasuredItemCount() > measuredAtTop);
        // 窗口之上条目偏移在本帧内不变，测量不会把视图挤动
        assert(chatList.ScrollOffset() == 200);

        // 贴底跟随：追加条目后仍停在末尾
        chatList.ScrollToBottom();
        assert(chatList.FollowBottom() && chatList.AtBottom());
        assert(chatList.ScrollOffset() == chatList.ContentHeight() - 100);
        chatList.AppendItem(std::make_unique<MutableHeightPane>(20));
        chatList.Layout({0, 0, 200, 100});
        assert(chatList.ItemCount() == 21);
        assert(chatList.AtBottom());
        assert(chatList.ScrollOffset() == chatList.ContentHeight() - 100);

        // 流式增长：失效该条后高度变大，贴底状态仍停在末尾
        auto growing = std::make_unique<MutableHeightPane>(20);
        MutableHeightPane* growingRaw = growing.get();
        const int lastIndex = chatList.AppendItem(std::move(growing));
        chatList.Layout({0, 0, 200, 100});
        const int beforeGrow = chatList.ContentHeight();
        growingRaw->SetDesiredHeight(60);
        chatList.InvalidateItem(lastIndex);
        chatList.Layout({0, 0, 200, 100});
        assert(chatList.ContentHeight() == beforeGrow + 40);
        assert(chatList.AtBottom() && chatList.FollowBottom());

        // 移离末尾后自动关闭跟随；追加内容不再移动视图
        assert(chatList.OnEvent(ysDui::test::MakeEvent(EventType::PointerWheel, {10, 10}, 0, 0, 0, 120)));
        assert(!chatList.FollowBottom() && !chatList.AtBottom());
        const int parked = chatList.ScrollOffset();
        assert(parked == chatList.ContentHeight() - 100 - chatList.ScrollLineSize() * 3);
        chatList.AppendItem(std::make_unique<MutableHeightPane>(20));
        chatList.Layout({0, 0, 200, 100});
        assert(chatList.ScrollOffset() == parked);
        // End 键回到末尾并重新开启跟随
        assert(chatList.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {10, 10}, ysDui::core::key::End)));
        assert(chatList.FollowBottom() && chatList.AtBottom());
        // Home 键回到开头
        assert(chatList.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {10, 10}, ysDui::core::key::Home)));
        assert(chatList.ScrollOffset() == 0 && !chatList.FollowBottom());
        assert(!chatList.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {10, 10}, ysDui::core::key::Function1)));
        assert(!chatList.OnEvent(ysDui::test::MakeEvent(EventType::PointerWheel, {300, 10}, 0, 0, 0, 120)));

        // 单条目可见性
        chatList.EnsureItemVisible(19);
        assert(chatList.ItemRect(19).top == 80);
        assert(chatList.ItemRect(19).bottom == 100);

        // 结构变化：移除与清空不影响滚动条这个内部子控件
        const int countBefore = chatList.ItemCount();
        const std::size_t childrenBefore = chatList.Children().size();
        assert(childrenBefore == static_cast<std::size_t>(countBefore) + 1);
        chatList.RemoveItem(0);
        assert(chatList.ItemCount() == countBefore - 1);
        assert(chatList.Children().size() == childrenBefore - 1);
        chatList.SetItemCount(5);
        assert(chatList.ItemCount() == 5);
        assert(chatList.Children().size() == 6);
        chatList.ClearItems();
        assert(chatList.ItemCount() == 0 && chatList.ContentHeight() == 0);
        assert(chatList.Children().size() == 1);
        assert(chatList.FirstVisibleIndex() == -1);
        chatList.Layout({0, 0, 200, 100});
        RecordingCanvas emptyCanvas;
        chatList.Paint(emptyCanvas, {0, 0, 200, 100});
        assert(emptyCanvas.clips.size() == 1 && emptyCanvas.popCount == 1);
        assert(chatList.Accessibility().role == DuiAccessibilityRole::List);

        // 空位（未设置控件的条目）按估值计入高度
        DuiChatList placeholders;
        placeholders.SetOverscan(0);
        placeholders.SetEstimatedItemHeight(50);
        placeholders.SetItemCount(4);
        placeholders.Layout({0, 0, 200, 100});
        assert(placeholders.Item(0) == nullptr);
        assert(placeholders.ContentHeight() == 200);
        assert(placeholders.ItemRect(2).top == 100);
        // 显式高度覆盖估值后偏移立即重算
        placeholders.SetEstimatedItemHeight(10);
        assert(placeholders.ContentHeight() == 40);

        // 间距与行步长
        DuiChatList spaced;        spaced.SetOverscan(0);
        spaced.SetEstimatedItemHeight(30);
        spaced.SetItemSpacing(5);
        spaced.SetItemCount(3);
        spaced.Layout({0, 0, 200, 100});
        assert(spaced.ItemSpacing() == 5);
        // 末条的尾部间距不计入内容高度
        assert(spaced.ContentHeight() == 100);
        assert(spaced.ItemRect(1).top == 35);
        assert(spaced.ScrollLineSize() == 30);
        spaced.SetScrollLineSize(12);
        assert(spaced.ScrollLineSize() == 12);
        // 未读计数：只在"贴底关闭时追加"自增；回到末尾真正落零（不是只靠派生）
        {
            DuiChatList unread;
            unread.SetOverscan(0);
            unread.SetEstimatedItemHeight(20);
            unread.SetItemCount(20);
            unread.Layout({0, 0, 200, 100});
            // 初始不贴底但尚无追加：不应凭空产生未读
            assert(!unread.FollowBottom() && !unread.AtBottom() && unread.UnseenCount() == 0);

            std::vector<int> reported;
            unread.SetUnseenChangedHandler([&reported](int value) { reported.push_back(value); });
            // 未贴底追加 → 计数自增并上报
            assert(unread.AppendItem(std::make_unique<MutableHeightPane>(20)) == 20);
            assert(unread.UnseenCount() == 1);
            assert(unread.AppendItem(std::make_unique<MutableHeightPane>(20)) == 21);
            assert(unread.UnseenCount() == 2);
            assert((reported == std::vector<int>{1, 2}));
            // 计数不变时不重复上报
            unread.SetScrollOffset(unread.ScrollOffset());
            assert((reported == std::vector<int>{1, 2}));

            // 回到末尾：计数落零并上报
            unread.ScrollToBottom();
            assert(unread.AtBottom() && unread.UnseenCount() == 0);
            assert((reported == std::vector<int>{1, 2, 0}));

            // 关键：再次上滚，已读过的旧计数不得"复现"
            assert(unread.OnEvent(ysDui::test::MakeEvent(
                ysDui::core::EventType::PointerWheel, {10, 10}, 0, 0, 0, 120)));
            assert(!unread.AtBottom() && unread.UnseenCount() == 0);

            // 贴底状态下追加：内容增长但跟随仍开启 → 不算未读
            unread.ScrollToBottom();
            const int followCount = unread.ItemCount();
            assert(unread.AppendItem(std::make_unique<MutableHeightPane>(20)) == followCount);
            // 注意：追加后布局是延迟的，此刻尚未钉到新末尾；但跟随开着就不该计入未读
            assert(unread.FollowBottom() && unread.UnseenCount() == 0);
            unread.Layout({0, 0, 200, 100});
            assert(unread.AtBottom() && unread.UnseenCount() == 0);

            // 手动清除
            assert(unread.OnEvent(ysDui::test::MakeEvent(
                ysDui::core::EventType::PointerWheel, {10, 10}, 0, 0, 0, 120)));
            assert(unread.AppendItem(std::make_unique<MutableHeightPane>(20)) > 0);
            assert(unread.UnseenCount() == 1);
            unread.ClearUnseen();
            assert(unread.UnseenCount() == 0);
            unread.Layout({0, 0, 200, 100});
            assert(unread.UnseenCount() == 0);

            // 内容不足一屏：全部可见，恒为 0（无需任何改动状态的操作）
            DuiChatList fitting;
            fitting.SetOverscan(0);
            fitting.SetEstimatedItemHeight(20);
            fitting.SetItemCount(3);
            fitting.Layout({0, 0, 200, 100});
            assert(fitting.AtBottom() && fitting.UnseenCount() == 0);
            (void)fitting.AppendItem(std::make_unique<MutableHeightPane>(20));
            assert(fitting.UnseenCount() == 0);

            // 删除条目后计数不超过条目总数
            DuiChatList clamped;
            clamped.SetOverscan(0);
            clamped.SetEstimatedItemHeight(20);
            clamped.SetItemCount(20);
            clamped.Layout({0, 0, 200, 100});
            for (int index = 0; index < 3; ++index)
                (void)clamped.AppendItem(std::make_unique<MutableHeightPane>(20));
            assert(clamped.UnseenCount() == 3);
            clamped.SetItemCount(1);
            assert(clamped.UnseenCount() == 0);
            clamped.ClearItems();
            assert(clamped.UnseenCount() == 0 && clamped.ItemCount() == 0);
        }

        // 图片查看器：贴合缩放、锚点缩放、平移夹取、关闭请求与绘制裁剪
        {
            using ysDui::controls::media::DuiImageViewer;
            using ysDui::controls::media::DuiImageViewerFit;
            using ysDui::render::DuiImage;

            // 静态贴合计算（含退化输入）
            assert(DuiImageViewer::FitScale({400, 200}, {0, 0, 200, 200}, DuiImageViewerFit::Contain) == 0.5);
            assert(DuiImageViewer::FitScale({400, 200}, {0, 0, 200, 200}, DuiImageViewerFit::Cover) == 1.0);
            assert(DuiImageViewer::FitScale({400, 200}, {0, 0, 200, 200}, DuiImageViewerFit::Actual) == 1.0);
            // Actual 与视口无关；尺寸非法时退回 1.0
            assert(DuiImageViewer::FitScale({400, 200}, {}, DuiImageViewerFit::Actual) == 1.0);
            assert(DuiImageViewer::FitScale({0, 0}, {0, 0, 200, 200}, DuiImageViewerFit::Contain) == 1.0);

            // 400x200 的纯色位图：贴合与缩放都便于手算
            const auto image = DuiImage::CreateBgra8Premultiplied(
                {400, 200}, std::vector<unsigned char>(400 * 200 * 4, 255));

            DuiImageViewer viewer;
            // 无图：不崩溃，且没有图像矩形
            viewer.Layout({0, 0, 100, 100});
            assert(viewer.ImageRect().Empty());
            assert(viewer.Image() == nullptr && !viewer.ViewAdjusted());

            viewer.SetImage(image);
            viewer.Layout({0, 0, 200, 200});
            // Contain 0.5 → 200x100 居中
            assert(viewer.Zoom() == 0.5);
            assert((viewer.ImageRect() == Rect{0, 50, 200, 150}));
            assert(!viewer.ViewAdjusted());
            // 屏幕 → 图像像素
            assert((viewer.ScreenToImage({100, 100}) == Point{200, 100}));
            assert((viewer.ScreenToImage({0, 50}) == Point{0, 0}));

            // 锚点缩放：锚点下的像素保持不动
            viewer.ZoomAt({0, 50}, 2.0);
            assert(viewer.Zoom() == 1.0 && viewer.ViewAdjusted());
            assert((viewer.ImageRect() == Rect{0, 0, 400, 200}));
            assert((viewer.ScreenToImage({0, 0}) == Point{0, 0}));

            // 复位回到贴合
            viewer.ResetView();
            assert(viewer.Zoom() == 0.5 && !viewer.ViewAdjusted());
            assert((viewer.ImageRect() == Rect{0, 50, 200, 150}));

            // 缩放范围夹取（以视口中心为锚）
            viewer.SetZoomLimits(0.5, 1.5);
            assert(viewer.MinimumZoom() == 0.5 && viewer.MaximumZoom() == 1.5);
            viewer.SetZoom(10.0);
            assert(viewer.Zoom() == 1.5);
            viewer.SetZoom(0.01);
            assert(viewer.Zoom() == 0.5);
            viewer.SetZoomLimits(0.05, 16.0);

            // 平移：图像恰好装进视口时不产生位移（该轴被强制居中）
            viewer.ResetView();
            const Point centered = viewer.Offset();
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {100, 100})));
            assert(viewer.Captured());
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {140, 160})));
            assert((viewer.Offset() == centered));
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {140, 160})));
            assert(!viewer.Captured());

            // 放大到超出视口后：可以拖动，且拖动后视口仍被图像覆盖
            viewer.SetZoom(1.0);
            const Point beforePan = viewer.Offset();
            assert(viewer.ImageRect().Width() > 200);
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {100, 100})));
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {130, 140})));
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {130, 140})));
            assert(viewer.Offset().x == beforePan.x + 30);
            assert((viewer.ImageRect().left <= 0 && viewer.ImageRect().right >= 200));
            // 拖到视口之外会被夹回"图像始终覆盖视口"
            viewer.SetOffset({5000, 5000});
            assert((viewer.ImageRect().left == 0 && viewer.ImageRect().top == 0));
            viewer.SetOffset({-5000, -5000});
            assert((viewer.ImageRect().right == 200 && viewer.ImageRect().bottom == 200));

            // 滚轮缩放：向上放大、向下缩小
            viewer.ResetView();
            assert(viewer.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerWheel, {100, 100}, 0, 0, 0, 120)));
            assert(viewer.Zoom() > 0.5);
            const double zoomedIn = viewer.Zoom();
            assert(viewer.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerWheel, {100, 100}, 0, 0, 0, -120)));
            assert(viewer.Zoom() < zoomedIn);
            // 视口之外的滚轮不处理
            assert(!viewer.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerWheel, {400, 400}, 0, 0, 0, 120)));

            // 双击：贴合 ↔ 原始尺寸
            viewer.ResetView();
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerDoubleClick, {100, 100})));
            assert(viewer.Zoom() == 1.0 && viewer.ViewAdjusted());
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerDoubleClick, {100, 100})));
            assert(viewer.Zoom() == 0.5 && !viewer.ViewAdjusted());

            // 关闭请求：Escape 与背板点击都只发请求，由调用方决定是否隐藏
            int closes{};
            viewer.SetCloseRequestedHandler([&closes] { ++closes; });
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Escape)));
            assert(closes == 1);
            viewer.SetCloseOnEscape(false);
            assert(!viewer.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Escape)));
            assert(closes == 1);
            // 图片之外的背板
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
            assert(closes == 2);
            viewer.SetCloseOnBackdropClick(false);
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
            assert(closes == 2);
            // 图片之内的按下不请求关闭，而是开始平移
            viewer.SetCloseOnBackdropClick(true);
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {100, 100})));
            assert(closes == 2 && viewer.Captured());
            assert(viewer.OnEvent(ysDui::test::MakeEvent(EventType::PointerCancel, {100, 100})));
            assert(!viewer.Captured());

            // 禁用后不响应
            viewer.SetEnabled(false);
            assert(!viewer.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Escape)));
            viewer.SetEnabled(true);

            // 说明文字进入无障碍名称
            viewer.SetCaptionText("主轴刀路预览");
            assert(viewer.Accessibility().role == DuiAccessibilityRole::Image);
            assert(viewer.Accessibility().name == "主轴刀路预览");
            assert(viewer.Accessibility().keyboardFocusable);

            // 绘制：背板铺满 + 图像按变换绘制 + 裁剪在视口内
            RecordingCanvas viewerCanvas;
            viewer.Paint(viewerCanvas, {0, 0, 200, 200});
            assert(!viewerCanvas.fills.empty());
            assert(viewerCanvas.drawnImages.size() == 1);
            assert(viewerCanvas.clips.size() == 1 && viewerCanvas.popCount == 1);
            // 脏矩形为空时不绘制
            RecordingCanvas emptyViewerCanvas;
            viewer.Paint(emptyViewerCanvas, {300, 300, 400, 400});
            assert(emptyViewerCanvas.drawnImages.empty());

            // 窗口尺寸变化：未手动调整时重新贴合，已调整时保留缩放仅夹取
            viewer.SetCaptionText({});
            viewer.ResetView();
            viewer.Layout({0, 0, 400, 400});
            assert(viewer.Zoom() == 1.0);
            assert((viewer.ImageRect() == Rect{0, 100, 400, 300}));
            viewer.SetZoom(2.0);
            viewer.Layout({0, 0, 100, 100});
            assert(viewer.Zoom() == 2.0);
            assert(viewer.ViewAdjusted());

            // 复位：清空图像
            viewer.SetImage(nullptr);
            assert(viewer.Image() == nullptr && viewer.ImageRect().Empty());
        }

        // 真实正文：高度按视口宽度测量，窄宽度下同一段落更高
        {
            std::string paragraph;
            for (int index = 0; index < 200; ++index)
                paragraph += "word ";
            paragraph += "中文段落用于换行验证";
            DuiChatList responsive;
            responsive.SetOverscan(0);
            responsive.SetEstimatedItemHeight(40);
            auto markdown = std::make_unique<ysDui::controls::content::DuiMarkdownView>();
            markdown->SetContent(paragraph);
            responsive.AppendItem(std::move(markdown));
            responsive.Layout({0, 0, 420, 200});
            const int wideHeight = responsive.ContentHeight();
            assert(wideHeight > 40);
            assert(responsive.MeasuredItemCount() == 1);
            // 宽度变化使测量失效并按新宽度重测
            responsive.Layout({0, 0, 220, 200});
            assert(responsive.ContentHeight() > wideHeight);
            // 大量真实正文条目：只有窗口内的被解析与排版
            DuiChatList heavy;
            heavy.SetOverscan(0);
            heavy.SetEstimatedItemHeight(80);
            for (int index = 0; index < 50; ++index)
            {
                auto body = std::make_unique<ysDui::controls::content::DuiMarkdownView>();
                body->SetContent("## 第 " + std::to_string(index) + " 条\n\n" + paragraph);
                heavy.AppendItem(std::move(body));
            }
            heavy.Layout({0, 0, 420, 300});
            assert(heavy.MeasuredItemCount() < 50);
            assert(heavy.Item(49) != nullptr && heavy.Item(49)->Bounds().Empty());
            assert(heavy.Item(heavy.FirstVisibleIndex())->Bounds().Height() > 0);
        }
    }

    ysDui::controls::feedback::DuiProgressBar progress;    progress.SetBounds({0, 0, 100, 20});
    progress.SetRange(0, 200);
    progress.SetValue(50);
    assert((progress.ComputeFillRect() == Rect{0, 0, 25, 20}));
    progress.SetVertical(true);
    assert((progress.ComputeFillRect() == Rect{0, 0, 100, 5}));
    progress.SetMarqueePhase(-1);
    assert(progress.MarqueePhase() == 1199);
    progress.SetMarquee(true);
    assert((progress.ComputeMarqueeRect() == Rect{0, 0, 100, 20}));
    progress.SetAnimationClock(&clock);
    clock.Advance(600);
    assert(progress.MarqueePhase() == 600 && clock.HasScheduledTasks());
    clock.Advance(600);
    assert(progress.MarqueePhase() == 1199 && clock.HasScheduledTasks());
    clock.Advance(16);
    assert(progress.MarqueePhase() < 1199 && progress.MarqueePhase() > 1100);
    progress.SetVertical(false);
    progress.SetMarqueePhase(600);
    assert((progress.ComputeMarqueeRect() == Rect{20, 0, 80, 20}));
    RecordingCanvas progressCanvas;
    progress.Paint(progressCanvas, {0, 0, 100, 20});
    assert(progressCanvas.fills.size() == 2);
    progress.SetMarquee(false);
    RecordingCanvas progressTextCanvas;
    progress.Paint(progressTextCanvas, {0, 0, 100, 20});
    assert(progressTextCanvas.drawnTexts.size() == 2 && progressTextCanvas.clips.size() == 1
        && progressTextCanvas.popCount == 1);

    bool switched{};
    auto toggle = std::make_unique<ysDui::controls::input::DuiSwitch>();
    assert((toggle->DesiredSize() == Size{46, 24}));
    assert((toggle->OnColor() == Color{7, 193, 96, 255}));
    assert((toggle->OffColor() == Color{229, 229, 229, 255}));
    assert((toggle->KnobColor() == Color{255, 255, 255, 255}));
    toggle->SetBounds({0, 0, 46, 24});
    assert((toggle->ComputeKnobRect() == Rect{4, 3, 21, 20}));
    toggle->SetValueChangedHandler([&switched](bool checked) { switched = checked; });
    ysDui::controls::input::DuiSwitch* toggleRaw = toggle.get();
    Host toggleHost;
    toggleHost.SetRoot(std::move(toggle));
    assert(toggleHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(toggleHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerUp, {10, 10})));
    assert(switched && toggleRaw->Checked());
    assert((toggleRaw->ComputeKnobRect() == Rect{25, 3, 42, 20}));
    RecordingCanvas toggleCanvas;
    toggleRaw->Paint(toggleCanvas, {0, 0, 46, 24});
    assert(toggleCanvas.roundedRadius == 12 && toggleCanvas.ellipses.size() == 1 && toggleCanvas.arcs == 1);
    assert((toggleCanvas.rounded.color == Color{7, 193, 96, 255}));
    assert((toggleCanvas.ellipses.front().color == Color{255, 255, 255, 255}));
    assert((toggleCanvas.arcColor == Color{180, 180, 180, 255}) && toggleCanvas.arcWidth == 1.0F);
    toggleRaw->SetEnabled(false);
    assert(!toggleHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    RecordingCanvas disabledToggleCanvas;
    toggleRaw->Paint(disabledToggleCanvas, {0, 0, 46, 24});
    assert((disabledToggleCanvas.rounded.color == Color{155, 230, 191, 255}));
    assert((disabledToggleCanvas.arcColor == Color{225, 225, 225, 255}));
    DuiTheme customSwitchTheme;
    customSwitchTheme.Set(ThemeSlot::ToggleOn, {20, 40, 60, 255});
    toggleRaw->SetTheme(&customSwitchTheme);
    assert((toggleRaw->OnColor() == Color{20, 40, 60, 255}));
    toggleRaw->SetOnColor({30, 50, 70, 255});
    assert((toggleRaw->OnColor() == Color{30, 50, 70, 255}));

    ysDui::core::AnimationClock switchClock;
    ysDui::controls::input::DuiSwitch animatedSwitch;
    animatedSwitch.SetAnimationClock(&switchClock);
    animatedSwitch.SetChecked(true, true, false);
    assert(switchClock.HasScheduledTasks() && animatedSwitch.AnimationProgress() == 0.0);
    switchClock.Advance(75);
    assert(animatedSwitch.AnimationProgress() == 0.875);
    switchClock.Advance(75);
    assert(animatedSwitch.AnimationProgress() == 1.0 && !switchClock.HasScheduledTasks());
    animatedSwitch.SetAnimated(false);
    animatedSwitch.SetChecked(false, true, false);
    assert(switchClock.HasScheduledTasks());
    switchClock.Advance(150);
    assert(animatedSwitch.AnimationProgress() == 0.0);
    animatedSwitch.SetValueChangedHandler([&switched](bool checked) { switched = checked; });
    assert(animatedSwitch.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Space)));
    assert(animatedSwitch.Checked() && switched && animatedSwitch.AnimationProgress() == 1.0);
    ysDui::core::AnimationClock lifetimeClock;
    {
        ysDui::controls::input::DuiSwitch lifetimeSwitch;
        lifetimeSwitch.SetAnimationClock(&lifetimeClock);
        lifetimeSwitch.SetChecked(true, true, false);
        assert(lifetimeClock.HasScheduledTasks());
    }
    assert(!lifetimeClock.HasScheduledTasks());

    ysDui::core::AnimationClock movedSwitchClock;
    ysDui::controls::input::DuiSwitch movingSwitch;
    movingSwitch.SetAnimationClock(&movedSwitchClock);
    movingSwitch.SetChecked(true, true, false);
    ysDui::controls::input::DuiSwitch movedSwitch(std::move(movingSwitch));
    movedSwitchClock.Advance(150);
    assert(movedSwitch.Checked() && movedSwitch.AnimationProgress() == 1.0);
    assert(!movedSwitchClock.HasScheduledTasks());

    // DuiInfoBar：内联横幅，高度按显式换行行数计算，不依赖文本测量器
    ysDui::controls::basic::DuiInfoBar infoBar;
    assert(infoBar.Severity() == ysDui::controls::basic::DuiInfoBarSeverity::Information);
    assert(!infoBar.Closable() && infoBar.LineCount() == 1);
    infoBar.SetTitle("Authorization");
    infoBar.SetMessage("License is about to expire.");
    assert(infoBar.LineCount() == 2);
    infoBar.SetBounds({0, 0, 300, 40});
    // 关闭按钮未启用 -> 空矩形
    assert(infoBar.CloseRect().Empty());

    // 正文换成两行（显式 '\n'）后总行数 1 + 2 = 3，DesiredSize 高度随之增长
    const int twoLineHeight = infoBar.DesiredSize().height;
    infoBar.SetMessage("Line one\nLine two");
    assert(infoBar.LineCount() == 3);
    assert(infoBar.DesiredSize().height == twoLineHeight + 16);
    // 宽度交回容器决定
    assert(infoBar.DesiredSize().width == 0);

    // 关闭按钮出现后占位于右侧，内容区右边界相应左移
    infoBar.SetClosable(true);
    infoBar.SetBounds({0, 0, 300, 48});
    const Rect closeRect = infoBar.CloseRect();
    assert(!closeRect.Empty() && closeRect.left > infoBar.ContentRect().right);
    assert(infoBar.ContentRect().right < 300);

    // 点击关闭按钮：先隐藏并回调，再复原可见性供后续绘制断言
    bool infoBarClosed{};
    infoBar.SetClosedHandler([&infoBarClosed] { infoBarClosed = true; });
    const Point closeCenter{(closeRect.left + closeRect.right) / 2,
                            (closeRect.top + closeRect.bottom) / 2};
    assert(infoBar.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, closeCenter)));
    assert(infoBar.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, closeCenter)));
    assert(infoBarClosed && !infoBar.Visible());

    // 各级别绘制：强调条与图标均落在脏区内
    infoBar.SetVisible(true);
    infoBar.SetBounds({0, 0, 300, 48});
    const ysDui::controls::basic::DuiInfoBarSeverity severities[]{
        ysDui::controls::basic::DuiInfoBarSeverity::Information,
        ysDui::controls::basic::DuiInfoBarSeverity::Success,
        ysDui::controls::basic::DuiInfoBarSeverity::Warning,
        ysDui::controls::basic::DuiInfoBarSeverity::Error,
    };
    for (const auto severity : severities)
    {
        infoBar.SetSeverity(severity);
        RecordingCanvas infoCanvas;
        infoBar.Paint(infoCanvas, infoBar.Bounds());
        // 背景 + 左侧强调条
        assert(infoCanvas.fills.size() >= 2);
        assert(!infoCanvas.fills.front().bounds.Empty());
        // 级别图标：实心圆
        assert(infoCanvas.ellipses.size() == 1);
        // 标题与正文都绘制
        assert(std::find(infoCanvas.drawnTexts.begin(), infoCanvas.drawnTexts.end(), "Authorization")
               != infoCanvas.drawnTexts.end());
        assert(std::find(infoCanvas.drawnTexts.begin(), infoCanvas.drawnTexts.end(), "Line one\nLine two")
               != infoCanvas.drawnTexts.end());
    }

    // 四套预设下 Success 强调色都必须可见（非透明）——FillLight 不调用 applyDerivedColors
    for (const auto preset : {ThemePreset::Light, ThemePreset::Dark,
                              ThemePreset::HighContrast, ThemePreset::ElementLight})
    {
        DuiTheme presetTheme;
        presetTheme.ApplyPreset(preset);
        const Color success = presetTheme.Get(ThemeSlot::MessageSuccessFill);
        assert(success.alpha > 0);
    }

    ysDui::controls::window::DuiNavigationView nav;
    nav.AddItem("Home");
    nav.AddItem("Settings");
    nav.SetSelectedIndex(1);
    auto navContent = std::make_unique<ysDui::controls::basic::DuiLabel>();
    navContent->SetText("Settings page");
    nav.SetContent(std::move(navContent));
    nav.Layout({0, 0, 400, 200});
    assert(nav.ItemCount() == 2 && nav.SelectedIndex() == 1 && nav.ItemText(0) == "Home");
    assert(nav.Content() != nullptr);
    assert(nav.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(nav.SelectedIndex() == 0);
    nav.SetPaneDisplayMode(ysDui::controls::window::DuiNavigationPaneDisplayMode::LeftCompact);
    assert(nav.PaneRect().Width() == 48);

    ysDui::controls::window::DuiTeachingTip tip;
    tip.SetTitle("New command");
    tip.SetMessage("Try the sidebar.");
    tip.SetActionText("Got it");
    assert(tip.Title() == "New command" && tip.ActionText() == "Got it" && !tip.Visible());

    // 新手引导：未显示时穿透命中；生效时独占命中并逐步流转，末步完成后自动关闭
    ysDui::controls::window::DuiGuide guide;
    guide.SetSteps({
        {{0, 0, 40, 20}, "One", "First", ysDui::ui::DuiPopupPlacement::Below, -1},
        {{0, 40, 40, 60}, "Two", "Second", ysDui::ui::DuiPopupPlacement::Below, -1},
    });
    int guideChanges{};
    int guideFinishes{};
    int guideSkips{};
    guide.SetChangeHandler([&guideChanges](int, int) { ++guideChanges; });
    guide.SetFinishHandler([&guideFinishes](int, int) { ++guideFinishes; });
    guide.SetSkipHandler([&guideSkips](int, int) { ++guideSkips; });
    guide.Layout({0, 0, 200, 160});
    assert(guide.StepCount() == 2 && !guide.Active() && guide.HitTest({1, 1}) == nullptr);

    guide.Show();
    assert(guide.Active() && guide.HitTest({1, 1}) == &guide);
    guide.Next();
    assert(guide.Current() == 1 && guideChanges == 1);
    guide.Prev();
    assert(guide.Current() == 0 && guideChanges == 2);
    guide.Next();
    guide.Next();
    assert(!guide.Active() && guideFinishes == 1 && guideChanges == 3 && guide.HitTest({1, 1}) == nullptr);

    guide.Show();
    guide.Skip();
    assert(!guide.Active() && guideSkips == 1);

    // 最后一步同时出现跳过 / 上一步 / 完成：固定宽度按钮必须仍落在引导框内（曾溢出卡片右缘）
    guide.SetDefaultCurrent(1);
    guide.Show();
    const ysDui::core::Control& guideCard = *guide.Children().front();
    const ysDui::core::Rect guideFooter = guideCard.Children().back()->Bounds();
    assert(guideFooter.right <= guideCard.Bounds().right - 16
           && guideFooter.left >= guideCard.Bounds().left + 16);
    guide.Hide();
    assert(!guide.Active() && guide.HitTest({1, 1}) == nullptr);

    // 水印：默认属性、空内容与矩形平铺
    ysDui::controls::basic::DuiWatermark watermark;
    assert(watermark.Alpha() == 1.0 && watermark.Repeat());
    assert(watermark.TileLayout() == ysDui::controls::basic::DuiWatermarkLayout::Rectangular);
    assert((watermark.Gap() == Size{100, 100}));
    assert((watermark.Offset() == Point{50, 50})); // 默认偏移取间距的一半
    assert((watermark.ContentSize() == Size{0, 0}));
    assert(watermark.LineSpace() == 16 && watermark.Rotate() == -22);
    assert(watermark.Texts().empty() && watermark.Image().image == nullptr);

    watermark.SetBounds({0, 0, 100, 60});
    RecordingCanvas emptyWatermarkCanvas;
    watermark.Paint(emptyWatermarkCanvas, {0, 0, 100, 60});
    assert(emptyWatermarkCanvas.drawnTexts.empty() && emptyWatermarkCanvas.images == 0);

    // 矢量路径用例固定 rotate=0，与旋转烘焙路径分开验证
    watermark.SetRotate(0);
    watermark.SetText({"WM"});
    watermark.SetGap({0, 0});
    watermark.SetBounds({0, 0, 16, 10});
    RecordingCanvas watermarkCanvas;
    watermark.Paint(watermarkCanvas, {0, 0, 16, 10});
    assert(watermarkCanvas.drawnTexts.size() == 1 && watermarkCanvas.drawnText == "WM");
    assert((watermarkCanvas.textBounds == Rect{0, 0, 16, 10}));
    assert(watermarkCanvas.textAlignment == ysDui::render::DuiTextAlignment::Center && !watermarkCanvas.wrapped);
    // 平铺内容必须先裁剪到 Bounds，避免覆盖相邻控件
    assert(watermarkCanvas.clips.size() == 1 && watermarkCanvas.popCount == 1
           && (watermarkCanvas.clips.front() == Rect{0, 0, 16, 10}));
    // 纯绘制覆盖层：指针输入必须穿透到下层内容
    assert(watermark.HitTest({1, 1}) == nullptr);

    // 整体不透明度按通道缩放文字颜色，并截断到 [0,1]
    watermark.SetAlpha(0.4);
    RecordingCanvas fadedCanvas;
    watermark.Paint(fadedCanvas, {0, 0, 16, 10});
    assert(fadedCanvas.drawnTextStyles.size() == 1 && fadedCanvas.drawnTextStyles.front().color.alpha == 10);
    watermark.SetAlpha(2.0);
    assert(watermark.Alpha() == 1.0);
    watermark.SetAlpha(-1.0);
    assert(watermark.Alpha() == 0.0);
    watermark.SetAlpha(1.0);

    // 固定内容尺寸后的网格平铺：10x10 步长铺满 100x100
    watermark.SetText({"W"});
    watermark.SetContentSize({10, 10});
    watermark.SetOffset({0, 0});
    watermark.SetBounds({0, 0, 100, 100});
    RecordingCanvas gridCanvas;
    watermark.Paint(gridCanvas, {0, 0, 100, 100});
    assert(gridCanvas.drawnTexts.size() == 100);
    for (const Rect& tile : gridCanvas.drawnTextBounds)
        assert(tile.left % 10 == 0);
    watermark.SetTileLayout(ysDui::controls::basic::DuiWatermarkLayout::Hexagonal);
    RecordingCanvas hexCanvas;
    watermark.Paint(hexCanvas, {0, 0, 100, 100});
    bool shiftedRow{};
    for (const Rect& tile : hexCanvas.drawnTextBounds)
        shiftedRow = shiftedRow || tile.left % 10 != 0;
    assert(shiftedRow); // 六边形排布存在水平错位的奇数行
    watermark.SetTileLayout(ysDui::controls::basic::DuiWatermarkLayout::Rectangular);

    // 显式内容尺寸大于实测内容时，内容在块内居中
    watermark.SetContentSize({40, 20});
    watermark.SetBounds({0, 0, 40, 20});
    RecordingCanvas centeredCanvas;
    watermark.Paint(centeredCanvas, {0, 0, 40, 20});
    assert(centeredCanvas.drawnTexts.size() == 1 && (centeredCanvas.textBounds == Rect{0, 5, 40, 15}));

    // 多行文字按 lineSpace 自上而下堆叠
    watermark.SetContentSize({0, 0});
    watermark.SetTexts({{"A"}, {"B"}});
    watermark.SetLineSpace(4);
    watermark.SetBounds({0, 0, 8, 24});
    RecordingCanvas stackedCanvas;
    watermark.Paint(stackedCanvas, {0, 0, 8, 24});
    assert(stackedCanvas.drawnTexts.size() == 2 && stackedCanvas.drawnTexts[0] == "A"
           && stackedCanvas.drawnTexts[1] == "B");
    assert(stackedCanvas.drawnTextBounds[1].top - stackedCanvas.drawnTextBounds[0].top == 14);

    // 关闭重复时只在偏移位置画一块水印
    watermark.SetText({"S"});
    watermark.SetRepeat(false);
    watermark.SetOffset({0, 0});
    watermark.SetBounds({0, 0, 100, 100});
    RecordingCanvas singleCanvas;
    watermark.Paint(singleCanvas, {0, 0, 100, 100});
    assert(singleCanvas.drawnTexts.size() == 1 && (singleCanvas.textBounds == Rect{0, 0, 8, 10}));
    watermark.SetRepeat(true);

    // 图片水印：灰度与整体透明度都会派生出绘制用位图
    const std::vector<unsigned char> opaquePixels(2 * 2 * 4, 255);
    const auto watermarkImage = ysDui::render::DuiImage::CreateBgra8Premultiplied({2, 2}, opaquePixels);
    assert(watermarkImage != nullptr);
    watermark.SetTexts({});
    watermark.SetImage({watermarkImage, false});
    watermark.SetBounds({0, 0, 2, 2});
    RecordingCanvas watermarkImageCanvas;
    watermark.Paint(watermarkImageCanvas, {0, 0, 2, 2});
    assert(watermarkImageCanvas.images == 1 && watermarkImageCanvas.lastImage == watermarkImage.get()
           && (watermarkImageCanvas.imageDestination == Rect{0, 0, 2, 2}));

    watermark.SetImage({watermarkImage, true});
    RecordingCanvas grayCanvas;
    watermark.Paint(grayCanvas, {0, 0, 2, 2});
    assert(grayCanvas.images == 1 && grayCanvas.lastImage != watermarkImage.get()
           && (grayCanvas.lastImage->Size() == Size{2, 2}));
    assert(watermark.Image().isGrayScale);

    watermark.SetAlpha(0.5);
    RecordingCanvas dimCanvas;
    watermark.Paint(dimCanvas, {0, 0, 2, 2});
    assert(dimCanvas.images == 1 && dimCanvas.lastImage != watermarkImage.get());

    // 整体完全透明时不产生任何绘制
    watermark.SetAlpha(0.0);
    RecordingCanvas invisibleCanvas;
    watermark.Paint(invisibleCanvas, {0, 0, 2, 2});
    assert(invisibleCanvas.images == 0 && invisibleCanvas.drawnTexts.empty());
    watermark.SetAlpha(1.0);
    watermark.SetImage({});
    assert(watermark.Image().image == nullptr);

    // 旋转：栅格化后 DrawImage，包围盒因旋转而扩大
    watermark.SetText({"R"});
    watermark.SetContentSize({20, 10});
    watermark.SetRotate(-30);
    watermark.SetGap({0, 0});
    watermark.SetOffset({0, 0});
    watermark.SetRepeat(false);
    watermark.SetBounds({0, 0, 80, 80});
    RecordingCanvas rotatedCanvas;
    watermark.Paint(rotatedCanvas, {0, 0, 80, 80});
    assert(watermark.Rotate() == -30);
    assert(rotatedCanvas.images == 1 && rotatedCanvas.drawnTexts.empty());
    assert(rotatedCanvas.imageDestination.Width() > 20 || rotatedCanvas.imageDestination.Height() > 10);
}
