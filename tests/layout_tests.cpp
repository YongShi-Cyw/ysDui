/**
 * 文件名：layout_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：验证平台无关布局、滚动、分割和尺寸分配逻辑。
 */
#include "test_support.hpp"

int main()
{
    using namespace ysDui::core;
    ysDui::controls::layout::DuiStack stack;
    stack.SetPadding({2, 1, 2, 1});
    stack.SetGap(2);
    auto fixed = std::make_unique<Control>();
    Control* fixedRaw = fixed.get();
    auto flexible = std::make_unique<Control>();
    Control* flexibleRaw = flexible.get();
    stack.AddChild(std::move(fixed), {20, 1, {1, 0, 1, 0}});
    stack.AddChild(std::move(flexible), {-1, 1, {0, 0, 0, 0}});
    stack.Layout({0, 0, 100, 20});
    assert((fixedRaw->Bounds() == Rect{3, 1, 23, 19}));
    assert((flexibleRaw->Bounds() == Rect{26, 1, 98, 19}));
    ysDui::controls::layout::DuiStack pageStack;
    pageStack.SetMode(ysDui::controls::layout::DuiStackMode::Pages);
    auto stackFirstPage = std::make_unique<Control>();
    Control* stackFirstPageRaw = stackFirstPage.get();
    auto stackSecondPage = std::make_unique<Control>();
    Control* stackSecondPageRaw = stackSecondPage.get();
    pageStack.AddPage(std::move(stackFirstPage));
    pageStack.AddPage(std::move(stackSecondPage));
    pageStack.Layout({4, 5, 84, 45});
    assert(pageStack.PageCount() == 2 && pageStack.CurrentIndex() == 0);
    assert(stackFirstPageRaw->Visible() && !stackSecondPageRaw->Visible());
    assert((stackFirstPageRaw->Bounds() == Rect{4, 5, 84, 45}));
    pageStack.SetCurrentIndex(1);
    pageStack.Layout({4, 5, 84, 45});
    assert(!stackFirstPageRaw->Visible() && stackSecondPageRaw->Visible());
    assert((stackSecondPageRaw->Bounds() == Rect{4, 5, 84, 45}));
    pageStack.SetMode(ysDui::controls::layout::DuiStackMode::WeightedLayout);
    assert(stackFirstPageRaw->Visible() && stackSecondPageRaw->Visible());
    assert(pageStack.RemovePage(1) != nullptr && pageStack.CurrentIndex() == 0);
    ysDui::controls::layout::DuiStack paintStack;
    auto paintLabel = std::make_unique<ysDui::controls::basic::DuiLabel>();
    paintLabel->SetText("Nested");
    paintStack.AddChild(std::move(paintLabel), {20, 1, {}});
    paintStack.Layout({0, 0, 40, 20});
    RecordingCanvas nestedCanvas;
    paintStack.Paint(nestedCanvas, {0, 0, 40, 20});
    assert(nestedCanvas.drawnText == "Nested");

    ysDui::controls::layout::DuiFlow flow;
    flow.SetPadding({1, 2, 1, 2});
    flow.SetGap(3);
    auto flowFirst = std::make_unique<Control>();
    Control* flowFirstRaw = flowFirst.get();
    auto flowSecond = std::make_unique<Control>();
    Control* flowSecondRaw = flowSecond.get();
    flow.AddChild(std::move(flowFirst), {{30, 10}, {1, 1, 1, 1}});
    flow.AddChild(std::move(flowSecond), {{30, 8}, {0, 0, 0, 0}});
    flow.Layout({0, 0, 50, 100});
    assert((flowFirstRaw->Bounds() == Rect{2, 3, 32, 13}));
    assert((flowSecondRaw->Bounds() == Rect{1, 17, 31, 25}));
    assert((flow.DesiredSize() == Size{50, 27}));

    ysDui::controls::layout::DuiDock dock;
    dock.SetPadding({1, 2, 3, 4});
    dock.SetGap(2);
    auto top = std::make_unique<Control>();
    Control* topRaw = top.get();
    auto left = std::make_unique<Control>();
    Control* leftRaw = left.get();
    auto fill = std::make_unique<Control>();
    Control* fillRaw = fill.get();
    dock.AddDocked(std::move(top), ysDui::controls::layout::DuiDockSide::Top, 10);
    dock.AddDocked(std::move(left), ysDui::controls::layout::DuiDockSide::Left, 20);
    dock.AddDocked(std::move(fill), ysDui::controls::layout::DuiDockSide::Fill);
    dock.Layout({0, 0, 100, 80});
    assert((topRaw->Bounds() == Rect{1, 2, 97, 12}));
    assert((leftRaw->Bounds() == Rect{1, 14, 21, 76}));
    assert((fillRaw->Bounds() == Rect{23, 14, 97, 76}));

    bool splitChanged{};
    auto splitter = std::make_unique<ysDui::controls::layout::DuiSplitter>();
    splitter->SetMinSizes(20, 20);
    splitter->SetSplitPixels(40);
    splitter->SetValueChangedHandler([&splitChanged](int pixels) { splitChanged = pixels == 60; });
    auto firstPane = std::make_unique<ysDui::controls::basic::DuiSeparator>();
    Control* firstPaneRaw = firstPane.get();
    auto secondPane = std::make_unique<Control>();
    Control* secondPaneRaw = secondPane.get();
    splitter->SetPane(0, std::move(firstPane));
    splitter->SetPane(1, std::move(secondPane));
    splitter->Layout({0, 0, 100, 40});
    assert((splitter->BarRect() == Rect{40, 0, 44, 40}));
    assert(splitter->HitTest({39, 10}) == splitter.get());
    assert(splitter->PointerCursor() == DuiPointerCursor::ResizeHorizontal);
    assert(splitter->HitTest({20, 10}) == firstPaneRaw);
    assert(firstPaneRaw->PointerCursor() == DuiPointerCursor::Arrow);
    assert((firstPaneRaw->Bounds() == Rect{0, 0, 40, 40}));
    assert((secondPaneRaw->Bounds() == Rect{44, 0, 100, 40}));
    RecordingCanvas splitterCanvas;
    splitter->Paint(splitterCanvas, {0, 0, 100, 40});
    assert(splitterCanvas.fills.size() == 2);
    assert((splitterCanvas.fills.back().bounds == Rect{40, 0, 44, 40}));
    assert((splitterCanvas.fills.back().color == Color{238, 238, 240, 255}));
    assert(splitter->HitTest({41, 10}) == splitter.get());
    RecordingCanvas hoveredSplitterCanvas;
    splitter->Paint(hoveredSplitterCanvas, {0, 0, 100, 40});
    assert((hoveredSplitterCanvas.fills.back().color == Color{160, 207, 255, 255}));
    DuiTheme splitterTheme;
    splitterTheme.Set(ThemeSlot::SplitterBar, {17, 27, 37, 255});
    ysDui::controls::layout::DuiSplitter themedSplitter;
    themedSplitter.SetTheme(&splitterTheme);
    themedSplitter.SetSplitPixels(40);
    themedSplitter.Layout({0, 0, 100, 40});
    RecordingCanvas themedSplitterCanvas;
    themedSplitter.Paint(themedSplitterCanvas, themedSplitter.Bounds());
    assert((themedSplitterCanvas.fills.back().color == Color{17, 27, 37, 255}));
    themedSplitter.SetBarColor({18, 28, 38, 255});
    themedSplitterCanvas = {};
    themedSplitter.Paint(themedSplitterCanvas, themedSplitter.Bounds());
    assert((themedSplitterCanvas.fills.back().color == Color{18, 28, 38, 255}));
    Host splitterHost;
    splitterHost.SetRoot(std::move(splitter));
    assert(splitterHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {41, 10})));
    assert(splitterHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {61, 10})));
    assert(splitChanged);
    assert(splitterHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerUp, {61, 10})));

    auto innerSplitter = std::make_unique<ysDui::controls::layout::DuiSplitter>();
    auto* innerSplitterRaw = innerSplitter.get();
    innerSplitter->SetOrientation(ysDui::controls::layout::DuiSplitterOrientation::Horizontal);
    innerSplitter->SetMinSizes(20, 20);
    innerSplitter->SetSplitFraction(0.5);
    auto innerFirstPane = std::make_unique<Control>();
    auto* innerFirstPaneRaw = innerFirstPane.get();
    auto innerSecondPane = std::make_unique<Control>();
    auto* innerSecondPaneRaw = innerSecondPane.get();
    innerSplitter->SetPane(0, std::move(innerFirstPane));
    innerSplitter->SetPane(1, std::move(innerSecondPane));
    auto nestedSplitter = std::make_unique<ysDui::controls::layout::DuiSplitter>();
    nestedSplitter->SetMinSizes(40, 40);
    nestedSplitter->SetSplitPixels(80);
    nestedSplitter->SetPane(0, std::make_unique<Control>());
    nestedSplitter->SetPane(1, std::move(innerSplitter));
    nestedSplitter->Layout({0, 0, 300, 200});
    innerSplitterRaw->Layout(innerSplitterRaw->Bounds());
    Host nestedSplitterHost;
    nestedSplitterHost.SetRoot(std::move(nestedSplitter));
    assert(nestedSplitterHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {81, 100})));
    assert(nestedSplitterHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {121, 100})));
    assert((innerSplitterRaw->Bounds() == Rect{124, 0, 300, 200}));
    assert((innerFirstPaneRaw->Bounds() == Rect{124, 0, 300, 98}));
    assert((innerSecondPaneRaw->Bounds() == Rect{124, 102, 300, 200}));
    assert(nestedSplitterHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerUp, {121, 100})));

    ysDui::controls::layout::DuiSplitter horizontalSplitter;
    horizontalSplitter.SetOrientation(ysDui::controls::layout::DuiSplitterOrientation::Horizontal);
    horizontalSplitter.SetBarThickness(1);
    horizontalSplitter.SetMinSizes(20, 20);
    horizontalSplitter.SetSplitPixels(40);
    horizontalSplitter.Layout({0, 0, 100, 100});
    assert(horizontalSplitter.HitTest({10, 40}) == &horizontalSplitter);
    assert(horizontalSplitter.PointerCursor() == DuiPointerCursor::ResizeVertical);

    ysDui::controls::basic::DuiStatusBar statusBar;
    const int fixedPane = statusBar.AddPane(30);
    const int springPane = statusBar.AddPane(20, true);
    statusBar.SetPaneText(fixedPane, "Ready");
    statusBar.SetPaneText(springPane, "Details");
    statusBar.Layout({0, 0, 100, 22});
    assert((statusBar.ComputePaneRect(fixedPane, statusBar.Bounds()) == Rect{0, 0, 30, 22}));
    assert((statusBar.ComputePaneRect(springPane, statusBar.Bounds()) == Rect{30, 0, 100, 22}));
    RecordingCanvas statusCanvas;
    statusBar.Paint(statusCanvas, {0, 0, 100, 22});
    assert(statusCanvas.fills.size() == 3);
    assert(statusCanvas.drawnText == "Details");

    bool scrolled{};
    auto scrollBar = std::make_unique<ysDui::controls::input::DuiScrollBar>();
    scrollBar->SetBounds({0, 0, 17, 100});
    scrollBar->SetRange(0, 80);
    scrollBar->SetPageSize(20);
    scrollBar->SetValueChangedHandler([&scrolled](int value) { scrolled = value == 80; });
    assert((scrollBar->ComputeThumbRect() == Rect{1, 18, 16, 36}));
    RecordingCanvas idleScrollCanvas;
    scrollBar->Paint(idleScrollCanvas, scrollBar->Bounds());
    assert(idleScrollCanvas.fills.size() == 4 && idleScrollCanvas.pathFillColors.size() == 2);
    assert(idleScrollCanvas.filledPaths[0].Commands().size() == 15);
    assert((idleScrollCanvas.filledPaths[0].Commands().front().point == Point{8, 7}));
    assert((idleScrollCanvas.filledPaths[1].Commands().front().point == Point{8, 93}));
    assert((idleScrollCanvas.fills[0].color == Color{240, 240, 240, 255}));
    assert((idleScrollCanvas.fills[1].color == Color{240, 240, 240, 255}));
    assert((idleScrollCanvas.fills[2].color == Color{240, 240, 240, 255}));
    assert((idleScrollCanvas.fills[3].color == Color{205, 205, 205, 255}));
    assert(!scrollBar->OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {10, 29})));
    RecordingCanvas hoveredThumbCanvas;
    scrollBar->Paint(hoveredThumbCanvas, scrollBar->Bounds());
    assert((hoveredThumbCanvas.fills[3].color == Color{166, 166, 166, 255}));
    assert(!scrollBar->OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {10, 10})));
    RecordingCanvas hoveredButtonCanvas;
    scrollBar->Paint(hoveredButtonCanvas, scrollBar->Bounds());
    assert((hoveredButtonCanvas.fills[1].color == Color{218, 218, 218, 255}));
    assert(!scrollBar->OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {10, 50})));
    RecordingCanvas restoredButtonCanvas;
    scrollBar->Paint(restoredButtonCanvas, scrollBar->Bounds());
    assert((restoredButtonCanvas.fills[1].color == Color{240, 240, 240, 255}));
    scrollBar->SetPosition(50, false);
    assert(scrollBar->OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(scrollBar->Position() == 49);
    assert(scrollBar->OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 90})));
    assert(scrollBar->Position() == 50);
    scrollBar->SetPosition(0, false);
    Host scrollHost;
    scrollHost.SetRoot(std::move(scrollBar));
    assert(scrollHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {10, 29})));
    assert(scrollHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerMove, {10, 90})));
    assert(scrollHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerUp, {10, 90})));
    assert(scrolled);
    scrollHost.Root()->SetBounds({0, 0, 17, 100});
    auto* scrollBarRaw = static_cast<ysDui::controls::input::DuiScrollBar*>(scrollHost.Root());
    scrollBarRaw->SetPosition(50, false);
    assert(scrollHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerWheel, {10, 10}, 0, 0, 0, 120)));
    assert(scrollBarRaw->Position() == 47);

    ysDui::controls::input::DuiScrollBar horizontalScrollBar(true);
    horizontalScrollBar.SetBounds({0, 0, 100, 17});
    horizontalScrollBar.SetRange(0, 80);
    horizontalScrollBar.SetPageSize(20);
    assert((horizontalScrollBar.ComputeThumbRect() == Rect{18, 1, 36, 16}));
    RecordingCanvas horizontalScrollCanvas;
    horizontalScrollBar.Paint(horizontalScrollCanvas, horizontalScrollBar.Bounds());
    assert((horizontalScrollCanvas.filledPaths[0].Commands().front().point == Point{7, 8}));
    assert((horizontalScrollCanvas.filledPaths[1].Commands().front().point == Point{93, 8}));

    auto slider = std::make_unique<ysDui::controls::input::DuiSlider>();
    slider->SetBounds({0, 0, 100, 24});
    int sliderValue = -1;
    slider->SetValueChangedHandler([&sliderValue](int value) { sliderValue = value; });
    auto* sliderRaw = slider.get();
    Host sliderHost;
    sliderHost.SetRoot(std::move(slider));
    assert(sliderHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {50, 12})));
    assert(sliderRaw->Value() == 50 && sliderValue == 50);
    assert(sliderHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerWheel, {50, 12}, 0, 0, 0, 120)));
    assert(sliderRaw->Value() == 51);
    RecordingCanvas sliderCanvas;
    sliderRaw->Paint(sliderCanvas, {0, 0, 100, 24});
    assert(sliderCanvas.roundedRadius == 2 && sliderCanvas.ellipses.size() == 1
        && sliderCanvas.roundedStrokes == 1);
    assert(sliderHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {50, 12}, ysDui::core::key::Right)));
    assert(sliderRaw->Value() == 52);
    assert(sliderHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {50, 12}, ysDui::core::key::Left)));
    assert(sliderRaw->Value() == 51);
    sliderRaw->SetRange(0, 1000);
    sliderRaw->SetLineSize(50);
    assert(sliderHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {37, 12})));
    assert(sliderRaw->Value() % 50 == 0);
    assert(sliderHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {37, 12}, ysDui::core::key::Right)));
    assert(sliderRaw->Value() % 50 == 0);
    sliderRaw->SetEnabled(false);
    const int disabledValue = sliderRaw->Value();
    assert(!sliderHost.Dispatch(ysDui::test::MakeEvent(EventType::KeyDown, {37, 12}, ysDui::core::key::Right)));
    assert(sliderRaw->Value() == disabledValue);

    auto scrollView = std::make_unique<ysDui::controls::layout::DuiScrollView>();
    auto scrollContent = std::make_unique<ysDui::controls::basic::DuiLabel>();
    scrollContent->SetText("Scrollable");
    scrollView->SetContent(std::move(scrollContent));
    scrollView->SetContentSize({100, 200});
    scrollView->Layout({0, 0, 100, 100});
    assert((scrollView->Children()[0]->Bounds() == Rect{83, 0, 100, 100}));
    Host scrollViewHost;
    auto* scrollViewRaw = scrollView.get();
    scrollViewHost.SetRoot(std::move(scrollView));
    assert(scrollViewHost.Dispatch(ysDui::test::MakeEvent(EventType::PointerWheel, {10, 10}, 0, 0, 0, -120)));
    assert(scrollViewRaw->ScrollPosition() == 48);
    RecordingCanvas scrollViewCanvas;
    scrollViewRaw->Paint(scrollViewCanvas, {0, 0, 100, 100});
    assert(scrollViewCanvas.clips.size() == 1 && scrollViewCanvas.popCount == 1);
    assert(!scrollViewCanvas.fills.empty());
    assert((scrollViewCanvas.fills.front().bounds == Rect{0, 0, 100, 100}));
    assert((scrollViewCanvas.fills.front().color == Color{252, 252, 252, 255}));
    // 外框由本控件绘制，从而把滚动条一并包住（内容的底边会被视口裁掉）
    assert((scrollViewCanvas.strokeBounds == Rect{0, 0, 100, 100}) && scrollViewCanvas.roundedStrokes == 1);
    scrollViewRaw->SetVisible(false);
    RecordingCanvas hiddenScrollViewCanvas;
    scrollViewRaw->Paint(hiddenScrollViewCanvas, {0, 0, 100, 100});
    assert(hiddenScrollViewCanvas.fills.empty() && hiddenScrollViewCanvas.clips.empty());

    ysDui::controls::layout::DuiScrollView movingScrollView;
    auto movingScrollContent = std::make_unique<ysDui::controls::basic::DuiLabel>();
    auto* movingScrollContentRaw = movingScrollContent.get();
    movingScrollView.SetContent(std::move(movingScrollContent));
    movingScrollView.SetContentSize({100, 200});
    movingScrollView.Layout({0, 0, 100, 100});
    ysDui::controls::layout::DuiScrollView movedScrollView(std::move(movingScrollView));
    assert(movingScrollContentRaw->Parent() == &movedScrollView);
    assert(movedScrollView.OnEvent(ysDui::test::MakeEvent(EventType::PointerWheel, {10, 10}, 0, 0, 0, -120)));
    assert(movedScrollView.ScrollPosition() == 48);
    // 内容宽度限制在视口内（viewport.right = 100 - 滚动条宽 17 = 83），
    // 避免内容盖住滚动条导致无法拖拽；高度按滚动位置偏移。
    assert((movingScrollContentRaw->Bounds() == Rect{0, -48, 83, 152}));

    // 贴底跟随：开启即滚到末尾，内容增长时保持在末尾
    ysDui::controls::layout::DuiScrollView followView;
    followView.SetContent(std::make_unique<ysDui::controls::basic::DuiLabel>());
    followView.SetContentSize({100, 200});
    followView.Layout({0, 0, 100, 100});
    assert(!followView.FollowBottom() && followView.ScrollPosition() == 0 && !followView.AtBottom());
    followView.SetFollowBottom(true);
    assert(followView.FollowBottom() && followView.AtBottom());
    assert(followView.ScrollPosition() == 100);
    // 内容增长（流式追加）：仍钉在末尾
    followView.SetContentSize({100, 300});
    assert(followView.ScrollPosition() == 200);
    // 滚轮上滚离开末尾：自动关闭跟随，不再抢用户的位置
    assert(followView.OnEvent(ysDui::test::MakeEvent(EventType::PointerWheel, {10, 10}, 0, 0, 0, 120)));
    assert(followView.ScrollPosition() == 152 && !followView.FollowBottom() && !followView.AtBottom());
    followView.SetContentSize({100, 400});
    assert(followView.ScrollPosition() == 152);
    // 显式定位同样遵循"是否落在末尾"的规则
    followView.SetScrollPosition(followView.ContentSize().height);
    assert(followView.FollowBottom() && followView.AtBottom());
    followView.SetScrollPosition(0);
    assert(!followView.FollowBottom() && followView.ScrollPosition() == 0);
    // 回到末尾重新开启跟随，随后的内容增长继续贴底
    followView.ScrollToBottom();
    assert(followView.FollowBottom() && followView.AtBottom());
    followView.SetContentSize({100, 500});
    assert(followView.ScrollPosition() == 400);
    // 无溢出时始终视为在末尾
    followView.SetContentSize({100, 50});
    assert(followView.AtBottom() && followView.ScrollPosition() == 0);
    // 关掉跟随：内容增长不再移动位置
    followView.SetFollowBottom(false);
    followView.SetContentSize({100, 200});
    assert(followView.ScrollPosition() == 0);

    Control movingParent;
    auto movingChild = std::make_unique<Control>();
    auto* movingChildRaw = movingChild.get();
    movingParent.AddChild(std::move(movingChild));
    Control movedParent(std::move(movingParent));
    assert(movedParent.Parent() == nullptr && movingChildRaw->Parent() == &movedParent);

    Control assignmentParent;
    auto assignmentTarget = std::make_unique<Control>();
    auto* assignmentTargetRaw = assignmentTarget.get();
    assignmentParent.AddChild(std::move(assignmentTarget));
    Control assignmentSource;
    auto assignmentDescendant = std::make_unique<Control>();
    auto* assignmentDescendantRaw = assignmentDescendant.get();
    assignmentSource.AddChild(std::move(assignmentDescendant));
    *assignmentTargetRaw = std::move(assignmentSource);
    assert(assignmentTargetRaw->Parent() == &assignmentParent);
    assert(assignmentDescendantRaw->Parent() == assignmentTargetRaw);

    ysDui::controls::layout::DuiHBox horizontalLayout;
    auto horizontalFirst = std::make_unique<ysDui::core::Control>();
    auto horizontalSecond = std::make_unique<ysDui::core::Control>();
    ysDui::core::Control* horizontalFirstRaw = horizontalFirst.get();
    ysDui::core::Control* horizontalSecondRaw = horizontalSecond.get();
    horizontalLayout.SetGap(4);
    horizontalLayout.AddChild(std::move(horizontalFirst), ysDui::controls::layout::DuiLayoutHint{}.Fixed(20));
    horizontalLayout.AddChild(std::move(horizontalSecond), ysDui::controls::layout::DuiLayoutHint{}.Flexible());
    horizontalLayout.Layout({0, 0, 100, 20});
    assert((horizontalFirstRaw->Bounds() == Rect{0, 0, 20, 20}));
    assert((horizontalSecondRaw->Bounds() == Rect{24, 0, 100, 20}));

    ysDui::controls::layout::DuiHBox remainderLayout;
    auto flexibleFirst = std::make_unique<ysDui::core::Control>();
    auto fixedLast = std::make_unique<ysDui::core::Control>();
    ysDui::core::Control* flexibleFirstRaw = flexibleFirst.get();
    remainderLayout.AddChild(std::move(flexibleFirst), ysDui::controls::layout::DuiLayoutHint{}.Flexible());
    remainderLayout.AddChild(std::move(fixedLast), ysDui::controls::layout::DuiLayoutHint{}.Fixed(20));
    remainderLayout.Layout({0, 0, 101, 20});
    assert((flexibleFirstRaw->Bounds() == Rect{0, 0, 81, 20}));

    ysDui::controls::layout::DuiGrid gridLayout;
    auto gridChild = std::make_unique<ysDui::core::Control>();
    ysDui::core::Control* gridChildRaw = gridChild.get();
    gridLayout.SetGrid(2, 2);
    gridLayout.SetGap(2);
    gridLayout.SetColumnWidth(0, 30);
    gridLayout.AddChild(std::move(gridChild), {});
    gridLayout.SetCell(gridChildRaw, 0, 0, 2, 1);
    gridLayout.Layout({0, 0, 100, 50});
    assert((gridChildRaw->Bounds() == Rect{0, 0, 30, 50}));

    // DesiredSize 现在是 Control 的虚协议：可经基类引用多态查询
    ysDui::controls::input::DuiSpinBox protocolSpinBox;
    ysDui::core::Control& protocolAsControl = protocolSpinBox;
    assert((protocolAsControl.DesiredSize() == Size{120, 23}));
    ysDui::core::Control bareControl; // 未重写 DesiredSize：默认无偏好
    assert((bareControl.DesiredSize() == Size{0, 0}));

    // 内容驱动尺寸：DuiLayoutHint::Auto 让主轴取子控件自报的首选尺寸
    ysDui::controls::layout::DuiHBox autoLayout;
    auto autoSized = std::make_unique<ysDui::controls::input::DuiSpinBox>();
    ysDui::core::Control* autoSizedRaw = autoSized.get();
    const Size autoSizedDesired = autoSized->DesiredSize();
    autoLayout.AddChild(std::move(autoSized), ysDui::controls::layout::DuiLayoutHint{}.Auto());
    autoLayout.Layout({0, 0, 300, 40});
    // 主轴 = 首选宽度；交叉轴未指定仍为 Fill（撑满 40）
    assert((autoSizedRaw->Bounds() == Rect{0, 0, autoSizedDesired.width, 40}));

    // Auto 与 Flexible 混排：Auto 计入固定部分，Flexible 吃掉余量
    ysDui::controls::layout::DuiHBox mixedLayout;
    auto mixedAuto = std::make_unique<ysDui::controls::input::DuiSpinBox>();
    auto mixedFlexible = std::make_unique<Control>();
    ysDui::core::Control* mixedAutoRaw = mixedAuto.get();
    ysDui::core::Control* mixedFlexibleRaw = mixedFlexible.get();
    mixedLayout.SetGap(5);
    mixedLayout.AddChild(std::move(mixedAuto), ysDui::controls::layout::DuiLayoutHint{}.Auto());
    mixedLayout.AddChild(std::move(mixedFlexible), ysDui::controls::layout::DuiLayoutHint{}.Flexible());
    mixedLayout.Layout({0, 0, 300, 40});
    assert((mixedAutoRaw->Bounds() == Rect{0, 0, autoSizedDesired.width, 40}));
    assert((mixedFlexibleRaw->Bounds() == Rect{autoSizedDesired.width + 5, 0, 300, 40}));

    // DuiUniformGrid：所有单元格等宽等高
    ysDui::controls::layout::DuiUniformGrid uniform;
    uniform.SetGrid(3);
    uniform.SetGap(4);
    auto uniformFirst = std::make_unique<Control>();
    auto uniformSecond = std::make_unique<Control>();
    auto uniformThird = std::make_unique<Control>();
    auto uniformFourth = std::make_unique<Control>();
    Control* uniformFirstRaw = uniformFirst.get();
    Control* uniformSecondRaw = uniformSecond.get();
    Control* uniformThirdRaw = uniformThird.get();
    Control* uniformFourthRaw = uniformFourth.get();
    uniform.AddChild(std::move(uniformFirst));
    uniform.AddChild(std::move(uniformSecond));
    uniform.AddChild(std::move(uniformThird));
    uniform.AddChild(std::move(uniformFourth));
    assert(uniform.Columns() == 3 && uniform.Rows() == 0 && uniform.Gap() == 4);
    // 100 宽 3 列 2 gap -> 单元格宽 (100 - 8) / 3 = 30；2 行自动换行 -> 高 (40 - 4) / 2 = 18
    uniform.Layout({0, 0, 100, 40});
    assert((uniformFirstRaw->Bounds() == Rect{0, 0, 30, 18}));
    assert((uniformSecondRaw->Bounds() == Rect{34, 0, 64, 18}));
    assert((uniformThirdRaw->Bounds() == Rect{68, 0, 98, 18}));
    // 第 4 项换到第二行（行高相等）
    assert((uniformFourthRaw->Bounds() == Rect{0, 22, 30, 40}));

    // 隐藏项不占位：第 2 项隐藏后余 3 项，恰好排满 1 行，行高变为整个可用高度
    uniformSecondRaw->SetVisible(false);
    uniform.Layout({0, 0, 100, 40});
    assert((uniformThirdRaw->Bounds() == Rect{34, 0, 64, 40}));
    assert((uniformFourthRaw->Bounds() == Rect{68, 0, 98, 40}));

    // DuiCanvas：子项按给定坐标与尺寸摆放，未给尺寸时取 DesiredSize
    ysDui::controls::layout::DuiCanvas absolute;
    auto canvasFixed = std::make_unique<Control>();
    Control* canvasFixedRaw = canvasFixed.get();
    absolute.AddChild(std::move(canvasFixed), {10, 5, 40, 20});
    auto canvasSpin = std::make_unique<ysDui::controls::input::DuiSpinBox>();
    Control* canvasSpinRaw = canvasSpin.get();
    const Size canvasSpinDesired = canvasSpin->DesiredSize();
    absolute.AddChild(std::move(canvasSpin), {4, 30, 0, 0});
    absolute.Layout({0, 0, 200, 100});
    assert((canvasFixedRaw->Bounds() == Rect{10, 5, 50, 25}));
    assert((canvasSpinRaw->Bounds() == Rect{4, 30, 4 + canvasSpinDesired.width,
                                           30 + canvasSpinDesired.height}));
    // DesiredSize 覆盖全部子项
    const Size canvasDesired = absolute.DesiredSize();
    assert(canvasDesired.width >= 50 && canvasDesired.height >= 30 + canvasSpinDesired.height);

    // SetPosition 只移动，保留原有尺寸设置
    absolute.SetPosition(canvasFixedRaw, 20, 15);
    absolute.Layout({0, 0, 200, 100});
    assert((canvasFixedRaw->Bounds() == Rect{20, 15, 60, 35}));
    assert((absolute.GetItem(canvasFixedRaw).width == 40));

    auto autoRoot = std::make_unique<ysDui::controls::layout::DuiVBox>();
    autoRoot->SetGap(4);
    auto firstSlot = std::make_unique<Control>();
    Control* firstSlotRaw = firstSlot.get();
    autoRoot->AddChild(std::move(firstSlot), ysDui::controls::layout::DuiLayoutHint{}.Fixed(20));
    autoRoot->SetBounds({0, 0, 100, 80});
    assert(autoRoot->LayoutDirty());
    Host host;
    ysDui::controls::layout::DuiVBox* autoBox = autoRoot.get();
    host.SetRoot(std::move(autoRoot));
    host.PrepareFrame();
    assert(!autoBox->LayoutDirty());
    assert((firstSlotRaw->Bounds() == Rect{0, 0, 100, 20}));
    auto secondSlot = std::make_unique<Control>();
    Control* secondSlotRaw = secondSlot.get();
    autoBox->AddChild(std::move(secondSlot), ysDui::controls::layout::DuiLayoutHint{}.Fixed(20));
    assert(autoBox->LayoutDirty());
    host.PrepareFrame();
    assert((secondSlotRaw->Bounds() == Rect{0, 24, 100, 44}));
}
