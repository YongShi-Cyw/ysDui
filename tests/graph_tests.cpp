/**
 * 文件名：graph_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：验证蓝图节点编辑器的模型校验、视图变换、命中测试、交互、撤销与序列化。
 */
#include "test_support.hpp"

int main()
{
    using namespace ysDui::core;
    using namespace ysDui::controls::graph;

    // ---------------------------------------------------------------- 模型校验
    {
        DuiNodeGraph graph;
        assert(graph.Empty());
        const DuiNodeId a = graph.AddNode("A", {0, 0});
        const DuiNodeId b = graph.AddNode("B", {200, 0});
        assert(a.Valid() && b.Valid() && !graph.Empty());
        assert(graph.Nodes().size() == 2);

        const DuiPinId aOut = graph.AddPin(a, "out", DuiPinKind::Output);
        const DuiPinId bIn = graph.AddPin(b, "in", DuiPinKind::Input);
        const DuiPinId aIn = graph.AddPin(a, "in", DuiPinKind::Input);
        assert(aOut.Valid() && bIn.Valid() && aIn.Valid());
        // 节点不存在时加引脚失败
        assert(!graph.AddPin(DuiNodeId{9999}, "x", DuiPinKind::Input).Valid());

        // 合法：输出 -> 输入
        assert(graph.CanLink(aOut, bIn));
        const DuiLinkId link = graph.AddLink(aOut, bIn);
        assert(link.Valid() && graph.Links().size() == 1);
        // 重复连线被拒绝
        assert(!graph.CanLink(aOut, bIn));
        assert(!graph.AddLink(aOut, bIn).Valid());
        // 方向不合法：输入 -> 输入、输出 -> 输出
        assert(!graph.CanLink(aIn, bIn));
        assert(!graph.CanLink(aOut, graph.AddPin(b, "out2", DuiPinKind::Output)));
        // 自环：同一节点内部引脚互不相连
        assert(!graph.CanLink(aOut, aIn));
        // 同一引脚自身
        assert(!graph.CanLink(aOut, aOut));

        // 类型：两端都声明且不同则拒绝；空类型通配
        const DuiPinId typedOut = graph.AddPin(a, "fOut", DuiPinKind::Output, false, "float");
        const DuiPinId typedIn = graph.AddPin(b, "fIn", DuiPinKind::Input, false, "float");
        const DuiPinId intIn = graph.AddPin(b, "iIn", DuiPinKind::Input, false, "int");
        const DuiPinId anyIn = graph.AddPin(b, "anyIn", DuiPinKind::Input);
        assert(graph.CanLink(typedOut, typedIn));
        assert(!graph.CanLink(typedOut, intIn));
        assert(graph.CanLink(typedOut, anyIn));

        assert(graph.ConnectedPin(aOut) == bIn && graph.ConnectedPin(bIn) == aOut);
        assert(graph.LinksOfPin(aOut).size() == 1);
        assert(graph.LinksOfNode(b).size() == 1);
        assert(graph.PinsOf(a).size() == 3);

        // 删除节点连带删除引脚与连线
        assert(graph.RemoveNode(b));
        assert(graph.Links().empty());
        assert(!graph.FindPin(bIn));
        assert(graph.FindNode(a) != nullptr);
        assert(!graph.RemoveNode(DuiNodeId{9999}));
    }

    // ---------------------------------------------------------------- 单连接顶替与多连接
    {
        DuiNodeGraph graph;
        const DuiNodeId source = graph.AddNode("src");
        const DuiNodeId sink1 = graph.AddNode("sink1");
        const DuiNodeId sink2 = graph.AddNode("sink2");
        const DuiPinId out = graph.AddPin(source, "out", DuiPinKind::Output);
        const DuiPinId in1 = graph.AddPin(sink1, "in", DuiPinKind::Input);
        const DuiPinId in2 = graph.AddPin(sink2, "in", DuiPinKind::Input);
        // 默认单连接：新连线顶掉旧连线
        assert(graph.AddLink(out, in1).Valid());
        assert(graph.Links().size() == 1);
        assert(graph.AddLink(out, in2).Valid());
        assert(graph.Links().size() == 1);
        assert(graph.ConnectedPin(out) == in2);

        // 多连接引脚可同时接多条
        const DuiNodeId hub = graph.AddNode("hub");
        const DuiPinId hubIn = graph.AddPin(hub, "in", DuiPinKind::Input, true);
        const DuiPinId hubOut = graph.AddPin(hub, "out", DuiPinKind::Output, true);
        assert(graph.AddLink(out, hubIn).Valid());
        assert(graph.AddLink(hubOut, in1).Valid());
        assert(graph.AddLink(hubOut, in2).Valid());
        assert(graph.LinksOfPin(hubOut).size() == 2);
        assert(graph.BreakLinksOfNode(hub) == 3);
        assert(graph.Links().empty());
        // 节点与引脚保留
        assert(graph.FindNode(hub) != nullptr && graph.FindPin(hubOut) != nullptr);
    }

    // ---------------------------------------------------------------- 分组
    {
        DuiNodeGraph graph;
        const DuiNodeId group = graph.AddGroup("Group", {0, 0, 400, 300});
        const DuiNodeId inside = graph.AddNode("inside", {50, 60});
        const DuiNodeId outside = graph.AddNode("outside", {500, 400});
        // 归属由几何推导
        assert(graph.NodeBelongsTo(inside, group));
        assert(!graph.NodeBelongsTo(outside, group));
        assert(!graph.NodeBelongsTo(group, group));

        // 整组拖动同时平移组内节点，组外不动
        assert(graph.MoveGroup(group, {10, 20}));
        const DuiGraphNode* groupEntry = graph.FindNode(group);
        const DuiGraphNode* insideEntry = graph.FindNode(inside);
        const DuiGraphNode* outsideEntry = graph.FindNode(outside);
        assert(groupEntry != nullptr && groupEntry->position.x == 10 && groupEntry->position.y == 20);
        assert(insideEntry != nullptr && insideEntry->position.x == 60 && insideEntry->position.y == 80);
        assert(outsideEntry != nullptr && outsideEntry->position.x == 500);
        const Rect content = graph.ContentBounds();
        assert(!content.Empty() && content.left <= 10 && content.right >= 500);
        graph.RefreshGroupMembership();
        assert(graph.FindNode(inside)->group == group);
        assert(!graph.FindNode(outside)->group.Valid());
        // 大位移后仍按显式归属平移组内节点
        assert(graph.MoveGroup(group, {1000, 0}));
        assert(graph.FindNode(inside)->position.x == 1060);
        assert(graph.FindNode(outside)->position.x == 500);
        // 非分组节点不能当分组移动
        assert(!graph.MoveGroup(inside, {1, 1}));
        assert(!graph.SetGroupSize(inside, {10, 10}));
    }

    // ---------------------------------------------------------------- 编辑器：布局 / 变换 / 缩放
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        DuiNodeGraph& graph = editor.Graph();
        const DuiNodeId node = graph.AddNode("Node", {100, 100});
        graph.AddPin(node, "in", DuiPinKind::Input);
        graph.AddPin(node, "out", DuiPinKind::Output);
        editor.NotifyGraphChanged();

        const DuiGraphNode* entry = editor.Graph().FindNode(node);
        assert(entry != nullptr);
        // 布局把尺寸写回模型，且不小于样式下限
        assert(entry->size.width >= editor.Style().minNodeWidth);
        assert(entry->size.height > editor.Style().headerHeight);
        assert((editor.DesiredSize() == Size{640, 420}));

        // 缩放钳制到样式范围
        editor.SetZoom(100.0);
        assert(editor.Zoom() == editor.Style().maxZoom);
        editor.SetZoom(0.001);
        assert(editor.Zoom() == editor.Style().minZoom);
        // 以锚点缩放时锚点处的画布坐标保持不变
        editor.SetPanOffset({0, 0});
        editor.SetZoom(1.0);
        const Point anchor{400, 300};
        const Point before = editor.ScreenToCanvas(anchor);
        editor.SetZoom(2.0, anchor);
        const Point after = editor.ScreenToCanvas(anchor);
        assert(std::abs(after.x - before.x) <= 1 && std::abs(after.y - before.y) <= 1);
        // 屏幕/画布往返一致
        editor.SetZoom(1.5);
        editor.SetPanOffset({37, -19});
        const Point canvasPoint{123, 456};
        const Point roundTrip = editor.ScreenToCanvas(editor.CanvasToScreen(canvasPoint));
        assert(std::abs(roundTrip.x - canvasPoint.x) <= 1 && std::abs(roundTrip.y - canvasPoint.y) <= 1);
        editor.SetZoom(1.0);
        editor.SetPanOffset({0, 0});

        // FrameAll 后全部内容必须落在控件矩形内（留边距），且尽量填满
        editor.FrameAll();
        const Rect viewport = editor.Bounds();
        int widestSpan{};
        for (const auto& graphNode : editor.Graph().Nodes())
        {
            const Point topLeft = editor.CanvasToScreen(graphNode.position);
            const Point bottomRight = editor.CanvasToScreen(
                {graphNode.position.x + graphNode.size.width,
                 graphNode.position.y + graphNode.size.height});
            assert(topLeft.x >= viewport.left && topLeft.y >= viewport.top);
            assert(bottomRight.x <= viewport.right && bottomRight.y <= viewport.bottom);
            widestSpan = std::max(widestSpan, bottomRight.x - topLeft.x);
        }
        // 适配后内容应占据控件宽度的合理比例，而不是缩成一小团
        assert(widestSpan >= viewport.Width() / 2);

        // 文字必须随缩放改变字号：渲染层没有画布变换，若字号恒定则小缩放下文字会
        // 相对节点框过大、引脚标签互相挤压（这正是"粗糙"观感的主因）。
        RecordingCanvas zoomedOut;
        editor.SetZoom(1.0);
        (void)editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {0, 0}));
        editor.Paint(zoomedOut, editor.Bounds());
        RecordingCanvas zoomedIn;
        editor.SetZoom(2.0);
        editor.Paint(zoomedIn, editor.Bounds());
        assert(!zoomedOut.drawnTextStyles.empty() && !zoomedIn.drawnTextStyles.empty());
        assert(zoomedIn.drawnTextStyles.front().pointSize > zoomedOut.drawnTextStyles.front().pointSize);
        // 字号有下限，不会缩到 0 或负数
        editor.SetZoom(editor.Style().minZoom);
        RecordingCanvas minZoom;
        editor.Paint(minZoom, editor.Bounds());
        for (const auto& style : minZoom.drawnTextStyles)
            assert(style.pointSize >= 1);
        editor.SetZoom(1.0);

        // HUD：右下角显示缩放百分比；空图时给出操作提示（均为可观察的绘制输出）
        RecordingCanvas hudCanvas;
        editor.Paint(hudCanvas, editor.Bounds());
        const auto containsText = [](const RecordingCanvas& canvas, std::string_view needle)
        {
            return std::any_of(canvas.drawnTexts.begin(), canvas.drawnTexts.end(),
                               [needle](const std::string& text)
                               { return text.find(needle) != std::string::npos; });
        };
        assert(containsText(hudCanvas, "zoom 100%"));
        editor.SetZoom(2.0);
        RecordingCanvas hudZoomed;
        editor.Paint(hudZoomed, editor.Bounds());
        assert(containsText(hudZoomed, "zoom 200%"));
        editor.SetZoom(1.0);

        DuiNodeEditor emptyEditor;
        emptyEditor.SetTextMeasurer(&measurer);
        emptyEditor.SetBounds({0, 0, 400, 240});
        RecordingCanvas emptyCanvas;
        emptyEditor.Paint(emptyCanvas, emptyEditor.Bounds());
        assert(containsText(emptyCanvas, "Empty graph"));
    }

    // ---------------------------------------------------------------- 编辑器：命中 / 选择 / 拖动
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        editor.SetPanOffset({0, 0});
        editor.SetZoom(1.0);
        DuiNodeGraph& graph = editor.Graph();
        const DuiNodeId first = graph.AddNode("First", {100, 100});
        const DuiPinId firstOut = graph.AddPin(first, "out", DuiPinKind::Output);
        const DuiNodeId second = graph.AddNode("Second", {400, 100});
        const DuiPinId secondIn = graph.AddPin(second, "in", DuiPinKind::Input);
        editor.NotifyGraphChanged();

        const DuiGraphNode* firstEntry = editor.Graph().FindNode(first);
        assert(firstEntry != nullptr);
        // 节点中心命中该节点
        const Point firstCenter = editor.CanvasToScreen(
            {firstEntry->position.x + firstEntry->size.width / 2,
             firstEntry->position.y + firstEntry->size.height / 2});
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, firstCenter)));
        assert(editor.HoveredNode() == first);

        // 点击选中
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, firstCenter)));
        assert(editor.IsNodeSelected(first));
        assert(editor.SelectedNodes().size() == 1);
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, firstCenter)));

        // 拖动节点：位置按画布增量平移，且可撤销
        const Point dragStart = firstCenter;
        const Point dragEnd{firstCenter.x + 60, firstCenter.y + 40};
        const Point positionBefore = editor.Graph().FindNode(first)->position;
        editor.ClearHistory();
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, dragStart)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, dragEnd)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, dragEnd)));
        const Point positionAfter = editor.Graph().FindNode(first)->position;
        assert(positionAfter.x == positionBefore.x + 60 && positionAfter.y == positionBefore.y + 40);
        assert(editor.CanUndo());
        assert(editor.Undo());
        assert(editor.Graph().FindNode(first)->position == positionBefore);
        assert(editor.Redo());
        assert(editor.Graph().FindNode(first)->position == positionAfter);
        editor.ClearHistory();

        // 框选：从空白拖过两个节点
        editor.ClearSelection();
        assert(editor.SelectedNodes().empty());
        assert(editor.OnEvent(ysDui::test::MakeEvent(
            EventType::PointerDown, editor.CanvasToScreen({50, 50}))));
        assert(editor.OnEvent(ysDui::test::MakeEvent(
            EventType::PointerMove, editor.CanvasToScreen({900, 400}))));
        assert(editor.OnEvent(ysDui::test::MakeEvent(
            EventType::PointerUp, editor.CanvasToScreen({900, 400}))));
        assert(editor.SelectedNodes().size() == 2);

        // 批量拖拽：在已选中的节点上按下不得清空选择，两节点应同位移
        {
            const Point beforeA = editor.Graph().FindNode(first)->position;
            const Point beforeB = editor.Graph().FindNode(second)->position;
            const Point batchStart = editor.CanvasToScreen(
                {beforeA.x + editor.Graph().FindNode(first)->size.width / 2,
                 beforeA.y + editor.Graph().FindNode(first)->size.height / 2});
            const Point batchEnd{batchStart.x + 40, batchStart.y + 30};
            assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, batchStart)));
            assert(editor.SelectedNodes().size() == 2); // 按下后仍保持多选
            assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, batchEnd)));
            assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, batchEnd)));
            assert(editor.Graph().FindNode(first)->position.x == beforeA.x + 40);
            assert(editor.Graph().FindNode(first)->position.y == beforeA.y + 30);
            assert(editor.Graph().FindNode(second)->position.x == beforeB.x + 40);
            assert(editor.Graph().FindNode(second)->position.y == beforeB.y + 30);
        }

        // Shift+左键加选；Shift+框选追加
        {
            editor.ClearSelection();
            const Point shiftFirstCenter = editor.CanvasToScreen(
                {editor.Graph().FindNode(first)->position.x + editor.Graph().FindNode(first)->size.width / 2,
                 editor.Graph().FindNode(first)->position.y + editor.Graph().FindNode(first)->size.height / 2});
            const Point secondCenter = editor.CanvasToScreen(
                {editor.Graph().FindNode(second)->position.x + editor.Graph().FindNode(second)->size.width / 2,
                 editor.Graph().FindNode(second)->position.y + editor.Graph().FindNode(second)->size.height / 2});
            assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, shiftFirstCenter)));
            assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, shiftFirstCenter)));
            assert(editor.SelectedNodes().size() == 1);
            assert(editor.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerDown, secondCenter, 0, modifier::Shift)));
            assert(editor.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerUp, secondCenter, 0, modifier::Shift)));
            assert(editor.SelectedNodes().size() == 2);
            assert(editor.IsNodeSelected(first) && editor.IsNodeSelected(second));

            editor.ClearSelection();
            editor.SelectNode(first);
            assert(editor.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerDown, editor.CanvasToScreen({50, 50}), 0, modifier::Shift)));
            assert(editor.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerMove, editor.CanvasToScreen({900, 400}), 0, modifier::Shift)));
            assert(editor.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerUp, editor.CanvasToScreen({900, 400}), 0, modifier::Shift)));
            assert(editor.IsNodeSelected(first) && editor.IsNodeSelected(second));
        }

        // Ctrl+拖动复制节点；原稿位置不变，撤销去掉副本
        {
            editor.ClearSelection();
            editor.ClearHistory();
            const std::size_t nodeCount = editor.Graph().Nodes().size();
            const Point origin = editor.Graph().FindNode(first)->position;
            const Point start = editor.CanvasToScreen(
                {origin.x + editor.Graph().FindNode(first)->size.width / 2,
                 origin.y + editor.Graph().FindNode(first)->size.height / 2});
            const Point end{start.x + 50, start.y + 20};
            assert(editor.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerDown, start, 0, modifier::Control)));
            assert(editor.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerMove, end, 0, modifier::Control)));
            assert(editor.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerUp, end, 0, modifier::Control)));
            assert(editor.Graph().Nodes().size() == nodeCount + 1);
            assert(editor.Graph().FindNode(first)->position == origin);
            DuiNodeId copy{};
            for (const auto& node : editor.Graph().Nodes())
            {
                if (node.id != first && node.id != second && node.title == "First")
                    copy = node.id;
            }
            assert(copy.Valid());
            assert(editor.Graph().FindNode(copy)->position.x == origin.x + 50);
            assert(editor.Undo());
            assert(editor.Graph().FindNode(copy) == nullptr);
            assert(editor.Graph().FindNode(first)->position == origin);
        }

        // 从输出引脚拉到输入引脚建立连线
        editor.ClearSelection();
        const Point outPinPoint = editor.CanvasToScreen(
            {editor.Graph().FindNode(first)->position.x + editor.Graph().FindNode(first)->size.width,
             editor.Graph().FindNode(first)->position.y + editor.Style().headerHeight
                 + editor.Style().pinRowHeight / 2});
        const Point inPinPoint = editor.CanvasToScreen(
            {editor.Graph().FindNode(second)->position.x,
             editor.Graph().FindNode(second)->position.y + editor.Style().headerHeight
                 + editor.Style().pinRowHeight / 2});
        assert(editor.Graph().Links().empty());
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, outPinPoint)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, inPinPoint)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, inPinPoint)));
        assert(editor.Graph().Links().size() == 1);
        assert(editor.Graph().ConnectedPin(firstOut) == secondIn);

        // Alt + 空白松手：断开引脚上的连线
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, outPinPoint)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(
            EventType::PointerUp, editor.CanvasToScreen({50, 50}), 0, modifier::Alt)));
        assert(editor.Graph().Links().empty());

        // 可键盘聚焦：Host 依此把 Delete / Ctrl+Z 路由到画布
        assert(editor.Accessibility().keyboardFocusable);

        // 滚轮缩放以光标为锚点
        const double zoomBefore = editor.Zoom();
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerWheel, {400, 300}, 0, 0, 0, 120)));
        assert(editor.Zoom() > zoomBefore);
    }

    // ---------------------------------------------------------------- 撤销 / 重做 / 复制粘贴 / 删除
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        DuiNodeGraph& graph = editor.Graph();
        const DuiNodeId node = graph.AddNode("N", {10, 10});
        editor.NotifyGraphChanged();
        assert(!editor.CanUndo() && !editor.CanRedo());

        // 删除 -> 撤销 -> 重做
        editor.SelectNode(node);
        assert(editor.DeleteSelection());
        assert(editor.Graph().Nodes().empty());
        assert(editor.CanUndo());
        assert(editor.Undo());
        assert(editor.Graph().Nodes().size() == 1);
        assert(editor.CanRedo());
        assert(editor.Redo());
        assert(editor.Graph().Nodes().empty());
        assert(editor.Undo());
        assert(editor.Graph().Nodes().size() == 1);
        editor.ClearHistory();
        assert(!editor.CanUndo() && !editor.CanRedo());

        // 复制粘贴：新节点不与原节点重叠，且继承标题与引脚
        const DuiNodeId source = editor.Graph().AddNode("WithPins", {200, 200});
        editor.Graph().AddPin(source, "in", DuiPinKind::Input);
        editor.Graph().AddPin(source, "out", DuiPinKind::Output);
        editor.NotifyGraphChanged();
        editor.SelectNode(source);
        assert(editor.CopySelection() && editor.HasClipboardContent());
        assert(editor.PasteClipboard({}));
        assert(editor.Graph().Nodes().size() == 3);
        // 原 2 个引脚 + 粘贴出的 2 个
        assert(editor.Graph().Pins().size() == 4);
        // 粘贴后选中新节点
        assert(editor.SelectedNodes().size() == 1);

        // 两端都在选择集内的连线一并复制
        {
            DuiNodeEditor linked;
            linked.SetTextMeasurer(&measurer);
            linked.SetBounds({0, 0, 800, 600});
            const DuiNodeId a = linked.Graph().AddNode("A", {10, 10});
            const DuiNodeId b = linked.Graph().AddNode("B", {240, 10});
            const DuiPinId aOut = linked.Graph().AddPin(a, "out", DuiPinKind::Output);
            const DuiPinId bIn = linked.Graph().AddPin(b, "in", DuiPinKind::Input);
            assert(linked.Graph().AddLink(aOut, bIn).Valid());
            linked.NotifyGraphChanged();
            linked.SelectNode(a);
            linked.SelectNode(b, true);
            assert(linked.CopySelection());
            assert(linked.PasteClipboard({400, 400}));
            assert(linked.Graph().Nodes().size() == 4);
            assert(linked.Graph().Links().size() == 2);
        }

        // 剪切：复制后删除，撤销可还原
        {
            DuiNodeEditor cutEditor;
            cutEditor.SetTextMeasurer(&measurer);
            cutEditor.SetBounds({0, 0, 800, 600});
            const DuiNodeId cutNode = cutEditor.Graph().AddNode("CutMe", {40, 40});
            cutEditor.NotifyGraphChanged();
            cutEditor.SelectNode(cutNode);
            assert(cutEditor.CutSelection());
            assert(cutEditor.Graph().Nodes().empty());
            assert(cutEditor.HasClipboardContent());
            assert(cutEditor.Undo());
            assert(cutEditor.Graph().Nodes().size() == 1);
            assert(cutEditor.PasteClipboard({300, 300}));
            assert(cutEditor.Graph().Nodes().size() == 2);
        }

        // 右键选中节点弹出剪切/复制/删除菜单
        {
            DuiNodeEditor menuEditor;
            menuEditor.SetTextMeasurer(&measurer);
            menuEditor.SetBounds({0, 0, 800, 600});
            const DuiNodeId menuNode = menuEditor.Graph().AddNode("Menu", {80, 80});
            menuEditor.NotifyGraphChanged();
            menuEditor.ResetView();
            UiHostFactoryMock menuFactory;
            menuEditor.SetPopupContext(menuFactory, {});
            const Point nodeScreen = menuEditor.CanvasToScreen({100, 100});
            assert(menuEditor.OnEvent(ysDui::test::MakeEvent(
                EventType::PointerDown, nodeScreen, 0, 0, 0, 0,
                PointerButton::Secondary)));
            assert(menuEditor.IsNodeSelected(menuNode));
            assert(menuFactory.popup != nullptr && !menuEditor.Captured());
            // 点菜单第一项 Cut：节点应被移除并进入剪贴板
            assert(menuFactory.popup->Dispatch(ysDui::test::MakeEvent(
                EventType::PointerDown, {20, 12})));
            assert(menuEditor.Graph().Nodes().empty());
            assert(menuEditor.HasClipboardContent());
        }

        // 只读模式禁止一切修改
        editor.SetReadOnly(true);
        assert(editor.ReadOnly());
        assert(!editor.DeleteSelection());
        assert(!editor.CutSelection());
        assert(!editor.PasteClipboard({}));
        assert(!editor.Undo());
        editor.SetReadOnly(false);
    }

    // ---------------------------------------------------------------- 类型过滤器与断开
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        const DuiNodeId a = editor.Graph().AddNode("A", {10, 10});
        const DuiNodeId b = editor.Graph().AddNode("B", {240, 10});
        const DuiPinId aOut = editor.Graph().AddPin(a, "out", DuiPinKind::Output, false, "float");
        const DuiPinId bIn = editor.Graph().AddPin(b, "in", DuiPinKind::Input, false, "float");
        assert(editor.Graph().AddLink(aOut, bIn).Valid());
        editor.NotifyGraphChanged();
        editor.SelectNode(a);
        assert(editor.DisconnectSelection());
        assert(editor.Graph().Links().empty());
        assert(editor.Undo());
        assert(editor.Graph().Links().size() == 1);
        editor.SelectNode(a);
        assert(editor.DisconnectSelection());
        assert(editor.Graph().CanLink(aOut, bIn));
        editor.SetCanLinkFilter([](DuiPinId, DuiPinId) { return false; });
        // 过滤器只约束交互，不改模型 CanLink
        assert(editor.Graph().CanLink(aOut, bIn));
    }

    // ---------------------------------------------------------------- 序列化
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        DuiNodeGraph& graph = editor.Graph();
        const DuiNodeId a = graph.AddNode("Alpha & <Beta>", {12, 34});
        DuiGraphNode* aNode = graph.FindNode(a);
        assert(aNode != nullptr);
        aNode->color = {45, 108, 223, 255};
        aNode->hasColor = true;
        const DuiPinId aOut = graph.AddPin(a, "out", DuiPinKind::Output, true, "flow");
        const DuiNodeId b = graph.AddNode("Second", {200, 100});
        const DuiPinId bIn = graph.AddPin(b, "in", DuiPinKind::Input);
        const DuiNodeId group = graph.AddGroup("Group 1", {0, 0, 400, 300});
        assert(graph.AddLink(aOut, bIn).Valid());
        editor.NotifyGraphChanged();
        editor.SetZoom(1.5);
        editor.SetPanOffset({33, -17});
        (void)group;

        const std::string xml = editor.ToXml();
        assert(xml.find("nodeGraph") != std::string::npos);
        // 特殊字符被转义，不会破坏属性
        assert(xml.find("&amp;") != std::string::npos);
        assert(xml.find("color=\"45,108,223,255\"") != std::string::npos);
        assert(xml.find("type=\"flow\"") != std::string::npos);
        assert(xml.find("<view ") != std::string::npos);

        DuiNodeEditor restored;
        restored.SetTextMeasurer(&measurer);
        restored.SetBounds({0, 0, 800, 600});
        assert(restored.FromXml(xml));
        assert(restored.Graph().Nodes().size() == 3);
        assert(restored.Graph().Pins().size() == 2);
        assert(restored.Graph().Links().size() == 1);
        assert(std::abs(restored.Zoom() - 1.5) < 1e-9);
        const Point expectedPan{33, -17};
        assert(restored.PanOffset() == expectedPan);
        const Color expectedColor{45, 108, 223, 255};
        bool colorRestored{};
        for (const auto& node : restored.Graph().Nodes())
        {
            if (node.title == "Alpha & <Beta>")
                colorRestored = node.hasColor && node.color == expectedColor;
        }
        assert(colorRestored);
        // 标题往返一致（转义已还原）
        bool foundTitle{};
        for (const auto& node : restored.Graph().Nodes())
            foundTitle = foundTitle || node.title == "Alpha & <Beta>";
        assert(foundTitle);
        // 分组种类保留
        int groupCount{};
        for (const auto& node : restored.Graph().Nodes())
        {
            if (node.kind == DuiNodeKind::Group)
                ++groupCount;
        }
        assert(groupCount == 1);
        // 多连接标记保留
        bool multipleFound{};
        for (const auto& pin : restored.Graph().Pins())
            multipleFound = multipleFound || (pin.name == "out" && pin.multipleConnections);
        assert(multipleFound);

        // 非法输入不破坏原模型
        const std::size_t nodesBefore = restored.Graph().Nodes().size();
        assert(!restored.FromXml("not xml"));
        assert(!restored.FromXml("<other/>"));
        assert(restored.Graph().Nodes().size() == nodesBefore);

        // 再次序列化应得到等价文本（往返稳定）
        assert(restored.ToXml() == xml);
    }

    // ---------------------------------------------------------------- 标题栏底部为直角
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        const DuiNodeId node = editor.Graph().AddNode("Corner", {0, 0});
        editor.NotifyGraphChanged();
        assert((editor.PanOffset() == ysDui::core::Point{0, 0}));
        editor.SetPanOffset({40, 40});
        editor.SetZoom(1.0);
        RecordingCanvas canvas;
        editor.Paint(canvas, editor.Bounds());

        const DuiGraphNode* entry = editor.Graph().FindNode(node);
        assert(entry != nullptr);
        const int radius = editor.Style().nodeRounding;
        const ysDui::core::Point topLeft = editor.CanvasToScreen(entry->position);
        const ysDui::core::Point bottomRight = editor.CanvasToScreen(
            {entry->position.x + entry->size.width, entry->position.y + entry->size.height});
        // 标题栏底边 = 节点顶边 + 标题栏高度（缩放为 1）
        const int headerBottom = topLeft.y + editor.Style().headerHeight;

        // 本体仍是四角圆角：FillRoundedRect 记录的是完整节点矩形与圆角半径
        assert(canvas.roundedRadius == radius);
        assert((canvas.rounded.bounds == ysDui::core::Rect{topLeft.x, topLeft.y,
                                                           bottomRight.x, bottomRight.y}));

        // 标题栏底部两角被方角（半径 0）补块填平，且补块正好贴在左右边缘上。
        // 缺口是个 radius×radius 的方角，圆角在外，故补块必须与底边同高、与左/右边缘对齐。
        const ysDui::core::Rect expectedLeft{topLeft.x, headerBottom - radius,
                                             topLeft.x + radius, headerBottom};
        const ysDui::core::Rect expectedRight{bottomRight.x - radius, headerBottom - radius,
                                              bottomRight.x, headerBottom};
        bool leftSquared{};
        bool rightSquared{};
        for (const auto& gradient : canvas.linearGradients)
        {
            if (gradient.radius != 0)
                continue; // 带圆角的是标题栏本体或其它圆角填充，不是补块
            leftSquared = leftSquared || gradient.bounds == expectedLeft;
            rightSquared = rightSquared || gradient.bounds == expectedRight;
        }
        assert(leftSquared && rightSquared);
    }

    // ---------------------------------------------------------------- 内嵌控件
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        const DuiNodeId node = editor.Graph().AddNode("WithContent", {50, 50});
        editor.NotifyGraphChanged();
        // 未设置内容工厂时不创建子节点
        assert(editor.Children().empty());

        int created{};
        editor.SetNodeContentFactory([&created](DuiNodeId, std::string_view)
        {
            ++created;
            return std::make_unique<Control>();
        });
        assert(created == 1);
        assert(editor.Children().size() == 1);
        // 节点布局为内容预留了高度
        assert(editor.Graph().FindNode(node)->size.height > editor.Style().headerHeight);

        // 删除节点后内容被回收
        editor.SelectNode(node);
        (void)editor.DeleteSelection();
        assert(editor.Children().empty());
        // 撤销恢复节点时内容重新创建
        assert(editor.Undo());
        assert(editor.Graph().FindNode(node) != nullptr);
        assert(editor.Children().size() == 1);

        // 引脚内联控件：输入引脚控件右对齐、输出引脚左对齐，标签占用剩余空间
        editor.SetNodeContentFactory(nullptr);
        assert(editor.Children().empty());
        DuiNodeGraph& inlineGraph = editor.Graph();
        const DuiNodeId inlineNode = inlineGraph.AddNode("Inline", {0, 0});
        const DuiPinId inlineIn = inlineGraph.AddPin(inlineNode, "value", DuiPinKind::Input);
        const DuiPinId inlineOut = inlineGraph.AddPin(inlineNode, "out", DuiPinKind::Output);
        editor.NotifyGraphChanged();

        // 首选尺寸路径：建议的用法（重写 DesiredSize）。
        class SizedControl final : public Control
        {
        public:
            [[nodiscard]] ysDui::core::Size DesiredSize() const override { return {80, 20}; }
        };
        int pinWidgetsCreated{};
        editor.SetPinContentFactory([&pinWidgetsCreated](DuiPinId, std::string_view)
        {
            ++pinWidgetsCreated;
            return std::make_unique<SizedControl>();
        });
        assert(pinWidgetsCreated == 2);
        assert(editor.Children().size() == 2);
        assert(editor.PinContent(inlineIn) != nullptr);
        assert(editor.PinContent(inlineOut) != nullptr);
        assert(editor.PinContent(DuiPinId{9999}) == nullptr);

        // 内联控件必须有非空槽位，且落在节点水平范围内
        const DuiGraphNode* inlineEntry = editor.Graph().FindNode(inlineNode);
        assert(inlineEntry != nullptr);
        const ysDui::core::Rect inWidget = editor.PinContent(inlineIn)->Bounds();
        const ysDui::core::Rect outWidget = editor.PinContent(inlineOut)->Bounds();
        assert(!inWidget.Empty() && !outWidget.Empty());
        const ysDui::core::Point nodeLeft = editor.CanvasToScreen(inlineEntry->position);
        const ysDui::core::Point nodeRight = editor.CanvasToScreen(
            {inlineEntry->position.x + inlineEntry->size.width,
             inlineEntry->position.y + inlineEntry->size.height});
        assert(inWidget.left >= nodeLeft.x && inWidget.right <= nodeRight.x);
        assert(outWidget.left >= nodeLeft.x && outWidget.right <= nodeRight.x);
        // 输入控件靠右、输出控件靠左
        assert(inWidget.left > outWidget.left);
        // 缩放偏离 1:1 时控件仍可见（FrameAll / 滚轮不应把它们藏掉）
        editor.SetZoom(1.1);
        assert(editor.PinContent(inlineIn)->Visible());
        assert(!editor.PinContent(inlineIn)->Bounds().Empty());
        editor.SetZoom(1.0);

        // 上层节点挡住下层节点时，下层内嵌控件不得抢走点击
        {
            DuiNodeEditor stacked;
            stacked.SetTextMeasurer(&measurer);
            stacked.SetBounds({0, 0, 800, 600});
            stacked.SetPanOffset({0, 0});
            stacked.SetZoom(1.0);
            const DuiNodeId back = stacked.Graph().AddNode("Back", {0, 0});
            const DuiPinId backIn = stacked.Graph().AddPin(back, "in", DuiPinKind::Input);
            const DuiNodeId frontId = stacked.Graph().AddNode("Front", {0, 0});
            int widgetHits{};
            class HitControl final : public Control
            {
            public:
                explicit HitControl(int* hits) : hits_(hits) {}
                [[nodiscard]] Size DesiredSize() const override { return {80, 20}; }
                bool OnEvent(const Event& event) override
                {
                    if (hits_ != nullptr && event.type != EventType::PointerLeave)
                        ++(*hits_);
                    return true;
                }
            private:
                int* hits_{};
            };
            stacked.SetPinContentFactory([&widgetHits, backIn](DuiPinId pin, std::string_view)
            {
                if (pin != backIn)
                    return std::unique_ptr<Control>{};
                return std::unique_ptr<Control>(std::make_unique<HitControl>(&widgetHits));
            });
            stacked.NotifyGraphChanged();
            const Rect widget0 = stacked.PinContent(backIn)->Bounds();
            assert(!widget0.Empty());
            const Point widgetCanvas = stacked.ScreenToCanvas({widget0.left, widget0.top});
            stacked.Graph().FindNode(frontId)->position = {widgetCanvas.x - 8, widgetCanvas.y - 8};
            stacked.NotifyGraphChanged();
            stacked.SelectNode(frontId);
            const DuiGraphNode* front = stacked.Graph().FindNode(frontId);
            const Rect widget = stacked.PinContent(backIn)->Bounds();
            const Point click{(widget.left + widget.right) / 2, (widget.top + widget.bottom) / 2};
            const Rect frontScreen = {stacked.CanvasToScreen(front->position).x,
                                      stacked.CanvasToScreen(front->position).y,
                                      stacked.CanvasToScreen({front->position.x + front->size.width,
                                                              front->position.y + front->size.height}).x,
                                      stacked.CanvasToScreen({front->position.x + front->size.width,
                                                              front->position.y + front->size.height}).y};
            assert(frontScreen.Contains(click));
            widgetHits = 0;
            (void)stacked.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, click));
            assert(stacked.HoveredNode() == frontId);
            (void)stacked.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, click));
            assert(widgetHits == 0);
        }

        // 回退路径：未重写 DesiredSize 但显式设过尺寸的控件同样应占位（否则会静默失效）
        editor.SetPinContentFactory([](DuiPinId, std::string_view)
        {
            auto control = std::make_unique<Control>();
            control->SetBounds({0, 0, 64, 20});
            return control;
        });
        const ysDui::core::Rect fallbackWidget = editor.PinContent(inlineIn)->Bounds();
        assert(!fallbackWidget.Empty() && fallbackWidget.Width() > 0);

        // 引脚删除后其内联控件一并回收
        editor.SetPinContentFactory([](DuiPinId, std::string_view)
        {
            return std::make_unique<SizedControl>();
        });
        assert(editor.Children().size() == 2);
        (void)editor.Graph().RemovePin(inlineIn);
        editor.NotifyGraphChanged();
        assert(editor.Children().size() == 1 && editor.PinContent(inlineIn) == nullptr);
        (void)inlineOut;

        // 引脚形状可切换，且三种形状都能产生绘制输出
        {
            RecordingCanvas squareCanvas;
            editor.SetStyle([] { DuiNodeEditorStyle style; style.pinShape = DuiPinShape::Square; return style; }());
            editor.Paint(squareCanvas, editor.Bounds());
            RecordingCanvas arrowCanvas;
            editor.SetStyle([] { DuiNodeEditorStyle style; style.pinShape = DuiPinShape::Arrow; return style; }());
            editor.Paint(arrowCanvas, editor.Bounds());
            // 箭头用 FillPath，方形/圆形用圆角矩形或椭圆；三者至少有一种图元产出
            assert(!squareCanvas.filledPaths.empty() || !squareCanvas.fills.empty()
                   || !arrowCanvas.filledPaths.empty());
            editor.SetStyle(DuiNodeEditorStyle{});
        }

        // 分组适配内容：收缩到恰好包裹内部节点
        {
            DuiNodeEditor grouped;
            grouped.SetTextMeasurer(&measurer);
            grouped.SetBounds({0, 0, 600, 400});
            const DuiNodeId group = grouped.Graph().AddGroup("G", {0, 0, 900, 900});
            const DuiNodeId inner = grouped.Graph().AddNode("Inner", {100, 100});
            grouped.NotifyGraphChanged();
            const ysDui::core::Size innerSize = grouped.Graph().FindNode(inner)->size;
            assert(grouped.FitGroupToContents(group));
            const DuiGraphNode* fitted = grouped.Graph().FindNode(group);
            assert(fitted != nullptr);
            // 分组应恰好包住内部节点：宽度 = 节点宽 + 两侧内边距
            assert(fitted->size.width == innerSize.width + grouped.Style().nodePadding * 2);
            assert(fitted->size.height == innerSize.height + grouped.Style().nodePadding * 2
                   + grouped.Style().headerHeight);
            assert(fitted->position.x < 100 && fitted->position.y < 100);
            // 无内容的分组无法适配
            const DuiNodeId emptyGroup = grouped.Graph().AddGroup("Empty", {0, 0, 100, 100});
            assert(!grouped.FitGroupToContents(emptyGroup));
            // 非分组节点不适用
            assert(!grouped.FitGroupToContents(inner));
        }

        // 连线流动动画：按连线开启，注入时钟后驱动相位
        {
            DuiNodeEditor flowing;
            flowing.SetTextMeasurer(&measurer);
            flowing.SetBounds({0, 0, 600, 400});
            ysDui::core::AnimationClock clock;
            const DuiNodeId a = flowing.Graph().AddNode("A", {0, 0});
            const DuiPinId aOut = flowing.Graph().AddPin(a, "out", DuiPinKind::Output);
            const DuiNodeId b = flowing.Graph().AddNode("B", {300, 0});
            const DuiPinId bIn = flowing.Graph().AddPin(b, "in", DuiPinKind::Input);
            const DuiLinkId link = flowing.Graph().AddLink(aOut, bIn);
            flowing.NotifyGraphChanged();
            flowing.SetAnimationClock(&clock);
            // 未开启时不应有持续任务（避免无条件重绘）
            assert(!flowing.LinkFlow(link));
            assert(!clock.HasScheduledTasks());
            flowing.SetLinkFlow(link, true);
            assert(flowing.LinkFlow(link));
            // 开启后动画时钟上出现重复任务
            assert(clock.HasScheduledTasks());
            // 推进时钟不崩溃，任务仍在
            clock.Advance(16);
            assert(clock.HasScheduledTasks());
            flowing.SetLinkFlow(link, false);
            assert(!flowing.LinkFlow(link));
            assert(!clock.HasScheduledTasks());
            // 无效连线被忽略
            flowing.SetLinkFlow(DuiLinkId{9999}, true);
            assert(!flowing.LinkFlow(DuiLinkId{9999}));

            // 流动标记应产生椭圆绘制（与引脚圆区分：关闭流动后数量减少）
            RecordingCanvas withFlow;
            flowing.SetLinkFlow(link, true);
            flowing.Paint(withFlow, flowing.Bounds());
            const std::size_t flowingCount = withFlow.ellipses.size();
            flowing.SetLinkFlow(link, false);
            RecordingCanvas withoutFlow;
            flowing.Paint(withoutFlow, flowing.Bounds());
            assert(flowingCount > withoutFlow.ellipses.size());
        }
    }

    // ---------------------------------------------------------------- 增量撤销 / 系统剪贴板 / F2 / 小地图
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        editor.SetPanOffset({0, 0});
        editor.SetZoom(1.0);
        const DuiNodeId node = editor.Graph().AddNode("RenameMe", {40, 40});
        editor.NotifyGraphChanged();
        editor.SelectNode(node);
        const Point origin = editor.Graph().FindNode(node)->position;

        const Point center = editor.CanvasToScreen(
            {origin.x + editor.Graph().FindNode(node)->size.width / 2,
             origin.y + editor.Graph().FindNode(node)->size.height / 2});
        editor.ClearHistory();
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, center)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {center.x + 30, center.y})));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {center.x + 30, center.y})));
        assert(editor.Graph().FindNode(node)->position.x == origin.x + 30);
        assert(editor.CanUndo());
        assert(editor.Undo());
        assert(editor.Graph().FindNode(node)->position == origin);

        TextInputMock renameInput;
        editor.SetTextInput(&renameInput);
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Function2)));
        assert(renameInput.visible && renameInput.focused);
        assert(renameInput.text == "RenameMe");
        renameInput.text = "RenameMeZ";
        renameInput.Submit();
        assert(editor.Graph().FindNode(node)->title == "RenameMeZ");
        assert(!renameInput.visible);
        assert(editor.Undo());
        assert(editor.Graph().FindNode(node)->title == "RenameMe");

        ClipboardMock clipboard;
        editor.SetClipboard(&clipboard);
        assert(editor.CopySelection());
        assert(clipboard.text.find("<nodeGraph") != std::string::npos);
        DuiNodeEditor other;
        other.SetTextMeasurer(&measurer);
        other.SetBounds({0, 0, 800, 600});
        other.SetClipboard(&clipboard);
        assert(other.HasClipboardContent());
        assert(other.PasteClipboard({}));
        assert(other.Graph().Nodes().size() == 1);

        const Point panBefore = editor.PanOffset();
        RecordingCanvas painted;
        editor.Paint(painted, editor.Bounds());
        const Point miniClick{editor.Bounds().right - 20, editor.Bounds().top + 20};
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, miniClick)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, miniClick)));
        assert(editor.PanOffset().x != panBefore.x || editor.PanOffset().y != panBefore.y);
    }

    // ---------------------------------------------------------------- 引脚命中放大 / 拉线吸附 / 双击标题重命名
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        editor.SetPanOffset({0, 0});
        editor.SetZoom(0.25);
        DuiNodeGraph& graph = editor.Graph();
        const DuiNodeId first = graph.AddNode("First", {100, 100});
        const DuiPinId firstOut = graph.AddPin(first, "out", DuiPinKind::Output);
        const DuiNodeId second = graph.AddNode("Second", {500, 100});
        const DuiPinId secondIn = graph.AddPin(second, "in", DuiPinKind::Input);
        editor.NotifyGraphChanged();

        const DuiGraphNode* firstEntry = editor.Graph().FindNode(first);
        const Point pinCenter = editor.CanvasToScreen(
            {firstEntry->position.x + firstEntry->size.width,
             firstEntry->position.y + editor.Style().headerHeight
                 + editor.Style().pinRowHeight / 2});
        // 0.25 缩放下视觉圆点约 2～3px；8px 外侧原先点不中，放大命中后应仍是该引脚
        const Point slop{pinCenter.x + 8, pinCenter.y};
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, slop)));
        assert(editor.HoveredPin() == firstOut);

        editor.SetZoom(1.0);
        editor.SetPanOffset({0, 0});
        editor.NotifyGraphChanged();
        const DuiGraphNode* secondEntry = editor.Graph().FindNode(second);
        const Point outPoint = editor.CanvasToScreen(
            {editor.Graph().FindNode(first)->position.x + editor.Graph().FindNode(first)->size.width,
             editor.Graph().FindNode(first)->position.y + editor.Style().headerHeight
                 + editor.Style().pinRowHeight / 2});
        const Point inCenter = editor.CanvasToScreen(
            {secondEntry->position.x,
             secondEntry->position.y + editor.Style().headerHeight
                 + editor.Style().pinRowHeight / 2});
        // 18px 在精确命中区外、吸附半径内
        const Point magnet{inCenter.x - 18, inCenter.y};
        assert(editor.Graph().Links().empty());
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, outPoint)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, magnet)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, magnet)));
        assert(editor.Graph().Links().size() == 1);
        assert(editor.Graph().ConnectedPin(firstOut) == secondIn);
        (void)first;

        TextInputMock headerInput;
        editor.SetTextInput(&headerInput);
        editor.SelectNode(first);
        const Point header = editor.CanvasToScreen(
            {editor.Graph().FindNode(first)->position.x + editor.Graph().FindNode(first)->size.width / 2,
             editor.Graph().FindNode(first)->position.y + 8});
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDoubleClick, header)));
        assert(headerInput.visible);
        headerInput.text = "FirstX";
        headerInput.Submit();
        assert(editor.Graph().FindNode(first)->title == "FirstX");
    }

    // ---------------------------------------------------------------- 结构增量撤销 / IME 取消
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        const DuiNodeId a = editor.Graph().AddNode("A", {10, 10});
        const DuiNodeId b = editor.Graph().AddNode("B", {240, 10});
        const DuiNodeId c = editor.Graph().AddNode("C", {470, 10});
        const DuiPinId aOut = editor.Graph().AddPin(a, "out", DuiPinKind::Output);
        const DuiPinId bIn = editor.Graph().AddPin(b, "in", DuiPinKind::Input);
        const DuiPinId cIn = editor.Graph().AddPin(c, "in", DuiPinKind::Input);
        const DuiLinkId firstLink = editor.Graph().AddLink(aOut, bIn);
        editor.NotifyGraphChanged();
        assert(firstLink.Valid());

        editor.SelectNode(b);
        assert(editor.DeleteSelection());
        assert(editor.Graph().FindNode(b) == nullptr);
        assert(editor.Graph().FindPin(bIn) == nullptr);
        assert(editor.Undo());
        assert(editor.Graph().FindNode(b) != nullptr);
        assert(editor.Graph().FindNode(b)->title == "B");
        assert(editor.Graph().FindPin(bIn) != nullptr);
        assert(editor.Graph().FindLink(firstLink) != nullptr);
        assert(editor.Graph().ConnectedPin(aOut) == bIn);

        editor.NotifyGraphChanged();
        editor.SetPanOffset({0, 0});
        editor.SetZoom(1.0);
        const Point aOutScreen = editor.CanvasToScreen(
            {editor.Graph().FindNode(a)->position.x + editor.Graph().FindNode(a)->size.width,
             editor.Graph().FindNode(a)->position.y + editor.Style().headerHeight
                 + editor.Style().pinRowHeight / 2});
        const Point cInScreen = editor.CanvasToScreen(
            {editor.Graph().FindNode(c)->position.x,
             editor.Graph().FindNode(c)->position.y + editor.Style().headerHeight
                 + editor.Style().pinRowHeight / 2});
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, aOutScreen)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, cInScreen)));
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, cInScreen)));
        assert(editor.Graph().ConnectedPin(aOut) == cIn);
        assert(editor.Graph().FindLink(firstLink) == nullptr);
        assert(editor.Undo());
        assert(editor.Graph().ConnectedPin(aOut) == bIn);
        assert(editor.Graph().FindLink(firstLink) != nullptr);

        const std::uint64_t originalA = a.value;
        editor.SelectNode(a);
        editor.SelectNode(c, true);
        assert(editor.CopySelection());
        assert(editor.PasteClipboard({10, 200}));
        assert(editor.Graph().Nodes().size() == 5);
        assert(editor.Undo());
        assert(editor.Graph().Nodes().size() == 3);
        assert(editor.Graph().FindNode(DuiNodeId{originalA}) != nullptr);

        TextInputMock ime;
        editor.SetTextInput(&ime);
        editor.SelectNode(a);
        assert(editor.BeginRenameSelected());
        ime.text = "Nope";
        ime.Cancel();
        assert(editor.Graph().FindNode(a)->title == "A");
        (void)bIn;
    }

    // ---------------------------------------------------------------- 拉线预亮 / Ctrl+D / 连线包围盒命中
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        editor.SetPanOffset({0, 0});
        editor.SetZoom(1.0);
        const DuiNodeId a = editor.Graph().AddNode("A", {40, 40});
        const DuiNodeId b = editor.Graph().AddNode("B", {320, 40});
        const DuiNodeId c = editor.Graph().AddNode("C", {600, 40});
        const DuiPinId aOut = editor.Graph().AddPin(a, "out", DuiPinKind::Output, false, "float");
        const DuiPinId bIn = editor.Graph().AddPin(b, "in", DuiPinKind::Input, false, "float");
        const DuiPinId cIn = editor.Graph().AddPin(c, "in", DuiPinKind::Input, false, "int");
        editor.NotifyGraphChanged();

        RecordingCanvas idle;
        editor.Paint(idle, editor.Bounds());
        const auto countSlot = [](const RecordingCanvas& canvas, Color color)
        {
            std::size_t count{};
            for (const auto& ellipse : canvas.ellipses)
            {
                if (ellipse.color == color)
                    ++count;
            }
            return count;
        };
        const Color hover = editor.Theme().Get(ThemeSlot::BrandHover);
        const std::size_t idleHover = countSlot(idle, hover);

        const Point outPoint = editor.CanvasToScreen(
            {editor.Graph().FindNode(a)->position.x + editor.Graph().FindNode(a)->size.width,
             editor.Graph().FindNode(a)->position.y + editor.Style().headerHeight
                 + editor.Style().pinRowHeight / 2});
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, outPoint)));
        RecordingCanvas linking;
        editor.Paint(linking, editor.Bounds());
        // float 输出只能预亮 float 输入：BrandHover 至少多一处（bIn）
        assert(countSlot(linking, hover) > idleHover);
        (void)editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp,
            editor.CanvasToScreen({10, 10})));

        // 连线命中：端点连线中点应选中；远离贝塞尔包络的点不应命中
        assert(editor.Graph().AddLink(aOut, bIn).Valid());
        editor.NotifyGraphChanged();
        const Point start = editor.CanvasToScreen(
            {editor.Graph().FindNode(a)->position.x + editor.Graph().FindNode(a)->size.width,
             editor.Graph().FindNode(a)->position.y + editor.Style().headerHeight
                 + editor.Style().pinRowHeight / 2});
        const Point end = editor.CanvasToScreen(
            {editor.Graph().FindNode(b)->position.x,
             editor.Graph().FindNode(b)->position.y + editor.Style().headerHeight
                 + editor.Style().pinRowHeight / 2});
        const Point mid{(start.x + end.x) / 2, (start.y + end.y) / 2};
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, mid)));
        assert(editor.SelectedLinks().size() == 1);
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, mid)));
        const Point farAway{mid.x, mid.y + 200};
        editor.ClearSelection();
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, farAway)));
        assert(editor.SelectedLinks().empty());

        // Ctrl+D：副本偏移一格，原稿不动，可撤销
        editor.ClearHistory();
        editor.SelectNode(a);
        const Point origin = editor.Graph().FindNode(a)->position;
        const std::size_t before = editor.Graph().Nodes().size();
        assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, 'D', modifier::Control)));
        assert(editor.Graph().Nodes().size() == before + 1);
        assert(editor.Graph().FindNode(a)->position == origin);
        DuiNodeId copy{};
        for (const auto& node : editor.Graph().Nodes())
        {
            if (node.id != a && node.id != b && node.id != c && node.title == "A")
                copy = node.id;
        }
        assert(copy.Valid());
        assert(editor.Graph().FindNode(copy)->position.x == origin.x + editor.Style().gridSize);
        assert(editor.Graph().FindNode(copy)->position.y == origin.y + editor.Style().gridSize);
        assert(editor.DuplicateSelection()); // API 路径
        assert(editor.Graph().Nodes().size() == before + 2);
        assert(editor.Undo());
        assert(editor.Undo());
        assert(editor.Graph().FindNode(copy) == nullptr);
        (void)bIn;
        (void)cIn;
    }

    // ---------------------------------------------------------------- 正交折线 / 注释框 / 节点搜索
    {
        RecordingCanvas measurer;
        DuiNodeEditor editor;
        editor.SetTextMeasurer(&measurer);
        editor.SetBounds({0, 0, 800, 600});
        editor.SetPanOffset({0, 0});
        editor.SetZoom(1.0);

        const DuiNodeId a = editor.Graph().AddNode("AlphaGate", {40, 40});
        const DuiNodeId b = editor.Graph().AddNode("Beta", {360, 160});
        const DuiPinId aOut = editor.Graph().AddPin(a, "out", DuiPinKind::Output);
        const DuiPinId bIn = editor.Graph().AddPin(b, "in", DuiPinKind::Input);
        assert(editor.Graph().AddLink(aOut, bIn).Valid());
        const DuiNodeId note = editor.Graph().AddComment("Note me", {40, 220, 220, 320});
        assert(!editor.Graph().AddPin(note, "x", DuiPinKind::Input).Valid());
        editor.NotifyGraphChanged();

        // 搜索：子串、忽略大小写、聚焦循环
        const auto hits = editor.FindNodesByTitle("gate");
        assert(hits.size() == 1 && hits.front() == a);
        assert(editor.FindAndFocus("beta"));
        assert(editor.IsNodeSelected(b));
        assert(editor.FindAndFocus({})); // 复用上次词，仍只有 Beta
        assert(editor.IsNodeSelected(b));
        assert(editor.FindAndFocus("Note"));
        assert(editor.IsNodeSelected(note));

        // 正交折线：命中中段水平/竖直线；Paint 走 StrokePath
        {
            DuiNodeEditorStyle style = editor.Style();
            style.linkRouting = DuiLinkRouting::Orthogonal;
            editor.SetStyle(style);
            editor.ClearSelection();
            const Point start = editor.CanvasToScreen(
                {editor.Graph().FindNode(a)->position.x + editor.Graph().FindNode(a)->size.width,
                 editor.Graph().FindNode(a)->position.y + editor.Style().headerHeight
                     + editor.Style().pinRowHeight / 2});
            const Point end = editor.CanvasToScreen(
                {editor.Graph().FindNode(b)->position.x,
                 editor.Graph().FindNode(b)->position.y + editor.Style().headerHeight
                     + editor.Style().pinRowHeight / 2});
            const Point elbow{(start.x + end.x) / 2, start.y};
            assert(editor.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, elbow)));
            assert(editor.SelectedLinks().size() == 1);
            RecordingCanvas ortho;
            editor.Paint(ortho, editor.Bounds());
            assert(!ortho.strokedPaths.empty());
        }

        // 注释：XML 往返保留 kind/尺寸；可改名
        const std::string xml = editor.ToXml();
        assert(xml.find("kind=\"comment\"") != std::string::npos);
        DuiNodeEditor loaded;
        loaded.SetTextMeasurer(&measurer);
        loaded.SetBounds({0, 0, 800, 600});
        assert(loaded.FromXml(xml));
        bool foundComment{};
        for (const auto& node : loaded.Graph().Nodes())
        {
            if (node.kind == DuiNodeKind::Comment && node.title == "Note me")
            {
                foundComment = true;
                assert(node.size.width == 180 && node.size.height == 100);
            }
        }
        assert(foundComment);
        (void)aOut;
        (void)bIn;
    }

    return 0;
}
