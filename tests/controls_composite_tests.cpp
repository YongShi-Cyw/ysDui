/**
 * 文件名：controls_composite_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：验证菜单、树、提示、对话框和复合控件行为。
 */
#include "test_support.hpp"

int main()
{
    using namespace ysDui::core;
    RecordingCanvas canvas;

    ysDui::core::AnimationClock toolTipClock;
    PopupHostMock toolTipPopup;
    ysDui::controls::feedback::DuiToolTipManager toolTip;
    ysDui::core::Control toolTipControl;
    toolTip.SetAnimationClock(&toolTipClock);
    toolTip.SetPopupHost(&toolTipPopup);
    toolTip.SetTextMeasurer(&canvas);
    toolTip.Register(&toolTipControl, "Helpful text");
    toolTip.OnHover(&toolTipControl, {30, 40});
    toolTipClock.Advance(499);
    assert(!toolTip.Showing());
    toolTipClock.Advance(1);
    assert(toolTip.Showing() && toolTip.ShowingText() == "Helpful text");
    assert((toolTipPopup.options.anchor == ysDui::core::Rect{42, 58, 42, 58}));
    assert((toolTipPopup.options.size == ysDui::core::Size{112, 18}));
    RecordingCanvas toolTipCanvas;
    toolTipPopup.paint(toolTipCanvas, {0, 0, 112, 18});
    assert(!toolTipCanvas.fills.empty()
        && (toolTipCanvas.fills.back().color == ysDui::core::Color{255, 255, 225, 255}));
    assert((toolTipCanvas.strokeColor == ysDui::core::Color{120, 120, 120, 255}));
    assert((toolTipCanvas.textStyle.color == ysDui::core::Color{40, 40, 40, 255}));
    toolTip.OnLeave(&toolTipControl);
    assert(!toolTip.Showing() && toolTipPopup.requestedHide);

    ysDui::core::Host toolTipHost;
    auto toolTipRoot = std::make_unique<ysDui::core::Control>();
    toolTipRoot->SetBounds({0, 0, 200, 40});
    auto firstTipTarget = std::make_unique<ysDui::core::Control>();
    firstTipTarget->SetBounds({0, 0, 80, 40});
    auto* firstTipTargetRaw = firstTipTarget.get();
    auto secondTipTarget = std::make_unique<ysDui::core::Control>();
    secondTipTarget->SetBounds({100, 0, 180, 40});
    auto* secondTipTargetRaw = secondTipTarget.get();
    toolTipRoot->AddChild(std::move(firstTipTarget));
    toolTipRoot->AddChild(std::move(secondTipTarget));
    toolTipHost.SetRoot(std::move(toolTipRoot));
    ysDui::core::AnimationClock automaticToolTipClock;
    PopupHostMock automaticToolTipPopup;
    ysDui::controls::feedback::DuiToolTipManager automaticToolTip;
    automaticToolTip.SetAnimationClock(&automaticToolTipClock);
    automaticToolTip.SetPopupHost(&automaticToolTipPopup);
    automaticToolTip.SetHost(&toolTipHost);
    automaticToolTip.Register(firstTipTargetRaw, "First hint");
    automaticToolTip.Register(secondTipTargetRaw, "Second hint");
    toolTipHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {10, 10}));
    automaticToolTipClock.Advance(250);
    toolTipHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {110, 10}));
    automaticToolTipClock.Advance(499);
    assert(!automaticToolTip.Showing());
    toolTipHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {110, 10}));
    assert(!automaticToolTipClock.HasScheduledTasks());
    toolTipHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {10, 10}));
    automaticToolTipClock.Advance(500);
    assert(automaticToolTip.Showing() && automaticToolTip.ShowingText() == "First hint");
    toolTipHost.SetRoot({});
    assert(!automaticToolTip.Showing() && !automaticToolTipClock.HasScheduledTasks());
    automaticToolTip.SetHost(nullptr);
    assert(!automaticToolTip.Showing() && !automaticToolTipClock.HasScheduledTasks());

    UiHostFactoryMock menuFactory;
    ysDui::controls::list::DuiMenu menu;
    menu.SetTextMeasurer(&canvas);
    menu.AddItem(10, "&Open");
    menu.AddSeparator();
    menu.AddCheckItem(20, "&Pinned");
    menu.AddItem(30, "&Disabled");
    menu.SetItemEnabled(30, false);
    std::uint32_t invokedMenuItem{};
    menu.SetItemInvokedHandler([&invokedMenuItem](std::uint32_t id) { invokedMenuItem = id; });
    std::uint32_t subscribedMenuItem{};
    int menuClosed{};
    {
        auto invokedSubscription = menu.SubscribeItemInvokedScoped(
            [&subscribedMenuItem](std::uint32_t id) { subscribedMenuItem = id; });
        auto closedSubscription = menu.SubscribeClosedScoped([&menuClosed] { ++menuClosed; });
        assert(invokedSubscription && closedSubscription);
        assert(menu.Show(menuFactory, {}, {10, 20, 10, 20}));
        assert(menu.Visible() && menuFactory.popup != nullptr);
        assert(menuFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {32, 20})));
        assert(invokedMenuItem == 10 && subscribedMenuItem == 10 && menuClosed == 1 && !menu.Visible());
        assert(menu.Show(menuFactory, {}, {10, 20, 10, 20}));
        Event menuTextInput = ysDui::test::MakeEvent(EventType::TextInput);
        menuTextInput.text = "p";
        assert(menuFactory.popup->Dispatch(menuTextInput));
        assert(invokedMenuItem == 20 && subscribedMenuItem == 20 && menuClosed == 2 && menu.ItemChecked(20));
    }
    assert(!menu.SubscribeItemInvokedScoped({}));
    assert(!menu.SubscribeClosedScoped({}));
    assert(menu.Show(menuFactory, {}, {10, 20, 10, 20}));
    menu.Hide();
    assert(menuClosed == 2);

    ysDui::controls::list::DuiMenu fileMenu;
    ysDui::controls::list::DuiMenu editMenu;
    fileMenu.AddItem(41, "Open");
    editMenu.AddItem(42, "Copy");
    ysDui::controls::list::DuiMenuBar menuBar;
    menuBar.SetPopupContext(menuFactory, {});
    menuBar.SetTextMeasurer(&canvas);
    menuBar.AddItem(1, "&File", &fileMenu);
    menuBar.AddItem(2, "&Edit", &editMenu);
    menuBar.Layout({0, 0, 160, 24});
    std::uint32_t menuBarCommand{};
    menuBar.SetCommandHandler([&menuBarCommand](std::uint32_t id) { menuBarCommand = id; });
    assert(menuBar.ProcessMnemonic('f') && menuBar.ActiveIndex() == 0);
    assert(menuBar.ProcessMnemonic('e') && menuBar.ActiveIndex() == 1);
    assert(menuFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {20, 10})));
    assert(menuBarCommand == 42 && menuBar.ActiveIndex() == -1);
    assert(menuBar.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {menuBar.ItemRect(1).left + 1, 1})));
    assert(menuBar.ActiveIndex() == 1);
    menuFactory.popup->RequestHide();
    assert(menuBar.ActiveIndex() == -1);

    ysDui::controls::list::DuiTreeView tree;
    const int rootId = tree.AddRoot("Root", 1);
    const int firstChildId = tree.AddChild(rootId, "First", 2);
    const int secondChildId = tree.AddChild(rootId, "Second", 3);
    assert(rootId > 0 && tree.VisibleCount() == 3 && tree.ChildCount(rootId) == 2);
    tree.CollapseAll();
    assert(tree.VisibleCount() == 1 && !tree.Expanded(rootId));
    tree.ExpandAll();
    assert(tree.IdAtVisibleRow(1) == firstChildId && tree.IdAtVisibleRow(2) == secondChildId);
    tree.Layout({0, 0, 160, 100});
    std::vector<int> treeSelections;
    tree.SetSelectionChangedHandler([&treeSelections](int id) { treeSelections.push_back(id); });
    std::vector<int> treeHovers;
    tree.SetHoverChangedHandler([&treeHovers](int id) { treeHovers.push_back(id); });
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {10, 5})));
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {32, 35})));
    assert(!tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {200, 35})));
    assert((tree.HoveredId() == -1 && treeHovers == std::vector<int>{rootId, firstChildId, -1}));
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {32, 35})));
    assert(tree.SelectedId() == firstChildId && tree.Value(firstChildId) == 2);
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Down)));
    assert(tree.SelectedId() == secondChildId && treeSelections.size() == 2);
    tree.SetMultiSelect(true);
    assert(tree.MultiSelect());
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {32, 5}, {}, modifier::Control)));
    assert(tree.IsSelected(rootId) && tree.IsSelected(secondChildId) && tree.SelectedIds().size() == 2);
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {32, 35}, {}, modifier::Shift)));
    assert(tree.SelectedIds().size() == 2 && tree.IsSelected(firstChildId));
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Home)));
    assert(tree.SelectedId() == rootId);
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Space)));
    assert(!tree.Expanded(rootId));
    tree.ClearSelection();
    assert(tree.SelectedId() == -1 && tree.SelectedIds().empty());
    tree.SetMultiSelect(false);
    tree.SetItemSelectable(secondChildId, false);
    tree.SetSelectedId(secondChildId);
    assert(tree.SelectedId() == -1);
    const auto treeAccessibility = tree.Accessibility();
    assert(treeAccessibility.role == ysDui::core::DuiAccessibilityRole::Tree
        && ysDui::core::HasAccessibilityPattern(treeAccessibility.patterns,
                                                 ysDui::core::DuiAccessibilityPattern::ExpandCollapse));
    tree.SetItemSelectable(secondChildId, true);
    tree.SetExpanded(rootId, true);
    tree.SetFilter([firstChildId](int id) { return id == firstChildId; });
    assert(tree.VisibleCount() == 2 && tree.VisibleRow(firstChildId) == 1
        && tree.VisibleRow(rootId) == 0);
    const auto treeIcon = ysDui::render::DuiImage::CreateBgra8Premultiplied({1, 1}, {255, 0, 0, 255});
    tree.ClearFilter();
    tree.SetIcon(secondChildId, treeIcon);
    RecordingCanvas treeCanvas;
    tree.Paint(treeCanvas, {0, 0, 160, 100});
    assert(tree.IconAt(secondChildId) == treeIcon && treeCanvas.images == 1
        && treeCanvas.imageDestination.Width() == 18 && treeCanvas.imageDestination.Height() == 18
        && treeCanvas.ellipses.empty());
    assert(treeCanvas.roundedStrokes >= 1 && treeCanvas.strokeBounds == tree.Bounds());
    assert(treeCanvas.filledPaths.size() == 1);
    const auto& treeGlyph = treeCanvas.filledPaths.front().Commands();
    assert(treeGlyph.size() == 4);
    const int treeGlyphWidth = (std::max)({treeGlyph[0].point.x, treeGlyph[1].point.x,
                                          treeGlyph[2].point.x})
        - (std::min)({treeGlyph[0].point.x, treeGlyph[1].point.x, treeGlyph[2].point.x});
    const int treeGlyphHeight = (std::max)({treeGlyph[0].point.y, treeGlyph[1].point.y,
                                           treeGlyph[2].point.y})
        - (std::min)({treeGlyph[0].point.y, treeGlyph[1].point.y, treeGlyph[2].point.y});
    assert(treeGlyphWidth == treeGlyphHeight * 2);
    tree.SetExpanded(rootId, false);
    RecordingCanvas collapsedTreeCanvas;
    tree.Paint(collapsedTreeCanvas, {0, 0, 160, 100});
    const auto& collapsedTreeGlyph = collapsedTreeCanvas.filledPaths.front().Commands();
    const int collapsedTreeGlyphWidth = collapsedTreeGlyph[1].point.x - collapsedTreeGlyph[0].point.x;
    const int collapsedTreeGlyphHeight = collapsedTreeGlyph[2].point.y - collapsedTreeGlyph[0].point.y;
    assert(collapsedTreeGlyphHeight == collapsedTreeGlyphWidth * 2);
    tree.SetExpanded(rootId, true);
    tree.SetStatusColor(secondChildId, {42, 167, 92, 255});
    RecordingCanvas treeStatusCanvas;
    tree.Paint(treeStatusCanvas, {0, 0, 160, 100});
    assert(treeStatusCanvas.ellipses.size() == 1);
    tree.SetIconMuted(secondChildId, true);
    RecordingCanvas treeMutedCanvas;
    tree.Paint(treeMutedCanvas, {0, 0, 160, 100});
    assert(tree.IconMuted(secondChildId) && treeMutedCanvas.lastImage != treeIcon.get());
    tree.SetIconMuted(secondChildId, false);
    RecordingCanvas treeColorCanvas;
    tree.Paint(treeColorCanvas, {0, 0, 160, 100});
    assert(!tree.IconMuted(secondChildId) && treeColorCanvas.lastImage == treeIcon.get());
    tree.SetSubtitle(secondChildId, "Last active 2 minutes ago");
    RecordingCanvas treeSubtitleCanvas;
    tree.Paint(treeSubtitleCanvas, {0, 0, 160, 100});
    assert(tree.Subtitle(secondChildId) == "Last active 2 minutes ago"
        && std::find(treeSubtitleCanvas.drawnTexts.begin(), treeSubtitleCanvas.drawnTexts.end(), "Second")
            != treeSubtitleCanvas.drawnTexts.end()
        && std::find(treeSubtitleCanvas.drawnTexts.begin(), treeSubtitleCanvas.drawnTexts.end(), "Last active 2 minutes ago")
            != treeSubtitleCanvas.drawnTexts.end());
    tree.SetSubtitle(secondChildId, {});
    assert(tree.Subtitle(secondChildId).empty());
    ysDui::controls::list::DuiTreeColumn doneColumn{"Done", 56};
    doneColumn.kind = ysDui::controls::list::DuiTreeColumnKind::CheckBox;
    tree.AddColumn(std::move(doneColumn));
    ysDui::controls::list::DuiTreeColumn progressColumn{"Progress", 80};
    progressColumn.kind = ysDui::controls::list::DuiTreeColumnKind::ProgressBar;
    tree.AddColumn(std::move(progressColumn));
    ysDui::controls::list::DuiTreeColumn linkColumn{"Link", 60};
    linkColumn.kind = ysDui::controls::list::DuiTreeColumnKind::Hyperlink;
    tree.AddColumn(std::move(linkColumn));
    ysDui::controls::list::DuiTreeColumn iconColumn{"Icon", 40};
    iconColumn.kind = ysDui::controls::list::DuiTreeColumnKind::Icon;
    tree.AddColumn(std::move(iconColumn));
    tree.SetCellProgress(firstChildId, 1, 60);
    tree.SetCellLink(firstChildId, 2, "open", "https://example.com/open");
    tree.SetCellImage(firstChildId, 3, treeIcon);
    std::string clickedTreeLink;
    tree.SetLinkClickedHandler([&clickedTreeLink](int, int, std::string_view url) { clickedTreeLink = url; });
    int clickedTreeColumn{-1};
    int clickedTreeDirection{};
    tree.SetColumnClickedHandler([&clickedTreeColumn, &clickedTreeDirection](int column, int direction)
    {
        clickedTreeColumn = column;
        clickedTreeDirection = direction;
    });
    bool clickedTreeChecked{};
    int clickedTreeProgress{-1};
    tree.SetCellCheckedChangedHandler([&clickedTreeChecked](int, int, bool checked) { clickedTreeChecked = checked; });
    tree.SetCellProgressChangedHandler([&clickedTreeProgress](int, int, int progress) { clickedTreeProgress = progress; });
    tree.Layout({0, 0, 160, 100});
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {30, 5})));
    assert(tree.SortColumn() == 0 && tree.SortDirection() == 1 && clickedTreeColumn == 0 && clickedTreeDirection == 1);
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {30, 5})));
    assert(tree.SortDirection() == -1 && clickedTreeDirection == -1);
    const int firstChildY = tree.HeaderHeight() + tree.VisibleRow(firstChildId) * tree.RowHeight() + tree.RowHeight() / 2;
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {30, firstChildY})));
    assert(tree.CellChecked(firstChildId, 0) && clickedTreeChecked);
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {96, firstChildY})));
    assert(tree.CellProgress(firstChildId, 1) > 0 && clickedTreeProgress == tree.CellProgress(firstChildId, 1));
    assert(tree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {145, firstChildY})));
    assert(clickedTreeLink == "https://example.com/open");
    RecordingCanvas treeCellImageCanvas;
    tree.Paint(treeCellImageCanvas, {0, 0, 160, 100});
    assert(tree.CellImageAt(firstChildId, 3) == treeIcon && treeCellImageCanvas.images > 0);
    tree.Remove(secondChildId);
    assert(!tree.IconAt(secondChildId));
    tree.Remove(firstChildId);
    assert(tree.CellProgress(firstChildId, 1) == 0 && tree.CellLinkUrl(firstChildId, 2).empty()
        && !tree.CellImageAt(firstChildId, 3));
    tree.Clear();
    assert(tree.VisibleCount() == 0 && tree.SortColumn() == -1 && tree.SortDirection() == 0);
    const int rebuiltRootId = tree.AddRoot("Rebuilt");
    assert(rebuiltRootId == 1);

    ysDui::controls::list::DuiTreeView p1Tree;
    const int checkRoot = p1Tree.AddRoot("Checks");
    const int checkFirst = p1Tree.AddChild(checkRoot, "First");
    const int checkSecond = p1Tree.AddChild(checkRoot, "Second");
    p1Tree.SetNodeChecksVisible(true);
    std::vector<std::pair<int, ysDui::controls::list::DuiTreeCheckState>> checkChanges;
    p1Tree.SetNodeCheckChangedHandler([&checkChanges](int id, ysDui::controls::list::DuiTreeCheckState state)
    {
        checkChanges.emplace_back(id, state);
    });
    p1Tree.SetNodeCheckState(checkFirst, ysDui::controls::list::DuiTreeCheckState::Checked);
    assert(p1Tree.NodeCheckState(checkRoot) == ysDui::controls::list::DuiTreeCheckState::Mixed);
    p1Tree.SetNodeCheckState(checkSecond, ysDui::controls::list::DuiTreeCheckState::Checked);
    assert(p1Tree.NodeCheckState(checkRoot) == ysDui::controls::list::DuiTreeCheckState::Checked);
    p1Tree.SetNodeCheckState(checkRoot, ysDui::controls::list::DuiTreeCheckState::Unchecked);
    assert(p1Tree.NodeCheckState(checkFirst) == ysDui::controls::list::DuiTreeCheckState::Unchecked
        && p1Tree.NodeCheckState(checkSecond) == ysDui::controls::list::DuiTreeCheckState::Unchecked
        && checkChanges.size() == 3);

    ysDui::controls::list::DuiTreeView dragTree;
    const int dragRoot = dragTree.AddRoot("Root");
    const int dragFirst = dragTree.AddChild(dragRoot, "First");
    const int dragSecond = dragTree.AddChild(dragRoot, "Second");
    dragTree.Layout({0, 0, 180, 100});
    dragTree.SetNodeDragEnabled(true);
    std::vector<int> draggedSources;
    int draggedTarget{-1};
    ysDui::controls::list::DuiTreeDropPosition draggedPosition{ysDui::controls::list::DuiTreeDropPosition::Inside};
    dragTree.SetNodeDragHandler([&](const std::vector<int>& sourceIds, int targetId,
                                    ysDui::controls::list::DuiTreeDropPosition position)
    {
        draggedSources = sourceIds;
        draggedTarget = targetId;
        draggedPosition = position;
    });
    assert(dragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {40, 35})));
    assert(!dragTree.Dragging());
    assert(dragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {40, 37})));
    assert(!dragTree.Dragging());
    assert(dragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {40, 80})));
    assert(dragTree.Dragging() && dragTree.Captured()
        && dragTree.DragTargetId() == dragSecond
        && dragTree.DragTargetPosition() == ysDui::controls::list::DuiTreeDropPosition::After);
    assert(dragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {40, 80})));
    const std::vector<int> expectedDragSources{dragFirst};
    assert(!dragTree.Dragging() && !dragTree.Captured() && draggedSources == expectedDragSources
        && draggedTarget == dragSecond
        && draggedPosition == ysDui::controls::list::DuiTreeDropPosition::After
        && dragTree.IdAtVisibleRow(1) == dragSecond && dragTree.IdAtVisibleRow(2) == dragFirst);

    ysDui::controls::list::DuiTreeView insideTree;
    const int insideRoot = insideTree.AddRoot("Root");
    const int insideTarget = insideTree.AddChild(insideRoot, "Target");
    const int insideSource = insideTree.AddChild(insideRoot, "Source");
    insideTree.Layout({0, 0, 180, 100});
    insideTree.SetNodeDragEnabled(true);
    assert(insideTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {40, 65})));
    assert(insideTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {40, 42})));
    assert(insideTree.DragTargetId() == insideTarget
        && insideTree.DragTargetPosition() == ysDui::controls::list::DuiTreeDropPosition::Inside);
    assert(insideTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {40, 42})));
    assert(insideTree.ChildCount(insideRoot) == 1 && insideTree.ChildCount(insideTarget) == 1
        && insideTree.IdAtVisibleRow(2) == insideSource);

    ysDui::controls::list::DuiTreeView invalidDragTree;
    const int invalidRoot = invalidDragTree.AddRoot("Root");
    const int invalidChild = invalidDragTree.AddChild(invalidRoot, "Child");
    invalidDragTree.Layout({0, 0, 180, 100});
    invalidDragTree.SetNodeDragEnabled(true);
    assert(invalidDragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {40, 5})));
    assert(invalidDragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {40, 35})));
    assert(invalidDragTree.Dragging() && invalidDragTree.DragTargetId() == invalidChild
        && invalidDragTree.DragTargetPosition() == ysDui::controls::list::DuiTreeDropPosition::Before);
    assert(invalidDragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {40, 35})));
    assert(!invalidDragTree.Dragging() && !invalidDragTree.Captured() && invalidDragTree.ChildCount(invalidRoot) == 1);

    ysDui::controls::list::DuiTreeView multiDragTree;
    const int multiRoot = multiDragTree.AddRoot("Root");
    const int multiA = multiDragTree.AddChild(multiRoot, "A");
    const int multiB = multiDragTree.AddChild(multiRoot, "B");
    const int multiC = multiDragTree.AddChild(multiRoot, "C");
    multiDragTree.Layout({0, 0, 180, 130});
    multiDragTree.SetMultiSelect(true);
    multiDragTree.SetNodeDragEnabled(true);
    assert(multiDragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {40, 63})));
    assert(!multiDragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {40, 63})));
    assert(multiDragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {40, 35}, {}, modifier::Control)));
    assert(!multiDragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {40, 35}, {}, modifier::Control)));
    std::vector<int> multiSources;
    multiDragTree.SetNodeDragHandler([&](const std::vector<int>& sourceIds, int, ysDui::controls::list::DuiTreeDropPosition)
    {
        multiSources = sourceIds;
    });
    assert(multiDragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {40, 63})));
    assert(multiDragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {40, 108})));
    assert(multiDragTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {40, 108})));
    const std::vector<int> expectedMultiSources{multiB, multiA};
    assert(multiSources == expectedMultiSources
        && multiDragTree.IdAtVisibleRow(1) == multiC
        && multiDragTree.IdAtVisibleRow(2) == multiA
        && multiDragTree.IdAtVisibleRow(3) == multiB);

    ysDui::controls::list::DuiTreeView sortableTree;
    sortableTree.AddColumn({"Name", 120});
    const int sortedRoot = sortableTree.AddRoot("Root");
    const int sortedZ = sortableTree.AddChild(sortedRoot, "Zulu");
    const int sortedA = sortableTree.AddChild(sortedRoot, "Alpha");
    sortableTree.SetCellText(sortedZ, 0, "Zulu");
    sortableTree.SetCellText(sortedA, 0, "Alpha");
    sortableTree.SetSortIndicator(0, 1);
    assert(sortableTree.IdAtVisibleRow(1) == sortedA && sortableTree.IdAtVisibleRow(2) == sortedZ);
    sortableTree.SetSortIndicator(0, -1);
    assert(sortableTree.IdAtVisibleRow(1) == sortedZ && sortableTree.IdAtVisibleRow(2) == sortedA);

    TextInputMock treeInput;
    ysDui::controls::list::DuiTreeView editableTree;
    const int editableRoot = editableTree.AddRoot("Before");
    editableTree.SetEditable(true);
    editableTree.SetTextInput(&treeInput);
    editableTree.Layout({0, 0, 180, 60});
    std::string editedValue;
    editableTree.SetCellEditedHandler([&editedValue](int, int, std::string_view value) { editedValue = value; });
    assert(editableTree.BeginEdit(editableRoot));
    assert(treeInput.visible && !treeInput.borderVisible);
    treeInput.SetText("After");
    editableTree.SetRowHeight(32);
    assert(treeInput.Bounds().top == 1 && treeInput.Bounds().bottom == 31);
    editableTree.CommitEdit();
    assert(editableTree.Label(editableRoot) == "After" && editedValue == "After" && !treeInput.visible);
    assert(!editableTree.BeginEdit(editableRoot, 1));

    int contextId{-1};
    Point contextPoint{};
    editableTree.SetContextMenuHandler([&](int id, Point point) { contextId = id; contextPoint = point; });
    assert(editableTree.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {12, 10}, {}, {}, {}, {}, PointerButton::Secondary)));
    assert((contextId == editableRoot && contextPoint == Point{12, 10}));

    ysDui::controls::list::DuiTreeView lazyTree;
    const int lazyId = lazyTree.AddRoot("Lazy");
    int loadRequested{-1};
    lazyTree.SetChildrenLoadRequestedHandler([&loadRequested](int id) { loadRequested = id; });
    lazyTree.SetExpanded(lazyId, true);
    assert(loadRequested == lazyId && lazyTree.NodeLoadState(lazyId) == ysDui::controls::list::DuiTreeLoadState::Loading);
    lazyTree.SetNodeLoadState(lazyId, ysDui::controls::list::DuiTreeLoadState::Failed);
    lazyTree.Layout({0, 0, 180, 40});
    RecordingCanvas lazyCanvas;
    lazyTree.Paint(lazyCanvas, {0, 0, 180, 40});
    assert(std::find(lazyCanvas.drawnTexts.begin(), lazyCanvas.drawnTexts.end(), "Load failed") != lazyCanvas.drawnTexts.end());

    // 外框由滚动容器绘制：交给 DuiScrollView 时须能关掉自绘外框
    ysDui::controls::list::DuiTreeView borderTree;
    borderTree.Layout({0, 0, 160, 80});
    assert(borderTree.BorderVisible());
    RecordingCanvas framedCanvas;
    borderTree.Paint(framedCanvas, {0, 0, 160, 80});
    assert(framedCanvas.roundedStrokes == 1 && (framedCanvas.strokeBounds == Rect{0, 0, 160, 80}));
    borderTree.SetBorderVisible(false);
    assert(!borderTree.BorderVisible());
    RecordingCanvas unframedCanvas;
    borderTree.Paint(unframedCanvas, {0, 0, 160, 80});
    assert(unframedCanvas.roundedStrokes == 0);

    auto hotKey = std::make_unique<ysDui::controls::input::DuiHotKey>();
    hotKey->Layout({0, 0, 160, 25});
    auto* hotKeyRaw = hotKey.get();
    unsigned int capturedModifiers{};
    hotKeyRaw->SetValueChangedHandler([&capturedModifiers](unsigned int, unsigned int modifiers) { capturedModifiers = modifiers; });
    Host hotKeyHost;
    hotKeyHost.SetRoot(std::move(hotKey));
    assert(hotKeyHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {4, 4})));
    assert(hotKeyRaw->Focused());
    assert(hotKeyHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {}, 'S', modifier::Control | modifier::Shift)));
    assert(hotKeyRaw->DisplayText() == "Ctrl+Shift+S");
    assert(capturedModifiers == (modifier::Control | modifier::Shift));
    assert(hotKeyHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Backspace)));
    assert(hotKeyRaw->Empty());

    ysDui::controls::feedback::DuiBusyIndicator busy;
    busy.SetBounds({0, 0, 24, 24});
    ysDui::core::AnimationClock busyClock;
    busy.SetAnimationClock(&busyClock);
    busy.SetActive(true);
    assert(busyClock.HasScheduledTasks());
    busyClock.Advance(250);
    assert(busy.Active());
    busy.SetPhase(250);
    RecordingCanvas busyCanvas;
    busy.Paint(busyCanvas, {0, 0, 24, 24});
    assert(busyCanvas.arcs == 2);
    DuiTheme busyTheme;
    busyTheme.Set(ThemeSlot::BusyTrack, {19, 29, 39, 255});
    busyTheme.Set(ThemeSlot::BusyIndicator, {20, 30, 40, 255});
    busy.SetTheme(&busyTheme);
    busyCanvas = {};
    busy.Paint(busyCanvas, {0, 0, 24, 24});
    assert((busyCanvas.arcColor == Color{20, 30, 40, 255}));
    busy.SetActive(false);
    assert(!busyClock.HasScheduledTasks());

    const auto editableMenu = ysDui::controls::input::BuildDuiEditContextMenu({false, true, true, true, false});
    assert(editableMenu.size() == 5);
    assert(editableMenu[0].command == ysDui::controls::input::DuiEditContextCommand::Cut && editableMenu[0].enabled);
    assert(editableMenu[2].command == ysDui::controls::input::DuiEditContextCommand::Paste && editableMenu[2].enabled);
    const auto passwordMenu = ysDui::controls::input::BuildDuiEditContextMenu({false, true, true, true, true});
    assert(!passwordMenu[0].enabled && !passwordMenu[1].enabled && passwordMenu[2].enabled);
    const auto readOnlyMenu = ysDui::controls::input::BuildDuiEditContextMenu({true, false, false, false, false});
    assert(readOnlyMenu.size() == 3 && readOnlyMenu[0].command == ysDui::controls::input::DuiEditContextCommand::Copy);
    assert(ysDui::controls::input::DuiEditContextCommandLabel(ysDui::controls::input::DuiEditContextCommand::SelectAll) == "全选(&A)");

    UiHostFactoryMock dialogFactory;
    ysDui::controls::window::DuiDialog dialog;
    int dialogResult = ysDui::controls::window::DuiDialog::IdNone;
    int settingsInvoked{};
    int refreshInvoked{};
    int helpInvoked{};
    dialog.SetTitle("Settings");
    dialog.SetButtons({{"OK (&O)", ysDui::controls::window::DuiDialog::IdOk, true},
                       {"Apply (&A)", ysDui::controls::window::DuiDialog::IdNone, false},
                       {"Cancel (&C)", ysDui::controls::window::DuiDialog::IdCancel, false}});
    dialog.SetCollapsible(true);
    assert(!dialog.FollowOwnerWindow());
    dialog.SetFollowOwnerWindow(true);
    assert(dialog.FollowOwnerWindow());
    dialog.SetSettingsHandler([&settingsInvoked] { ++settingsInvoked; });
    dialog.SetRefreshHandler([&refreshInvoked] { ++refreshInvoked; });
    dialog.SetHelpHandler([&helpInvoked] { ++helpInvoked; });
    dialog.SetClosedHandler([&dialogResult](int result) { dialogResult = result; });
    ysDui::ui::DuiPopupOptions dialogOptions;
    dialogOptions.anchor = {0, 0, 12, 12};
    dialogOptions.size = {320, 180};
    dialogOptions.placement = ysDui::ui::DuiPopupPlacement::CenterOwner;
    dialogOptions.dismissOnFocusLost = false;
    dialogOptions.modality = ysDui::ui::DuiPopupModality::OwnerModal;
    dialogOptions.resizable = true;
    assert(dialog.Show(dialogFactory, {}, dialogOptions, std::make_unique<Control>()));
    assert(dialog.Visible());
    assert(dialogFactory.popup->options.placement == ysDui::ui::DuiPopupPlacement::CenterOwner);
    assert(dialogFactory.popup->options.modality == ysDui::ui::DuiPopupModality::OwnerModal);
    assert(dialogFactory.popup->options.followOwner);
    dialog.SetFollowOwnerWindow(false);
    assert(!dialog.FollowOwnerWindow() && !dialogFactory.popup->options.followOwner);
    dialog.SetFollowOwnerWindow(true);
    assert(dialogFactory.popup->options.chrome.has_value());
    assert(dialogFactory.popup->options.chrome->titleBarHeight == 24);
    assert(dialogFactory.popup->options.chrome->leadingActionWidth == 24);
    assert(dialogFactory.popup->options.chrome->trailingActionWidth == 72);
    assert(dialogFactory.popup->content != nullptr && dialogFactory.popup->content->Children().size() == 4);
    RecordingCanvas dialogCanvas;
    dialogFactory.popup->paint(dialogCanvas, {0, 0, 320, 180});
    assert((dialogCanvas.fills.size() >= 2 && dialogCanvas.fills[1].color == Color{0, 95, 135, 255}));
    assert(dialogCanvas.images == 4 && dialogCanvas.drawnImages.size() == 4 && dialogCanvas.paths == 3);
    assert(std::find(dialogCanvas.drawnTexts.begin(), dialogCanvas.drawnTexts.end(), "Apply (A)")
        != dialogCanvas.drawnTexts.end());
    assert(dialogFactory.popup->content->Children()[2]->Bounds().Width() > 54);
    DuiTheme dialogTheme;
    dialogTheme.Set(ThemeSlot::DialogTitleBackground, {21, 31, 41, 255});
    dialogTheme.Set(ThemeSlot::DialogTitleText, {22, 32, 42, 255});
    dialogFactory.popup->content->SetTheme(&dialogTheme);
    RecordingCanvas themedDialogCanvas;
    dialogFactory.popup->paint(themedDialogCanvas, {0, 0, 320, 180});
    assert(std::any_of(themedDialogCanvas.fills.begin(), themedDialogCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{21, 31, 41, 255};
    }));
    assert(themedDialogCanvas.drawnImages.front() != dialogCanvas.drawnImages.front());
    dialogFactory.popup->content->SetTheme(nullptr);
    dialogCanvas = {};
    dialogFactory.popup->paint(dialogCanvas, {0, 0, 320, 180});
    dialogFactory.popup->layout({0, 0, 480, 320});
    RecordingCanvas enlargedDialogCanvas;
    dialogFactory.popup->paint(enlargedDialogCanvas, {0, 0, 480, 320});
    assert(enlargedDialogCanvas.images == 4);
    assert(std::any_of(enlargedDialogCanvas.fills.begin(), enlargedDialogCanvas.fills.end(), [](const auto& fill)
    {
        return fill.bounds == Rect{0, 0, 480, 24} && fill.color == Color{0, 95, 135, 255};
    }));
    // 面板矩形 = 窗口内缩 panelMargin(12)；上边距同样取自 margin，
    // 即 top = TitleBarHeight(24) + panelMargin(12) = 36（并非 panelPadding）。
    assert(std::find(enlargedDialogCanvas.strokeBoundsList.begin(), enlargedDialogCanvas.strokeBoundsList.end(),
                     Rect{12, 36, 468, 275}) != enlargedDialogCanvas.strokeBoundsList.end());
    assert(std::all_of(dialogFactory.popup->content->Children().begin() + 1,
                       dialogFactory.popup->content->Children().end(), [](const auto& child)
    {
        return child->Bounds().right <= 480 && child->Bounds().bottom <= 320;
    }));
    dialogFactory.popup->layout({0, 0, 320, 180});
    (void)dialogFactory.popup->content->HitTest({12, 12});
    RecordingCanvas hoveredCaptionCanvas;
    dialogFactory.popup->paint(hoveredCaptionCanvas, {0, 0, 320, 180});
    assert(std::any_of(hoveredCaptionCanvas.fills.begin(), hoveredCaptionCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{232, 232, 232, 255};
    }));
    assert(hoveredCaptionCanvas.drawnImages.front() != dialogCanvas.drawnImages.front());
    (void)dialogFactory.popup->content->HitTest({100, 80});
    RecordingCanvas idleCaptionCanvas;
    dialogFactory.popup->paint(idleCaptionCanvas, {0, 0, 320, 180});
    assert(std::none_of(idleCaptionCanvas.fills.begin(), idleCaptionCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{232, 232, 232, 255};
    }));
    (void)dialogFactory.popup->content->HitTest({308, 12});
    RecordingCanvas closeHoverCanvas;
    dialogFactory.popup->paint(closeHoverCanvas, {0, 0, 320, 180});
    assert(std::any_of(closeHoverCanvas.fills.begin(), closeHoverCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{232, 17, 35, 255};
    }));
    assert(closeHoverCanvas.drawnImages.back() == dialogCanvas.drawnImages.back());
    assert(dialogFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerLeave, {308, 12})));
    RecordingCanvas afterLeaveCaptionCanvas;
    dialogFactory.popup->paint(afterLeaveCaptionCanvas, {0, 0, 320, 180});
    assert(std::none_of(afterLeaveCaptionCanvas.fills.begin(), afterLeaveCaptionCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{232, 17, 35, 255};
    }));
    (void)dialogFactory.popup->content->HitTest({100, 80});
    assert(dialogFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {12, 12})));
    assert(dialogFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerUp, {12, 12})));
    assert(settingsInvoked == 1);
    assert(dialogFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {260, 12})));
    assert(dialogFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerUp, {260, 12})));
    assert(refreshInvoked == 1);
    assert(dialogFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {284, 12})));
    assert(dialogFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerUp, {284, 12})));
    assert(helpInvoked == 1);
    (void)dialogFactory.popup->content->HitTest({260, 12});
    RecordingCanvas refreshHoverCanvas;
    dialogFactory.popup->paint(refreshHoverCanvas, {0, 0, 320, 180});
    assert(std::any_of(refreshHoverCanvas.fills.begin(), refreshHoverCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{232, 232, 232, 255};
    }));
    assert(dialogFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerDoubleClick, {120, 12})));
    RecordingCanvas afterCaptionDoubleClickCanvas;
    dialogFactory.popup->paint(afterCaptionDoubleClickCanvas, {0, 0, 320, 24});
    assert(std::none_of(afterCaptionDoubleClickCanvas.fills.begin(), afterCaptionDoubleClickCanvas.fills.end(),
                        [](const auto& fill)
    {
        return fill.color == Color{232, 232, 232, 255};
    }));
    assert(dialog.Collapsed() && dialogFactory.popup->options.size.height == 24
        && !dialogFactory.popup->options.resizable);
    dialog.ToggleCollapsed();
    assert((!dialog.Collapsed() && dialogFactory.popup->options.size == Size{320, 180}));
    dialog.SetMaximizable(true);
    dialog.ToggleMaximized();
    assert(dialog.Maximized() && dialogFactory.popup->options.sizeMode == ysDui::ui::DuiPopupSizeMode::WorkArea);
    assert(dialogFactory.popup->options.chrome.has_value());
    assert(dialogFactory.popup->options.chrome->leadingActionWidth == 0);
    assert(dialogFactory.popup->options.chrome->trailingActionWidth == 72);
    RecordingCanvas maximizedDialogCanvas;
    dialogFactory.popup->paint(maximizedDialogCanvas, {0, 0, 320, 180});
    assert(maximizedDialogCanvas.images == 3);
    assert(dialogFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {260, 12})));
    assert(dialogFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::PointerUp, {260, 12})));
    assert(dialogFactory.popup->requestedMinimize);
    dialog.ToggleMaximized();
    assert(!dialog.Maximized() && dialogFactory.popup->options.sizeMode == ysDui::ui::DuiPopupSizeMode::Specified);
    RecordingCanvas restoredDialogCanvas;
    dialogFactory.popup->paint(restoredDialogCanvas, {0, 0, 320, 180});
    assert(restoredDialogCanvas.images == 3);
    auto* applyButton = dialogFactory.popup->content->Children()[2].get();
    auto* okButton = dialogFactory.popup->content->Children()[1].get();
    const auto clickDialogButton = [](ysDui::core::Control& button)
    {
        const Rect bounds = button.Bounds();
        const Point center{(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
        return button.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, center))
            && button.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, center));
    };
    assert(clickDialogButton(*applyButton) && dialog.Visible());
    assert(clickDialogButton(*okButton));
    assert(!dialog.Visible());
    assert(dialog.Result() == ysDui::controls::window::DuiDialog::IdOk
        && dialogResult == ysDui::controls::window::DuiDialog::IdOk);
    assert(dialogFactory.popup->requestedHide);

    UiHostFactoryMock messageBoxFactory;
    ysDui::controls::window::DuiMessageBox messageBox;
    messageBox.SetTitle("Question");
    messageBox.SetMessage("Continue?");
    messageBox.SetType(ysDui::controls::window::DuiMessageBoxType::Question);
    messageBox.SetButtons({{"Yes", 7}, {"No", 8}});
    messageBox.SetDefaultButton(8);
    assert(messageBox.Show(messageBoxFactory, {}, {0, 0, 10, 10}));
    assert(messageBox.Visible());
    assert(messageBoxFactory.popup->options.placement == ysDui::ui::DuiPopupPlacement::CenterOwner
        && messageBoxFactory.popup->options.modality == ysDui::ui::DuiPopupModality::OwnerModal);
    assert(messageBoxFactory.popup->Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {}, key::Enter)));
    assert(!messageBox.Visible() && messageBox.Result() == 8);

    const ysDui::controls::window::DuiMessageBoxType messageTypes[]{
        ysDui::controls::window::DuiMessageBoxType::Error,
        ysDui::controls::window::DuiMessageBoxType::Warning,
        ysDui::controls::window::DuiMessageBoxType::Information,
        ysDui::controls::window::DuiMessageBoxType::Question,
    };
    for (int index = 0; index < 4; ++index)
    {
        UiHostFactoryMock factory;
        ysDui::controls::window::DuiMessageBox current;
        current.SetTitle("Type");
        current.SetMessage("Message");
        current.SetType(messageTypes[index]);
        assert(current.Show(factory, {}, {0, 0, 10, 10}));
        RecordingCanvas messageCanvas;
        assert((factory.popup->options.placement == ysDui::ui::DuiPopupPlacement::CenterOwner
            && factory.popup->options.modality == ysDui::ui::DuiPopupModality::OwnerModal
            && factory.popup->options.size == Size{244, 115}));
        factory.popup->paint(messageCanvas, {0, 0, 244, 115});
        // MessageBackground / DialogAccent 是消息框的语义底色。这里只断言它们确实被绘制，
        // 不锁定 fills 下标：对话框（含无标题栏分支）会先铺一层 DialogBackground，
        // 下标随绘制顺序变化即失效——原先的下标断言正是因此长期隐藏失败。
        const auto hasFillColor = [&messageCanvas](const Color color)
        {
            return std::any_of(messageCanvas.fills.begin(), messageCanvas.fills.end(),
                               [color](const auto& fill) { return fill.color == color; });
        };
        assert(hasFillColor(Color{240, 240, 240, 255}) && hasFillColor(Color{0, 100, 128, 255}));
        assert(!messageCanvas.ellipses.empty() || !messageCanvas.filledPaths.empty());
        current.Close(ysDui::controls::window::DuiDialog::IdCancel);
    }

    // DuiFlyout：锚定内容气泡
    {
        using ysDui::controls::window::DuiFlyout;
        UiHostFactoryMock flyoutFactory;
        DuiFlyout flyout;
        // 未设置内容工厂时无法显示
        assert(!flyout.Show(flyoutFactory, {}, {0, 0, 10, 10}));
        assert(!flyout.Visible() && flyoutFactory.popup == nullptr);

        flyout.SetContentFactory([] { return std::make_unique<Control>(); });
        // 内容未重写 DesiredSize -> 使用兜底尺寸
        assert(flyout.Show(flyoutFactory, {}, {10, 20, 40, 60}, ysDui::ui::DuiPopupPlacement::Right));
        assert(flyout.Visible() && flyoutFactory.popup != nullptr);
        assert((flyoutFactory.popup->options.anchor == ysDui::core::Rect{10, 20, 40, 60}));
        assert(flyoutFactory.popup->options.placement == ysDui::ui::DuiPopupPlacement::Right);
        assert(flyoutFactory.popup->options.dismissOnFocusLost);
        assert((flyoutFactory.popup->options.size == Size{200, 120}));
        // 默认布局处理器按弹出尺寸摆放内容
        assert(flyoutFactory.popup->content != nullptr);
        assert((flyoutFactory.popup->content->Bounds() == Rect{0, 0, 200, 120}));

        // 固定尺寸优先于内容首选尺寸
        flyout.SetFixedSize({300, 150});
        assert((flyout.FixedSize() == Size{300, 150}));
        assert(flyout.Show(flyoutFactory, {}, {0, 0, 10, 10}));
        assert((flyoutFactory.popup->options.size == Size{300, 150}));
        assert((flyoutFactory.popup->content->Bounds() == Rect{0, 0, 300, 150}));

        // 弹出宿主不画背景：气泡必须自绘底色与 1 像素边框，否则会与宿主窗口背景混为一体
        RecordingCanvas flyoutCanvas;
        flyoutFactory.popup->paint(flyoutCanvas, {0, 0, 300, 150});
        assert(flyoutCanvas.fills.size() == 5);
        assert((flyoutCanvas.fills.front().bounds == Rect{0, 0, 300, 150}));
        const auto flyoutBorder = flyoutCanvas.fills.end() - 4;
        assert((flyoutBorder[0].bounds == Rect{0, 0, 300, 1}
            && flyoutBorder[1].bounds == Rect{0, 149, 300, 150}
            && flyoutBorder[2].bounds == Rect{0, 1, 1, 149}
            && flyoutBorder[3].bounds == Rect{299, 1, 300, 149}));
        const ysDui::core::DuiTheme flyoutTheme;
        assert((flyoutBorder[0].color == flyoutTheme.Get(ysDui::core::ThemeSlot::PopupBorder)));

        // 自定义布局处理器（例如内容自带布局算法时改调其 Layout）
        bool customLayout{};
        flyout.SetLayoutHandler([&customLayout](Control& content, Rect bounds)
        {
            customLayout = true;
            content.SetBounds(bounds);
        });
        assert(flyout.Show(flyoutFactory, {}, {0, 0, 10, 10}));
        assert(customLayout);

        // 关闭通知
        int dismissedCount{};
        flyout.SetDismissedHandler([&dismissedCount] { ++dismissedCount; });
        flyoutFactory.popup->RequestHide();
        assert(dismissedCount == 1 && !flyout.Visible());

        // Hide 释放宿主
        assert(flyout.Show(flyoutFactory, {}, {0, 0, 10, 10}));
        flyout.Hide();
        assert(!flyout.Visible());
    }

    // 图表：折线 / 柱状 / 饼图
    {
        ysDui::controls::chart::DuiLineChart charts;
        charts.SetBounds({0, 0, 320, 180});
        assert(charts.SeriesCount() == 0);
        // 空数据时纵轴退化为 {0, 1}
        const auto emptyRange = charts.ValueRange();
        assert(emptyRange.first == 0.0 && emptyRange.second == 1.0);
        RecordingCanvas emptyCanvas;
        charts.Paint(emptyCanvas, charts.Bounds());
        // 空数据只铺底色，不画折线也不画点
        assert(emptyCanvas.strokedPaths.empty() && emptyCanvas.ellipses.empty());

        charts.AddSeries("Wall", {2.0, 4.0, 3.0, 6.0});
        charts.AddSeries("Core", {1.0, 1.5, 2.0, 2.5}, ysDui::core::Color{200, 60, 60, 255});
        assert(charts.SeriesCount() == 2);
        assert(charts.SeriesAt(0).name == "Wall" && charts.SeriesAt(1).values.size() == 4);
        charts.SetCategoryLabels({"A", "B", "C", "D"});
        // 自动范围必须覆盖全部数据
        const auto autoRange = charts.ValueRange();
        assert(autoRange.first <= 1.0 && autoRange.second >= 6.0);
        // 显式范围覆盖自动结果
        charts.SetValueRange(0, 10);
        const auto fixedRange = charts.ValueRange();
        assert(fixedRange.first == 0.0 && fixedRange.second == 10.0);
        charts.ClearValueRange();
        assert(charts.ValueRange().second >= 6.0);

        RecordingCanvas lineCanvas;
        charts.Paint(lineCanvas, charts.Bounds());
        // 两条系列各一条折线
        assert(lineCanvas.strokedPaths.size() == 2);
        // 数据点 4 + 4
        assert(lineCanvas.ellipses.size() == 8);
        // 图例色块（FillRoundedRect 记录到 rounded 槽，非 filledPaths）
        assert(!lineCanvas.rounded.bounds.Empty());
        assert((charts.DesiredSize() == Size{320, 180}));
        charts.SetShowPoints(false);
        RecordingCanvas noPointCanvas;
        charts.Paint(noPointCanvas, charts.Bounds());
        assert(noPointCanvas.ellipses.empty());
        charts.ClearSeries();
        assert(charts.SeriesCount() == 0);

        ysDui::controls::chart::DuiBarChart bars;
        bars.SetBounds({0, 0, 320, 180});
        bars.AddBar("P1", 12.0);
        bars.AddBar("P2", 30.0);
        bars.AddBar("P3", 6.0);
        assert(bars.BarCount() == 3);
        assert(bars.BarAt(1).label == "P2" && bars.BarAt(1).value == 30.0);
        const auto barRange = bars.ValueRange();
        // 柱状图以 0 为基线
        assert(barRange.first == 0.0 && barRange.second >= 30.0);
        RecordingCanvas barCanvas;
        bars.Paint(barCanvas, bars.Bounds());
        // 柱子走 FillRoundedRect；文本包含数值刻度、柱顶数值与类别标签
        assert(barCanvas.roundedRadius == 2 && !barCanvas.rounded.bounds.Empty());
        assert(std::find(barCanvas.drawnTexts.begin(), barCanvas.drawnTexts.end(), "P2") != barCanvas.drawnTexts.end());
        bars.SetShowValues(false);
        RecordingCanvas noValueCanvas;
        bars.Paint(noValueCanvas, bars.Bounds());
        // 关闭数值标注后恰好少了每根柱子一条文本
        assert(noValueCanvas.drawnTexts.size() + 3 == barCanvas.drawnTexts.size());
        bars.ClearBars();
        assert(bars.BarCount() == 0);

        ysDui::controls::chart::DuiPieChart pie;
        pie.SetBounds({0, 0, 320, 180});
        assert(pie.SliceCount() == 0);
        assert(pie.InnerRadiusRatio() == 0.0);
        // 内圈比例被钳制到 [0, 0.95]
        pie.SetInnerRadiusRatio(1.5);
        assert(pie.InnerRadiusRatio() == 0.95);
        pie.SetInnerRadiusRatio(-1.0);
        assert(pie.InnerRadiusRatio() == 0.0);
        pie.SetInnerRadiusRatio(0.6);
        assert(pie.InnerRadiusRatio() == 0.6);
        assert(pie.Radius() > 0);

        pie.AddSlice("Steel", 40.0);
        pie.AddSlice("Aluminium", 30.0);
        pie.AddSlice("Copper", 30.0);
        // 负值不参与统计
        pie.AddSlice("Invalid", -5.0);
        assert(pie.SliceCount() == 4);
        RecordingCanvas pieCanvas;
        pie.Paint(pieCanvas, pie.Bounds());
        // 3 个有效扇区；环形下每扇区外圈 + 内圈各一次填充
        assert(pieCanvas.filledPaths.size() == 6);
        // 占比与图例文字
        assert(!pieCanvas.drawnTexts.empty());
        // 全零/负值数据不绘制扇区
        pie.ClearSlices();
        pie.AddSlice("None", 0.0);
        RecordingCanvas emptyPieCanvas;
        pie.Paint(emptyPieCanvas, pie.Bounds());
        assert(emptyPieCanvas.filledPaths.empty());
    }

    // DuiDockManager：模型与视图同步、关闭回收、布局往返
    {
        using ysDui::controls::docking::DuiDockManager;
        using ysDui::controls::docking::DuiDockSlot;

        DuiDockManager dock;
        auto editor = std::make_unique<ysDui::controls::basic::DuiLabel>();
        editor->SetText("Editor");
        assert(dock.AddPane("editor", "Editor", std::move(editor)));
        auto outline = std::make_unique<ysDui::controls::basic::DuiLabel>();
        outline->SetText("Outline");
        assert(dock.AddPane("outline", "Outline", std::move(outline), DuiDockSlot::Left));
        assert(dock.PaneCount() == 2 && dock.Contains("editor") && dock.Contains("outline"));
        assert(dock.PaneContent("editor") != nullptr && dock.PaneContent("missing") == nullptr);
        assert(!dock.AddPane("editor", "Duplicate", std::make_unique<ysDui::controls::basic::DuiLabel>()));

        dock.Layout({0, 0, 600, 400});
        // 根是一个可拖拽的分割，比例取自模型（(600 - 4) * 0.25）
        assert(dock.Children().size() == 1);
        auto* splitter = dynamic_cast<ysDui::controls::layout::DuiSplitter*>(dock.Children()[0].get());
        assert(splitter != nullptr);
        assert(splitter->GetOrientation() == ysDui::controls::layout::DuiSplitterOrientation::Vertical);
        assert(splitter->SplitPixels() == 149);
        // 拖动分隔条后比例写回模型
        splitter->SetSplitPixels(200);
        dock.Layout({0, 0, 600, 400});
        assert(splitter->SplitPixels() == 200);
        const Rect bar = splitter->BarRect();
        const int barX = (bar.left + bar.right) / 2;
        const int barY = bar.top + 10;
        assert(splitter->OnEvent({EventType::PointerDown, {barX, barY}}));
        assert(splitter->OnEvent({EventType::PointerMove, {barX + 30, barY}}));
        assert(splitter->OnEvent({EventType::PointerUp, {barX + 30, barY}}));
        assert(splitter->SplitPixels() == 230);
        // 比例（230 / 596）已落到模型
        const double draggedFraction = dock.Tree().Root().fraction;
        assert(draggedFraction > 0.38 && draggedFraction < 0.39);

        // 活动窗格
        int activeChanges{};
        std::string lastActive;
        dock.SetActivePaneChangedHandler([&activeChanges, &lastActive](std::string id)
        {
            ++activeChanges;
            lastActive = std::move(id);
        });
        assert(dock.SetActivePane("outline"));
        assert(dock.ActivePane() == "outline");
        assert(activeChanges == 1 && lastActive == "outline");
        assert(dock.SetActivePane("outline")); // 未变化不通知
        assert(activeChanges == 1);

        // 关闭窗格：空组回收 + 分割折叠
        int closed{};
        std::string lastClosed;
        dock.SetPaneClosedHandler([&closed, &lastClosed](std::string id)
        {
            ++closed;
            lastClosed = std::move(id);
        });
        assert(dock.RemovePane("outline"));
        assert(closed == 1 && lastClosed == "outline");
        assert(dock.PaneCount() == 1);
        assert(dock.Children().size() == 1);
        assert(dynamic_cast<ysDui::controls::list::DuiTabPage*>(dock.Children()[0].get()) != nullptr);

        // 布局往返
        std::string layout;
        assert(dock.SaveLayout(layout));
        assert(!layout.empty());
        assert(dock.LoadLayout(layout));
        assert(dock.PaneCount() == 1 && dock.Contains("editor"));
        assert(!dock.LoadLayout("<dock-layout><split/></dock-layout>"));
        assert(dock.PaneCount() == 1);
    }

    // DuiDockManager：标签关闭按钮只改模型，视图重建推迟到安全点
    {
        using ysDui::controls::docking::DuiDockManager;
        DuiDockManager closing;
        assert(closing.AddPane("a", "A", std::make_unique<ysDui::controls::basic::DuiLabel>()));
        assert(closing.AddPane("b", "B", std::make_unique<ysDui::controls::basic::DuiLabel>()));
        closing.Layout({0, 0, 300, 200});
        auto* page = dynamic_cast<ysDui::controls::list::DuiTabPage*>(closing.Children()[0].get());
        assert(page != nullptr && page->PageCount() == 2);
        const Rect closeRect = page->Header().CloseRect(0);
        assert(!closeRect.Empty());
        const Point centre{(closeRect.left + closeRect.right) / 2,
                           (closeRect.top + closeRect.bottom) / 2};
        assert(page->Header().OnEvent({EventType::PointerDown, centre}));
        assert(page->Header().OnEvent({EventType::PointerUp, centre}));
        // 模型已移除，但视图尚未重建：不能在标签回调栈内销毁正在执行的标签页
        assert(closing.PaneCount() == 1);
        assert(page->PageCount() == 2);
        // 绘制是安全点，重建后视图与模型一致
        RecordingCanvas dockCanvas;
        closing.Paint(dockCanvas, {0, 0, 300, 200});
        auto* rebuilt = dynamic_cast<ysDui::controls::list::DuiTabPage*>(closing.Children()[0].get());
        assert(rebuilt != nullptr && rebuilt->PageCount() == 1);
        assert(!dockCanvas.fills.empty());
    }

    // DuiDockManager：拖动标签跨组停靠、拖出悬浮、自动隐藏滑出
    {
        using ysDui::controls::docking::DuiDockManager;
        using ysDui::controls::docking::DuiDockSlot;

        UiHostFactoryMock factory;
        DuiDockManager dock;
        dock.SetHostFactory(&factory);
        auto labelA = std::make_unique<ysDui::controls::basic::DuiLabel>();
        labelA->SetText("PaneA");
        assert(dock.AddPane("a", "A", std::move(labelA)));
        auto labelB = std::make_unique<ysDui::controls::basic::DuiLabel>();
        labelB->SetText("PaneB");
        assert(dock.AddPane("b", "B", std::move(labelB), DuiDockSlot::Left));
        dock.Layout({0, 0, 600, 400});
        assert(dock.Children().size() == 1);

        // 组视图由窗格内容的父节点反查
        const auto pageOf = [&dock](const char* id) -> ysDui::controls::list::DuiTabPage*
        {
            ysDui::core::Control* content = dock.PaneContent(id);
            return content == nullptr ? nullptr
                : dynamic_cast<ysDui::controls::list::DuiTabPage*>(content->Parent());
        };
        ysDui::controls::list::DuiTabPage* pageB = pageOf("b");
        ysDui::controls::list::DuiTabPage* pageA = pageOf("a");
        assert(pageB != nullptr && pageA != nullptr && pageB != pageA);

        const auto centreOf = [](Rect rect)
        {
            return Point{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
        };

        // 拖动 B 的标签到 A 组中央：并入同一组
        const Point tabB = centreOf(pageB->Header().TabRect(0));
        const Point centreA = centreOf(pageA->Bounds());
        assert(dock.OnEvent({EventType::PointerDown, tabB}));
        assert(!dock.Dragging() && dock.DropHint().empty());
        assert(dock.OnEvent({EventType::PointerMove, centreA}));
        assert(dock.Dragging() && dock.DropHint() == "merge");
        assert(dock.OnEvent({EventType::PointerUp, centreA}));
        assert(!dock.Dragging() && dock.DropHint().empty());
        assert(pageOf("a") == pageOf("b"));
        assert(pageOf("a")->PageCount() == 2);
        assert(dock.PaneCount() == 2);

        // 拖动 B 到 A 组右缘：切出新组
        pageB = pageOf("b");
        pageA = pageOf("a");
        const Point tabB2 = centreOf(pageB->Header().TabRect(1));
        const Rect rectA = pageA->Bounds();
        const Point rightEdge{rectA.right - 4, (rectA.top + rectA.bottom) / 2};
        assert(dock.OnEvent({EventType::PointerDown, tabB2}));
        assert(dock.OnEvent({EventType::PointerMove, rightEdge}));
        assert(dock.DropHint() == "right");
        assert(dock.OnEvent({EventType::PointerUp, rightEdge}));
        assert(pageOf("a") != pageOf("b"));
        assert(dock.Tree().Root().orientation == ysDui::controls::docking::DuiDockOrientation::Vertical);
        assert(dock.PaneCount() == 2);

        // 拖出到空白处：交给宿主工厂创建悬浮窗口
        pageB = pageOf("b");
        const Point tabB3 = centreOf(pageB->Header().TabRect(0));
        assert(dock.OnEvent({EventType::PointerDown, tabB3}));
        assert(dock.OnEvent({EventType::PointerMove, {1500, 900}}));
        assert(dock.DropHint() == "float");
        assert(dock.OnEvent({EventType::PointerUp, {1500, 900}}));
        assert(factory.frame != nullptr && factory.frame->shown);
        assert(factory.frame->options.title == "B");
        assert(factory.frame->content != nullptr);
        assert(dock.PaneCount() == 1 && dock.FloatingPaneIds().size() == 1);
        assert(dock.IsPaneFloating("b"));
        assert(!dock.Contains("b"));
        // 悬浮窗口关闭即窗格关闭：销毁推迟到回调栈退出后的安全点
        int closedFloats{};
        std::string closedId;
        dock.SetPaneClosedHandler([&closedFloats, &closedId](std::string id)
        {
            ++closedFloats;
            closedId = std::move(id);
        });
        factory.frame->closedHandler();
        // 关闭回调只登记，须等下一帧（Layout / Paint）才释放宿主并通知
        dock.Layout({0, 0, 600, 400});
        assert(closedFloats == 1 && closedId == "b" && dock.FloatingPaneIds().empty());

        // 悬浮后的内容可以收回停靠区：先摘出再挂回，且不算关闭
        auto labelD = std::make_unique<ysDui::controls::basic::DuiLabel>();
        labelD->SetText("PaneD");
        assert(dock.AddPane("d", "D", std::move(labelD), DuiDockSlot::Center));
        dock.Layout({0, 0, 600, 400});
        // 窗格图标：标签页与悬浮窗标题栏共用同一张图
        const auto paneIcon = ysDui::render::DuiImage::CreateBgra8Premultiplied(
            {2, 2}, std::vector<unsigned char>(16, 255));
        assert(dock.SetPaneIcon("d", paneIcon));
        auto* pageD = dynamic_cast<ysDui::controls::list::DuiTabPage*>(dock.PaneContent("d")->Parent());
        assert(pageD != nullptr);
        // 该组里 a 在前、d 在后，按标题定位而不是假定下标
        int iconIndex = -1;
        for (int index = 0; index < pageD->PageCount(); ++index)
        {
            if (pageD->PageTitle(index) == "D")
                iconIndex = index;
        }
        assert(iconIndex >= 0 && pageD->PageIcon(iconIndex) == paneIcon);
        assert(dock.FloatPane("d"));
        FrameHostMock* floatD = factory.frame;
        assert(floatD != nullptr && dock.IsPaneFloating("d") && !dock.Contains("d"));
        // 悬浮窗继承窗格图标与主题
        assert(floatD->options.icon == paneIcon);
        assert(floatD->options.theme != nullptr);
        // 收回：内容交还、窗格回到停靠区、不触发关闭通知
        const int closedBefore = closedFloats;
        // 悬浮窗标题栏自带「Dock」按钮，回调即收回（真实平台层会把它推迟到消息栈外执行）
        assert(floatD->options.captionButtons.size() == 1);
        assert(floatD->options.captionButtons.front().glyph == "Dock");
        assert(static_cast<bool>(floatD->captionButtonHandler));
        floatD->captionButtonHandler(floatD->options.captionButtons.front().id);
        // 收回时必须走异步关窗：不能在窗体自身的回调栈内销毁它
        assert(floatD->requestedClose && !floatD->closedByHost);
        assert(!dock.IsPaneFloating("d") && dock.Contains("d"));
        assert(dock.PaneContent("d") != nullptr);
        assert(dock.PaneCount() == 2);
        assert(dock.PaneIds().size() == 2);
        assert(closedFloats == closedBefore);
        // 收回后该窗体对象在安全点被释放（此处由下一次布局触发）
        dock.Layout({0, 0, 600, 400});
        // 收回后仍可继续正常操作
        assert(dock.SetActivePane("d"));
        assert(dock.ActivePane() == "d");
        // 非悬浮窗格无法收回
        assert(!dock.DockFloatingPane("d"));
        assert(!dock.DockFloatingPane("missing"));
        // 再浮出一次后用 API 收回，两条路径都要通
        assert(dock.FloatPane("d"));
        assert(dock.DockFloatingPane("d"));
        assert(dock.Contains("d") && !dock.IsPaneFloating("d"));

        // 自动隐藏：收起为边缘窄条，内容常驻，悬停滑出
        auto labelC = std::make_unique<ysDui::controls::basic::DuiLabel>();
        labelC->SetText("PaneC");
        assert(dock.AddPane("c", "C", std::move(labelC), DuiDockSlot::Center));
        dock.Layout({0, 0, 600, 400});
        assert(dock.SetPaneAutoHide("c", true, DuiDockSlot::Left));
        assert(dock.IsPaneAutoHidden("c"));
        assert(dock.PaneContent("c") != nullptr);
        assert(dock.PaneCount() == 3); // a、d 与 c 都在（c 已收起）
        // A 组左移让出窄条宽度
        dock.Layout({0, 0, 600, 400});
        const Rect rectA2 = pageOf("a")->Bounds();
        assert(rectA2.left == 24);
        // 悬停窄条条目 → 滑出面板绘制出窗格内容
        assert(dock.OnEvent({EventType::PointerMove, {12, 13}}));
        RecordingCanvas peekCanvas;
        dock.Paint(peekCanvas, {0, 0, 600, 400});
        assert(std::find(peekCanvas.drawnTexts.begin(), peekCanvas.drawnTexts.end(), "PaneC")
               != peekCanvas.drawnTexts.end());
        // 指针离开 → 收回
        assert(dock.OnEvent({EventType::PointerMove, {400, 300}}));
        RecordingCanvas hiddenPeekCanvas;
        dock.Paint(hiddenPeekCanvas, {0, 0, 600, 400});
        assert(std::find(hiddenPeekCanvas.drawnTexts.begin(), hiddenPeekCanvas.drawnTexts.end(), "PaneC")
               == hiddenPeekCanvas.drawnTexts.end());
        assert(dock.SetPaneAutoHide("c", false));
        assert(!dock.IsPaneAutoHidden("c"));
        assert(dock.PaneCount() == 3);
    }

    // DuiDockManager：十字停靠指示器的命中与绘制
    {
        using ysDui::controls::docking::DuiDockManager;
        using ysDui::controls::docking::DuiDockSlot;

        DuiDockManager dock;
        auto labelA = std::make_unique<ysDui::controls::basic::DuiLabel>();
        labelA->SetText("Alpha");
        assert(dock.AddPane("alpha", "Alpha", std::move(labelA)));
        auto labelB = std::make_unique<ysDui::controls::basic::DuiLabel>();
        labelB->SetText("Beta");
        assert(dock.AddPane("beta", "Beta", std::move(labelB), DuiDockSlot::Left));
        dock.Layout({0, 0, 600, 400});

        const auto pageOf = [&dock](const char* id) -> ysDui::controls::list::DuiTabPage*
        {
            ysDui::core::Control* content = dock.PaneContent(id);
            return content == nullptr ? nullptr
                : dynamic_cast<ysDui::controls::list::DuiTabPage*>(content->Parent());
        };
        ysDui::controls::list::DuiTabPage* pageBeta = pageOf("beta");
        ysDui::controls::list::DuiTabPage* pageAlpha = pageOf("alpha");
        assert(pageBeta != nullptr && pageAlpha != nullptr && pageBeta != pageAlpha);

        const Rect rectAlpha = pageAlpha->Bounds();
        const Point alphaCentre{(rectAlpha.left + rectAlpha.right) / 2,
                                (rectAlpha.top + rectAlpha.bottom) / 2};
        // 指示器以目标组中心为锚点，整体 3x3 格（每格 30）
        const Rect guide{alphaCentre.x - 45, alphaCentre.y - 45, alphaCentre.x + 45, alphaCentre.y + 45};

        const Point tabBeta{(pageBeta->Header().TabRect(0).left + pageBeta->Header().TabRect(0).right) / 2,
                            (pageBeta->Header().TabRect(0).top + pageBeta->Header().TabRect(0).bottom) / 2};
        assert(dock.OnEvent({EventType::PointerDown, tabBeta}));
        // 中心格 → 并入
        assert(dock.OnEvent({EventType::PointerMove, alphaCentre}));
        assert(dock.DropHint() == "merge");
        // 十字上下左右四格分别对应四向切分
        assert(dock.OnEvent({EventType::PointerMove, {guide.left + 45, guide.top + 15}}));
        assert(dock.DropHint() == "top");
        assert(dock.OnEvent({EventType::PointerMove, {guide.left + 15, guide.top + 45}}));
        assert(dock.DropHint() == "left");
        assert(dock.OnEvent({EventType::PointerMove, {guide.right - 15, guide.top + 45}}));
        assert(dock.DropHint() == "right");
        assert(dock.OnEvent({EventType::PointerMove, {guide.left + 45, guide.bottom - 15}}));
        assert(dock.DropHint() == "bottom");
        // 四个角不属于十字：回退到边缘带宽判定（此处为组内侧 → 融入）
        assert(dock.OnEvent({EventType::PointerMove, {guide.left + 15, guide.top + 15}}));
        assert(dock.DropHint() == "merge");
        // 绘制时指示器五格都出现，当前方向格子高亮
        assert(dock.OnEvent({EventType::PointerMove, {guide.left + 15, guide.top + 45}}));
        RecordingCanvas leftGuideCanvas;
        dock.Paint(leftGuideCanvas, {0, 0, 600, 400});
        assert(leftGuideCanvas.fills.size() >= 5);
        assert(dock.DropHint() == "left");
        // 左格高亮：该格底色为主题主色；位置条贴在该格最外侧（左侧）
        const Color brand = ysDui::core::DuiTheme{}.Get(ThemeSlot::BrandPrimary);
        const auto hitsBrand = [&leftGuideCanvas, brand](const Rect& probe)
        {
            return std::any_of(leftGuideCanvas.fills.begin(), leftGuideCanvas.fills.end(),
                [&probe, brand](const RecordingCanvas::Fill& fill)
                {
                    return fill.color == brand && !Rect::Intersect(fill.bounds, probe).Empty();
                });
        };
        assert(hitsBrand({guide.left, guide.top + 45, guide.left + 30, guide.top + 75}));
        // 位置条与箭头都在左格内部，且落在该格外侧的一半里
        assert(hitsBrand({guide.left, guide.top + 55, guide.left + 15, guide.top + 65}));
        // 抬起后按指示器方向切分
        assert(dock.OnEvent({EventType::PointerUp, {guide.left + 15, guide.top + 45}}));
        assert(pageOf("beta") != pageOf("alpha"));
        const ysDui::controls::docking::DuiDockNode& root = dock.Tree().Root();
        assert(root.kind == ysDui::controls::docking::DuiDockNodeKind::Split);
        // 左格停靠：新组沿竖直方向切分，且位于第一个子节点
        assert(root.orientation == ysDui::controls::docking::DuiDockOrientation::Vertical);
        assert(root.children[0]->panes.size() == 1 && root.children[0]->panes[0].id == "beta");
        assert(root.children[1]->panes.size() == 1 && root.children[1]->panes[0].id == "alpha");
        assert(dock.PaneCount() == 2);
        assert(!dock.Dragging() && dock.DropHint().empty());
    }

    // DuiDockManager：组内标签重排与空布局提示
    {
        using ysDui::controls::docking::DuiDockManager;
        using ysDui::controls::docking::DuiDockSlot;
        DuiDockManager dock;
        RecordingCanvas emptyCanvas;
        dock.Layout({0, 0, 300, 200});
        dock.Paint(emptyCanvas, {0, 0, 300, 200});
        assert(std::find(emptyCanvas.drawnTexts.begin(), emptyCanvas.drawnTexts.end(), "No panes")
               != emptyCanvas.drawnTexts.end());

        auto add = [&dock](const char* id)
        {
            auto label = std::make_unique<ysDui::controls::basic::DuiLabel>();
            label->SetText(id);
            return dock.AddPane(id, id, std::move(label));
        };
        assert(add("first"));
        assert(add("second"));
        dock.Layout({0, 0, 300, 200});
        auto* page = dynamic_cast<ysDui::controls::list::DuiTabPage*>(
            dock.PaneContent("first")->Parent());
        assert(page != nullptr && page->PageCount() == 2);
        assert(page->Header().TextAt(0) == "first" && page->Header().TextAt(1) == "second");
        // 把第二个标签拖到第一个之前
        const Rect firstTab = page->Header().TabRect(0);
        const Point from = {(page->Header().TabRect(1).left + page->Header().TabRect(1).right) / 2,
                            (page->Header().TabRect(1).top + page->Header().TabRect(1).bottom) / 2};
        const Point to{firstTab.left + 2, (firstTab.top + firstTab.bottom) / 2};
        assert(dock.OnEvent({EventType::PointerDown, from}));
        assert(dock.OnEvent({EventType::PointerMove, to}));
        assert(dock.DropHint() == "reorder");
        assert(dock.OnEvent({EventType::PointerUp, to}));
        const std::vector<std::string> order = dock.PaneIds();
        assert(order.size() == 2 && order[0] == "second" && order[1] == "first");

        // 单窗格组拖回自己：切分是空操作，不该出现十字停靠提示
        DuiDockManager pair;
        auto soloBody = std::make_unique<ysDui::controls::basic::DuiLabel>();
        soloBody->SetText("solo");
        assert(pair.AddPane("solo", "solo", std::move(soloBody)));
        auto otherBody = std::make_unique<ysDui::controls::basic::DuiLabel>();
        otherBody->SetText("other");
        assert(pair.AddPane("other", "other", std::move(otherBody), DuiDockSlot::Left));
        pair.Layout({0, 0, 600, 400});
        auto* soloPage = dynamic_cast<ysDui::controls::list::DuiTabPage*>(
            pair.PaneContent("solo")->Parent());
        auto* otherPage = dynamic_cast<ysDui::controls::list::DuiTabPage*>(
            pair.PaneContent("other")->Parent());
        assert(soloPage != nullptr && otherPage != nullptr);
        const Rect soloTab = soloPage->Header().TabRect(0);
        const Rect soloBounds = soloPage->Bounds();
        const Rect otherBounds = otherPage->Bounds();
        const Point dragFrom{(soloTab.left + soloTab.right) / 2, (soloTab.top + soloTab.bottom) / 2};
        // 指向自己那组的中心：对单窗格组而言等价于原地
        const Point overSelf{(soloBounds.left + soloBounds.right) / 2,
                             (soloBounds.top + soloBounds.bottom) / 2};
        assert(pair.OnEvent({EventType::PointerDown, dragFrom}));
        assert(pair.OnEvent({EventType::PointerMove, overSelf}));
        assert(pair.DropHint() == "reorder");
        RecordingCanvas overSelfCanvas;
        pair.Paint(overSelfCanvas, {0, 0, 600, 400});
        assert(pair.OnEvent({EventType::PointerUp, overSelf}));
        // 没有产生多余分割：仍是两个组、两个窗格
        assert(pair.PaneCount() == 2 && pair.PaneIds().size() == 2);

        // 对照：拖到另一个组时指示器 5 格都画出来（描边数恰好多 5）
        const Point overOther{(otherBounds.left + otherBounds.right) / 2,
                              (otherBounds.top + otherBounds.bottom) / 2};
        assert(pair.OnEvent({EventType::PointerDown, dragFrom}));
        assert(pair.OnEvent({EventType::PointerMove, overOther}));
        assert(pair.DropHint() == "merge");
        RecordingCanvas overOtherCanvas;
        pair.Paint(overOtherCanvas, {0, 0, 600, 400});
        assert(overOtherCanvas.roundedStrokes - overSelfCanvas.roundedStrokes == 5);
        assert(pair.OnEvent({EventType::PointerUp, overOther}));
        assert(pair.Tree().FindGroup("solo") == pair.Tree().FindGroup("other"));
    }
}
