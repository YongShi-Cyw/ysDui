/**
 * 文件名：dock_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：验证停靠布局模型的树操作、不变式与布局存取。
 */
#include "test_support.hpp"

#include <cmath>
#include <string>

namespace {
/** 比例比较：避免浮点误差导致误判。 */
bool Near(double value, double expected)
{
    return std::abs(value - expected) < 1e-6;
}
} // namespace

int main()
{
    using ysDui::controls::docking::DuiDockNodeKind;
    using ysDui::controls::docking::DuiDockOrientation;
    using ysDui::controls::docking::DuiDockSlot;
    using ysDui::controls::docking::DuiDockTree;

    // 空树
    {
        DuiDockTree tree;
        assert(tree.Empty());
        assert(tree.PaneCount() == 0);
        assert(tree.Root().kind == DuiDockNodeKind::Group);
        assert(tree.ActivePane().empty());
        assert(!tree.Contains("editor"));
        assert(!tree.RemovePane("editor"));
        assert(!tree.SetActivePane("editor"));
        assert(tree.PaneIds().empty());
    }

    // 空 id 与重复 id 必须拒绝
    {
        DuiDockTree tree;
        assert(!tree.AddPane("", "Empty"));
        assert(tree.AddPane("editor", "Editor"));
        assert(!tree.AddPane("editor", "Duplicate"));
        assert(tree.PaneCount() == 1);
        assert(tree.PaneTitle("editor") == "Editor");
        assert(tree.PaneTitle("missing").empty());
    }

    // 槽位切分：左右栏切分锚点、上下栏切分整棵树；同槽位窗格并入同组
    {
        DuiDockTree tree;
        assert(tree.AddPane("editor", "Editor"));
        // 第一个窗格占据整块区域，不产生分割
        assert(tree.Root().kind == DuiDockNodeKind::Group);
        assert(!tree.Empty());

        assert(tree.AddPane("outline", "Outline", DuiDockSlot::Left));
        assert(tree.Root().kind == DuiDockNodeKind::Split);
        assert(tree.Root().orientation == DuiDockOrientation::Vertical);
        assert(Near(tree.Root().fraction, 0.25));
        assert(tree.Root().children.size() == 2);
        assert(tree.Root().children[0]->panes.size() == 1);
        assert(tree.Root().children[0]->panes[0].id == "outline");
        assert(tree.Root().children[1]->panes[0].id == "editor");

        // 同槽位的第二个窗格并入左侧组，不新建分割
        assert(tree.AddPane("search", "Search", DuiDockSlot::Left));
        assert(tree.Root().children[0]->panes.size() == 2);
        assert(tree.Root().children[1]->panes.size() == 1);

        // 底部栏切分整棵树：原来的竖直分割成为第一个子节点
        assert(tree.AddPane("output", "Output", DuiDockSlot::Bottom));
        assert(tree.Root().kind == DuiDockNodeKind::Split);
        assert(tree.Root().orientation == DuiDockOrientation::Horizontal);
        assert(Near(tree.Root().fraction, 1.0 - 0.28));
        assert(tree.Root().children[0]->kind == DuiDockNodeKind::Split);
        assert(tree.Root().children[0]->orientation == DuiDockOrientation::Vertical);
        assert(tree.Root().children[1]->panes[0].id == "output");

        // 右侧栏切分中央组
        assert(tree.AddPane("toolbox", "Toolbox", DuiDockSlot::Right));
        const ysDui::controls::docking::DuiDockNode* editorGroup = tree.FindGroup("editor");
        assert(editorGroup != nullptr);
        assert(editorGroup->panes.size() == 1);
        assert(tree.PaneCount() == 5);
        assert(tree.PaneIds().size() == 5);
        assert(tree.FindGroup("toolbox") != editorGroup);
    }

    // 移除窗格：空组回收 + 单子分割折叠
    {
        DuiDockTree tree;
        assert(tree.AddPane("editor", "Editor"));
        assert(tree.AddPane("outline", "Outline", DuiDockSlot::Left));
        assert(tree.AddPane("output", "Output", DuiDockSlot::Bottom));
        assert(tree.Root().kind == DuiDockNodeKind::Split);

        assert(tree.RemovePane("editor"));
        // editor 组变空被回收，竖直分割只剩 outline 一侧 → 折叠
        assert(tree.PaneCount() == 2);
        assert(tree.Root().kind == DuiDockNodeKind::Split);
        assert(tree.Root().orientation == DuiDockOrientation::Horizontal);
        assert(tree.Root().children[0]->id == tree.FindGroup("outline")->id);

        assert(tree.RemovePane("outline"));
        // 水平分割也只剩 output 一侧 → 折叠，根回到单个组
        assert(tree.Root().kind == DuiDockNodeKind::Group);
        assert(tree.Root().panes.size() == 1);
        assert(tree.Root().panes[0].id == "output");

        assert(tree.RemovePane("output"));
        assert(tree.Empty());
        assert(tree.PaneCount() == 0);
        assert(tree.Root().kind == DuiDockNodeKind::Group);
        assert(!tree.RemovePane("output"));
    }

    // 嵌套分割：AddPaneBeside 三层
    {
        DuiDockTree tree;
        assert(tree.AddPane("a", "A"));
        assert(tree.AddPaneBeside("a", "b", "B", DuiDockSlot::Bottom, 0.3));
        assert(tree.Root().kind == DuiDockNodeKind::Split);
        assert(tree.Root().orientation == DuiDockOrientation::Horizontal);
        assert(Near(tree.Root().fraction, 0.3));

        assert(tree.AddPaneBeside("b", "c", "C", DuiDockSlot::Right, 0.4));
        assert(tree.Root().children[0]->kind == DuiDockNodeKind::Group);
        assert(tree.Root().children[1]->kind == DuiDockNodeKind::Split);
        assert(tree.Root().children[1]->orientation == DuiDockOrientation::Vertical);
        assert(Near(tree.Root().children[1]->fraction, 0.4));
        assert(tree.Root().children[1]->children[0]->panes[0].id == "b");
        assert(tree.Root().children[1]->children[1]->panes[0].id == "c");
        assert(tree.PaneCount() == 3);

        // Center 表示并入参照组
        assert(tree.AddPaneBeside("a", "d", "D", DuiDockSlot::Center));
        assert(tree.FindGroup("a")->panes.size() == 2);

        // 参照窗格不存在 / 标识重复
        assert(!tree.AddPaneBeside("missing", "e", "E", DuiDockSlot::Right));
        assert(!tree.AddPaneBeside("a", "c", "C", DuiDockSlot::Right));
        // 比例写入后会被钳制到可见范围
        assert(tree.SetSplitFraction(tree.Root().id, 5.0));
        assert(Near(tree.Root().fraction, 0.95));
        assert(tree.SetSplitFraction(tree.Root().id, -3.0));
        assert(Near(tree.Root().fraction, 0.05));
        // 组不是分割
        assert(!tree.SetSplitFraction(tree.FindGroup("a")->id, 0.5));
        assert(!tree.SetSplitFraction(999999, 0.5));
    }

    // 移动窗格
    {
        DuiDockTree tree;
        assert(tree.AddPane("editor", "Editor"));
        assert(tree.AddPane("outline", "Outline", DuiDockSlot::Left));
        assert(tree.AddPane("output", "Output", DuiDockSlot::Bottom));

        // 并入目标组：底部组变空被回收，水平分割折叠
        assert(tree.MovePane("output", "editor", DuiDockSlot::Center));
        assert(tree.PaneCount() == 3);
        assert(tree.FindGroup("output") == tree.FindGroup("editor"));
        assert(tree.Root().kind == DuiDockNodeKind::Split);
        assert(tree.Root().orientation == DuiDockOrientation::Vertical);

        // 相对目标切出新组
        assert(tree.MovePane("outline", "editor", DuiDockSlot::Bottom, 0.35));
        assert(tree.PaneCount() == 3);
        assert(tree.FindGroup("outline") != tree.FindGroup("editor"));
        assert(tree.Root().kind == DuiDockNodeKind::Split);
        assert(tree.Root().orientation == DuiDockOrientation::Horizontal);
        assert(Near(tree.Root().fraction, 0.35));
        assert(tree.Root().children[0]->panes.size() == 2);

        // 无效参数
        assert(!tree.MovePane("editor", "editor"));
        assert(!tree.MovePane("editor", "missing"));
        assert(!tree.MovePane("missing", "editor"));
    }

    // 活动窗格与组内选中
    {
        DuiDockTree tree;
        assert(tree.AddPane("editor", "Editor"));
        assert(tree.AddPane("output", "Output", DuiDockSlot::Center));
        // 同槽位并入同一组：editor、output 同组，活动项为最后加入者
        assert(tree.FindGroup("editor") == tree.FindGroup("output"));
        assert(tree.ActivePane() == "output");

        assert(tree.SetActivePane("editor"));
        assert(tree.ActivePane() == "editor");
        assert(tree.FindGroup("editor")->active == 0);

        assert(tree.SetActivePane("output"));
        assert(tree.ActivePane() == "output");
        assert(tree.FindGroup("output")->active == 1);
        assert(!tree.SetActivePane("missing"));

        const std::uint64_t groupId = tree.FindGroup("editor")->id;
        assert(tree.SetGroupActive(groupId, 99));
        assert(tree.FindGroup("editor")->active == 1);
        assert(!tree.SetGroupActive(999999, 0));

        // 重排：选中项跟随被移动的标签（output 由下标 1 变为 0）
        assert(tree.ReorderPane(groupId, 0, 1));
        assert(tree.FindGroup("editor")->panes[0].id == "output");
        assert(tree.FindGroup("editor")->panes[1].id == "editor");
        assert(tree.FindGroup("editor")->active == 0);
        assert(tree.ActivePane() == "output");
        assert(!tree.ReorderPane(groupId, 9, 0));

        // 标题修改
        assert(tree.SetPaneTitle("editor", "Editor 2"));
        assert(tree.PaneTitle("editor") == "Editor 2");
        assert(!tree.SetPaneTitle("missing", "X"));
    }

    // 布局存取：往返一致
    {
        DuiDockTree tree;
        assert(tree.AddPane("editor", "Editor"));
        assert(tree.AddPane("outline", "Outline", DuiDockSlot::Left));
        assert(tree.AddPane("output", "Output", DuiDockSlot::Bottom));
        assert(tree.AddPane("problems", "Problems", DuiDockSlot::Bottom));
        assert(tree.SetActivePane("editor"));

        std::string text;
        assert(tree.SaveLayout(text));
        assert(!text.empty());
        assert(text.find("dock-layout") != std::string::npos);
        assert(text.find("editor") != std::string::npos);
        // 布局只写 id：标题不进文件
        assert(text.find("Editor") == std::string::npos);

        DuiDockTree loaded;
        assert(loaded.AddPane("editor", "Editor"));
        assert(loaded.AddPane("outline", "Outline"));
        assert(loaded.AddPane("output", "Output"));
        assert(loaded.AddPane("problems", "Problems"));
        assert(loaded.LoadLayout(text));
        assert(loaded.PaneCount() == 4);
        assert(loaded.Root().kind == DuiDockNodeKind::Split);
        assert(Near(loaded.Root().fraction, tree.Root().fraction));
        assert(loaded.FindGroup("editor") != nullptr);
        assert(loaded.FindGroup("outline") != nullptr);
        assert(loaded.FindGroup("output")->panes.size() == 2);

        std::string again;
        assert(loaded.SaveLayout(again));
        assert(again == text);

        // 载入后仍可继续追加窗格
        assert(loaded.AddPane("toolbox", "Toolbox", DuiDockSlot::Right));
        assert(loaded.PaneCount() == 5);
    }

    // 布局存取：未知 id 忽略、未出现的窗格补进第一个组
    {
        DuiDockTree tree;
        assert(tree.AddPane("a", "A"));
        assert(tree.AddPane("b", "B"));
        assert(tree.AddPane("c", "C"));
        const std::string layout =
            "<dock-layout version=\"1\">"
            "<split orientation=\"vertical\" fraction=\"0.4\">"
            "<group active=\"0\"><pane id=\"a\"/></group>"
            "<group active=\"0\"><pane id=\"ghost\"/><pane id=\"b\"/></group>"
            "</split>"
            "</dock-layout>";
        assert(tree.LoadLayout(layout));
        assert(tree.PaneCount() == 3);
        assert(!tree.Contains("ghost"));
        assert(tree.FindGroup("a")->panes.size() == 2);
        assert(tree.FindGroup("b")->panes.size() == 1);
        assert(Near(tree.Root().fraction, 0.4));
    }

    // 布局存取：损坏输入不破坏现有布局
    {
        DuiDockTree tree;
        assert(tree.AddPane("a", "A"));
        assert(tree.AddPane("b", "B", DuiDockSlot::Left));
        std::string before;
        assert(tree.SaveLayout(before));

        assert(!tree.LoadLayout(""));
        assert(!tree.LoadLayout("not xml <<<"));
        assert(!tree.LoadLayout("<other/>"));
        assert(!tree.LoadLayout("<dock-layout><group/></dock-layout>"));   // 空组经整理后无内容
        assert(!tree.LoadLayout("<dock-layout><split/></dock-layout>"));   // 分割子节点不足
        assert(!tree.LoadLayout("<dock-layout><split orientation=\"vertical\">"
                                "<group active=\"0\"><pane id=\"a\"/></group>"
                                "</split></dock-layout>"));

        std::string after;
        assert(tree.SaveLayout(after));
        assert(before == after);
        assert(tree.PaneCount() == 2);
    }

    // 自动隐藏：收起后不占布局空间，展开回到中央组
    {
        DuiDockTree tree;
        assert(tree.AddPane("editor", "Editor"));
        assert(tree.AddPane("output", "Output", DuiDockSlot::Bottom));
        assert(tree.PaneCount() == 2);
        assert(!tree.IsPaneAutoHidden("output"));
        assert(tree.SetPaneAutoHide("output", true, DuiDockSlot::Right));
        assert(tree.IsPaneAutoHidden("output"));
        // 收起后仍算窗格，但不在树内；空组回收 + 分割折叠
        assert(tree.PaneCount() == 2);
        assert(tree.Contains("output"));
        assert(tree.PaneTitle("output") == "Output");
        assert(tree.AutoHideItems().size() == 1);
        assert(tree.AutoHideItems().front().slot == DuiDockSlot::Right);
        assert(tree.Root().kind == DuiDockNodeKind::Group);
        assert(tree.Root().panes.size() == 1);
        assert(tree.ActivePane() == "editor");
        // 展开回到中央组
        assert(tree.SetPaneAutoHide("output", false));
        assert(!tree.IsPaneAutoHidden("output"));
        assert(tree.Contains("output"));
        assert(tree.PaneCount() == 2);
        assert(tree.Root().kind == DuiDockNodeKind::Group);
        assert(tree.FindGroup("editor") == tree.FindGroup("output"));
        // 重复收起/不存在的窗格
        assert(!tree.SetPaneAutoHide("missing", true));
        assert(!tree.SetPaneAutoHide("editor", false));
        // 收起状态下也可以移除
        assert(tree.SetPaneAutoHide("editor", true, DuiDockSlot::Top));
        assert(tree.RemovePane("editor"));
        assert(!tree.Contains("editor"));
        assert(tree.PaneCount() == 1);

        // 自动隐藏随布局持久化
        std::string layout;
        assert(tree.SetPaneAutoHide("output", true, DuiDockSlot::Left));
        assert(tree.SaveLayout(layout));
        assert(layout.find("auto-hide") != std::string::npos);
        DuiDockTree loaded;
        assert(loaded.AddPane("output", "Output"));
        assert(loaded.AddPane("extra", "Extra"));
        assert(loaded.LoadLayout(layout));
        assert(loaded.IsPaneAutoHidden("output"));
        assert(loaded.AutoHideItems().front().slot == DuiDockSlot::Left);
        assert(loaded.PaneTitle("output") == "Output");
        // 布局里未出现的窗格仍会被补回树内
        assert(loaded.Contains("extra") && !loaded.IsPaneAutoHidden("extra"));
        std::string again;
        assert(loaded.SaveLayout(again));
        assert(again.find("auto-hide") != std::string::npos);
        assert(again.find("output") != std::string::npos);
        assert(again.find("extra") != std::string::npos);
    }

    // 拖放落点：并入、四向切分与拖出（模型层不经视图，直接校验不变式）
    {
        DuiDockTree tree;
        assert(tree.AddPane("a", "A"));
        assert(tree.AddPane("b", "B"));
        // 并入后同组
        assert(tree.MovePane("b", "a", DuiDockSlot::Center));
        assert(tree.FindGroup("a") == tree.FindGroup("b"));
        // 相对 a 在右侧切出 b
        assert(tree.MovePane("b", "a", DuiDockSlot::Right, 0.5));
        assert(tree.FindGroup("a") != tree.FindGroup("b"));
        assert(tree.Root().orientation == DuiDockOrientation::Vertical);
        // 相对 a 在上方切出 c：AddPaneBeside 以参照组为锚点，不改变根的方向
        assert(tree.AddPaneBeside("a", "c", "C", DuiDockSlot::Top, 0.5));
        assert(tree.Root().orientation == DuiDockOrientation::Vertical);
        assert(tree.Root().children[0]->orientation == DuiDockOrientation::Horizontal);
        assert(tree.Root().children[0]->children[0]->panes[0].id == "c");
        assert(tree.Root().children[0]->children[1]->panes[0].id == "a");
        assert(tree.PaneCount() == 3);
    }

    return 0;
}
