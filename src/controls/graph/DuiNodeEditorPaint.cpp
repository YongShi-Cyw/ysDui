#include "ysDui/controls/graph/DuiNodeEditor.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "DuiNodeEditorInternal.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiPath.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::graph {
using namespace detail;
namespace {

/** 节点/引脚/连线的取色：显式配色优先，否则回退到主题槽。 */
core::Color NodeFill(const core::DuiTheme& theme, const DuiGraphNode& node)
{
    // 节点底色用「交替表面色」而非纯背景色：白底白框在浅色主题下几乎不可辨
    return node.hasColor ? node.color : theme.Get(core::ThemeSlot::SurfaceAlternateBackground);
}
core::Color NodeBorder(const core::DuiTheme& theme)
{
    return theme.Get(core::ThemeSlot::BorderHeavy);
}
core::Color PinColor(const core::DuiTheme& theme, const DuiGraphPin& pin, bool highlighted,
                     bool linkCompatible)
{
    if (pin.hasColor)
        return pin.color;
    if (highlighted)
        return theme.Get(core::ThemeSlot::StatusAway);
    // 拉线时预亮可连接目标，与默认输出色区分开
    if (linkCompatible)
        return theme.Get(core::ThemeSlot::BrandHover);
    return pin.kind == DuiPinKind::Input ? theme.Get(core::ThemeSlot::BrandPrimary)
                                         : theme.Get(core::ThemeSlot::StatusOnline);
}

/**
 * 绘制两级网格：细格线 + 每 5 条一根的粗格线。
 * 按画布格索引 i 投影到屏幕（round(offset + i * spacing)），粗/细共用同一套位置，
 * 避免「minorStep 与 majorStep 各自 lround」在非整数缩放下出现两条线贴在一起。
 */
void PaintGrid(render::Canvas& canvas, const core::DuiTheme& theme, core::Rect bounds,
               const GraphTransform& transform, int gridSize)
{
    const core::Color base = theme.Get(core::ThemeSlot::GridLine);
    core::Color minor = base;
    minor.alpha = static_cast<unsigned char>(base.alpha > 16 ? base.alpha / 2 : base.alpha);
    core::Color major = base;
    major.alpha = base.alpha;

    const double spacing = static_cast<double>(gridSize) * transform.scale;
    if (spacing < 1.0)
        return;
    // 过密时省略细格线（只留粗格线），避免糊成一片
    const bool drawMinor = spacing >= 6.0;
    constexpr int kMajorEvery = 5; // 每 5 条细格一根粗格

    const auto drawAxis = [&](bool vertical)
    {
        const int viewMin = vertical ? bounds.left : bounds.top;
        const int viewMax = vertical ? bounds.right : bounds.bottom;
        const double origin = vertical ? static_cast<double>(transform.offset.x)
                                       : static_cast<double>(transform.offset.y);
        const int firstIndex = static_cast<int>(std::floor((viewMin - origin) / spacing)) - 1;
        const int lastIndex = static_cast<int>(std::ceil((viewMax - origin) / spacing)) + 1;
        for (int index = firstIndex; index <= lastIndex; ++index)
        {
            const int screen = static_cast<int>(std::lround(origin + index * spacing));
            if (screen < viewMin || screen > viewMax)
                continue;
            const bool isMajor = index % kMajorEvery == 0;
            if (!isMajor && !drawMinor)
                continue;
            if (vertical)
                canvas.FillRect({screen, bounds.top, screen + 1, bounds.bottom},
                                isMajor ? major : minor);
            else
                canvas.FillRect({bounds.left, screen, bounds.right, screen + 1},
                                isMajor ? major : minor);
        }
    };
    drawAxis(true);
    drawAxis(false);
}

/** 按比例混合两色；用于把标题栏底部两角补成直角时匹配该处的渐变颜色。 */
core::Color BlendColor(core::Color from, core::Color to, double amount)
{
    const auto mix = [amount](unsigned char a, unsigned char b)
    {
        const double value = static_cast<double>(a) * (1.0 - amount)
            + static_cast<double>(b) * amount;
        return static_cast<unsigned char>((std::clamp)(value, 0.0, 255.0) + 0.5);
    };
    return {mix(from.red, to.red), mix(from.green, to.green), mix(from.blue, to.blue), 255};
}

} // namespace

void DuiNodeEditor::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty())
        return;
    // 自动适配：在用户手动导航前，每次绘制都按当前内容与尺寸重新适配，
    // 这样"先建控件、后填模型"与"先填模型、后布局"两种时序都能得到正确视图。
    if (editor_->autoFit && !Bounds().Empty())
        const_cast<DuiNodeEditor*>(this)->FrameAll();

    const core::DuiTheme& theme = Theme();
    const GraphTransform& transform = editor_->transform;
    const double scale = transform.scale;
    const render::DuiTextStyle textStyle = editor_->textStyle;
    canvas.FillRect(bounds, theme.Get(core::ThemeSlot::SurfaceBackground));

    // 自绘内容一律裁剪到控件矩形，避免节点/连线溢出到相邻控件上
    canvas.PushClip(bounds);
    if (editor_->gridVisible)
        PaintGrid(canvas, theme, bounds, transform, editor_->style.gridSize);

    canvas.PushTransform(transform.ToCanvasTransform());

    // 分组/注释在最底层：分组半透明收纳，注释为便签色块
    for (const auto& node : editor_->graph.Nodes())
    {
        if (node.kind != DuiNodeKind::Group && node.kind != DuiNodeKind::Comment)
            continue;
        const NodeLayout* layout = editor_->LayoutOf(node.id);
        if (layout == nullptr)
            continue;
        if (core::Rect::Intersect(transform.ToScreenRect(layout->bounds), dirty).Empty())
            continue;
        const core::Rect rect = layout->bounds;
        const int radius = (std::max)(0, editor_->style.nodeRounding);
        const bool selected = editor_->IsNodeSelected(node.id);
        if (node.kind == DuiNodeKind::Comment)
        {
            core::Color fill = node.hasColor ? node.color
                                             : theme.Get(core::ThemeSlot::MessageWarningFill);
            fill.alpha = selected ? 230 : 200;
            canvas.FillRoundedRect(rect, radius, fill);
            canvas.StrokeRoundedRect(rect, radius,
                                     selected ? theme.Get(core::ThemeSlot::StatusAway)
                                              : theme.Get(core::ThemeSlot::BorderHeavy),
                                     selected ? 2.0F : 1.0F);
            if (!layout->header.Empty())
            {
                render::DuiTextStyle commentStyle = textStyle;
                commentStyle.bold = true;
                commentStyle.color = theme.Get(core::ThemeSlot::MessageWarningText);
                const std::string titleText = editor_->renamingNode == node.id
                    && editor_->renameInput == nullptr
                    ? editor_->renameBuffer + "|"
                    : (editor_->renamingNode == node.id ? std::string{} : node.title);
                if (!titleText.empty())
                    canvas.DrawText(titleText, layout->header, commentStyle,
                                    render::DuiTextAlignment::Start, true);
            }
            continue;
        }
        core::Color fill = NodeFill(theme, node);
        fill.alpha = 40; // 半透明，避免遮挡内部节点
        canvas.FillRoundedRect(rect, radius, fill);
        core::Color groupBorder = theme.Get(core::ThemeSlot::BorderHeavy);
        groupBorder.alpha = 140;
        canvas.StrokeRoundedRect(rect, radius, groupBorder, 1.0F);
        if (!node.title.empty())
        {
            const core::Rect title = layout->header;
            if (!title.Empty())
            {
                const int chipWidth = 12 + canvas.MeasureText(node.title, textStyle, {}).size.width;
                core::Color chip = theme.Get(core::ThemeSlot::SurfaceAlternateBackground);
                chip.alpha = 235;
                canvas.FillRoundedRect({title.left, title.top,
                                        (std::min)(title.right, title.left + chipWidth), title.bottom},
                                       3, chip);
                canvas.DrawText(node.title, title, textStyle, render::DuiTextAlignment::Start, false);
            }
        }
    }

    // 连线在节点之下绘制，使节点边界遮住线头
    const auto paintLink = [&](core::Point start, core::Point end, core::Color color, int thickness)
    {
        if (editor_->style.linkRouting == DuiLinkRouting::Orthogonal)
        {
            const std::vector<core::Point> points = OrthogonalPolyline(start, end);
            if (points.size() < 2)
                return;
            render::DuiPath path;
            path.MoveTo(points.front());
            for (std::size_t index = 1; index < points.size(); ++index)
                path.LineTo(points[index]);
            canvas.StrokePath(path, color, static_cast<float>(thickness));
            return;
        }
        const double handle = LinkHandleLength(start, end, editor_->style.linkStrength);
        canvas.StrokeCubicBezier(start,
                                 {start.x + static_cast<int>(std::lround(handle)), start.y},
                                 {end.x - static_cast<int>(std::lround(handle)), end.y},
                                 end, color, static_cast<float>(thickness));
    };

    for (const auto& link : editor_->graph.Links())
    {
        const bool selected = editor_->IsLinkSelected(link.id);
        const bool hovered = editor_->hovered.link == link.id;
        // 悬停引脚或节点时高亮其全部连线（参考项目所称的 automatic highlight），
        // 便于在密集图中一眼看清某节点与谁相连
        bool connected{};
        if (editor_->hovered.pin.Valid())
            connected = link.start == editor_->hovered.pin || link.end == editor_->hovered.pin;
        if (!connected && editor_->hovered.node.Valid())
        {
            const DuiGraphPin* startPin = editor_->graph.FindPin(link.start);
            const DuiGraphPin* endPin = editor_->graph.FindPin(link.end);
            connected = (startPin != nullptr && startPin->node == editor_->hovered.node)
                || (endPin != nullptr && endPin->node == editor_->hovered.node);
        }
        core::Color color = link.hasColor ? link.color
            : selected ? theme.Get(core::ThemeSlot::StatusAway)
            : hovered ? theme.Get(core::ThemeSlot::BrandHover)
            : connected ? theme.Get(core::ThemeSlot::StatusAway)
                        : theme.Get(core::ThemeSlot::BrandPrimary);
        // 连线保底 2px：随缩放等比缩小会让它在 0.5 倍下细成发丝
        const int thickness = editor_->style.linkThickness
            + (selected ? 1 : 0) + (connected || hovered ? 1 : 0);
        const core::Point startCanvas = editor_->PinCenter(link.start);
        const core::Point endCanvas = editor_->PinCenter(link.end);
        paintLink(startCanvas, endCanvas, color, thickness);

        if (!LinkFlow(link.id))
            continue;
        const std::vector<core::Point> points =
            BuildLinkPolyline(startCanvas, endCanvas, editor_->style);
        if (points.size() < 2)
            continue;
        const int spacing = (std::max)(8, editor_->style.flowSpacing);
        const int radius = (std::max)(2, editor_->style.flowRadius);
        // 累计弧长，按固定间距取点；起点用相位偏移，使标记随时间移动
        double travelled{};
        double next = editor_->flowPhase * spacing;
        for (std::size_t index = 1; index < points.size(); ++index)
        {
            const double dx = static_cast<double>(points[index].x - points[index - 1].x);
            const double dy = static_cast<double>(points[index].y - points[index - 1].y);
            const double length = std::hypot(dx, dy);
            if (length <= 0.0)
                continue;
            while (next <= travelled + length)
            {
                const double ratio = (next - travelled) / length;
                const core::Point marker{
                    points[index - 1].x + static_cast<int>(std::lround(dx * ratio)),
                    points[index - 1].y + static_cast<int>(std::lround(dy * ratio))};
                core::Color markerColor = theme.Get(core::ThemeSlot::TextOnPrimary);
                markerColor.alpha = 210;
                canvas.FillEllipse({marker.x - radius, marker.y - radius,
                                    marker.x + radius + 1, marker.y + radius + 1}, markerColor);
                next += spacing;
            }
            travelled += length;
        }
    }

    // 单节点绘制抽成 lambda，以便分两遍调用（未选中 → 选中/悬停），
    // 让选中节点压在其余节点之上，而不必改动模型中的节点顺序。
    const auto paintNode = [&](const DuiGraphNode& node)
    {
        if (node.kind != DuiNodeKind::Normal)
            return;
        const NodeLayout* layout = editor_->LayoutOf(node.id);
        if (layout == nullptr)
            return;
        if (core::Rect::Intersect(transform.ToScreenRect(layout->bounds), dirty).Empty())
            return;
        const core::Rect rect = layout->bounds;
        const bool selected = editor_->IsNodeSelected(node.id);
        const bool hovered = editor_->hovered.node == node.id;
        const int radius = (std::max)(0, editor_->style.nodeRounding);

        const int shadowOffset = 2;
        const core::Rect shadow{rect.left + shadowOffset, rect.top + shadowOffset,
                                rect.right + shadowOffset, rect.bottom + shadowOffset};
        {
            core::Color shadowColor{0, 0, 0, 255};
            shadowColor.alpha = static_cast<unsigned char>(selected ? 60 : 34);
            canvas.FillRoundedRect(shadow, radius, shadowColor);
        }
        canvas.FillRoundedRect(rect, radius, NodeFill(theme, node));

        const core::Rect header = layout->header;
        if (!header.Empty())
        {
            const core::Color top = theme.Get(core::ThemeSlot::BrandHover);
            const core::Color bottom = theme.Get(core::ThemeSlot::BrandPressed);
            canvas.FillLinearGradient(header, radius, render::DuiLinearGradient{top, bottom, true});
            // 标题栏底部两角改为直角（节点本体仍是四角圆角）：渐变只能按矩形填充，故用
            // 同色阶的方角矩形把圆角缺口补齐。渐变竖直均匀，缺口顶端位于标题栏高度的
            // (1 - radius/height) 处，据此混出局部顶色再向下渐变，与已绘制的渐变无缝衔接
            // （直接填 bottom 会在角上留下一道色阶）。
            if (radius > 0 && header.Height() > 0)
            {
                const double ratio =
                    (std::clamp)(1.0 - static_cast<double>(radius) / header.Height(), 0.0, 1.0);
                const core::Color localTop = BlendColor(top, bottom, ratio);
                const core::Rect leftCorner{header.left, header.bottom - radius,
                                            header.left + radius, header.bottom};
                const core::Rect rightCorner{header.right - radius, header.bottom - radius,
                                             header.right, header.bottom};
                const render::DuiLinearGradient cornerGradient{localTop, bottom, true};
                canvas.FillLinearGradient(leftCorner, 0, cornerGradient);
                canvas.FillLinearGradient(rightCorner, 0, cornerGradient);
            }
        }
        const core::Color border = selected ? theme.Get(core::ThemeSlot::StatusAway)
            : hovered ? theme.Get(core::ThemeSlot::BrandHover)
                      : NodeBorder(theme);
        canvas.StrokeRoundedRect(rect, radius, border, selected || hovered ? 2.0F : 1.0F);
        const int padX = editor_->style.headerPaddingX;
        const int padY = editor_->style.headerPaddingY;
        const int iconSlot = (std::max)(0, editor_->style.headerIconSlot);
        const core::Rect titleRect{header.left + padX + iconSlot, header.top + padY,
                                   header.right - padX, header.bottom - padY};
        if (!titleRect.Empty())
        {
            render::DuiTextStyle titleStyle = textStyle;
            titleStyle.color = theme.Get(core::ThemeSlot::TextOnPrimary);
            titleStyle.bold = true;
            const bool renamingHere = editor_->renamingNode == node.id && editor_->renameInput != nullptr;
            const std::string titleText = renamingHere ? std::string{}
                : (editor_->renamingNode == node.id ? editor_->renameBuffer + "|" : node.title);
            if (!titleText.empty())
                canvas.DrawText(titleText, titleRect, titleStyle,
                                render::DuiTextAlignment::Start, false);
        }

        // 引脚圆点与标签
        for (const auto& pin : editor_->graph.Pins())
        {
            if (pin.node != node.id)
                continue;
            const bool isInput = pin.kind == DuiPinKind::Input;
            const auto& order = isInput ? layout->inputOrder : layout->outputOrder;
            const auto& rects = isInput ? layout->inputPins : layout->outputPins;
            const auto& labels = isInput ? layout->inputLabels : layout->outputLabels;
            for (std::size_t index = 0; index < order.size(); ++index)
            {
                if (order[index] != pin.id || index >= rects.size())
                    continue;
                const core::Rect pinRect = rects[index];
                if (core::Rect::Intersect(transform.ToScreenRect(pinRect), dirty).Empty())
                    continue;
                const bool highlight = editor_->hovered.pin == pin.id
                    || editor_->linkCandidate == pin.id;
                const bool compatible = editor_->interaction == Interaction::CreatingLink
                    && editor_->IsCompatibleLinkPin(pin.id);
                const core::Color pinFill = PinColor(theme, pin, highlight, compatible);
                // 引脚形状：圆/方/箭头；箭头按输入朝左、输出朝右绘制三角形
                const core::Point pinCenter{(pinRect.left + pinRect.right) / 2,
                                            (pinRect.top + pinRect.bottom) / 2};
                switch (editor_->style.pinShape)
                {
                case DuiPinShape::Square:
                    canvas.FillRoundedRect(pinRect, 0, pinFill);
                    break;
                case DuiPinShape::Arrow:
                {
                    const int half = (std::max)(2, (pinRect.Width()) / 2);
                    render::DuiPath arrow;
                    if (isInput)
                    {
                        arrow.MoveTo({pinCenter.x + half, pinCenter.y - half});
                        arrow.LineTo({pinCenter.x + half, pinCenter.y + half});
                        arrow.LineTo({pinCenter.x - half, pinCenter.y});
                    }
                    else
                    {
                        arrow.MoveTo({pinCenter.x - half, pinCenter.y - half});
                        arrow.LineTo({pinCenter.x - half, pinCenter.y + half});
                        arrow.LineTo({pinCenter.x + half, pinCenter.y});
                    }
                    arrow.Close();
                    canvas.FillPath(arrow, pinFill);
                    break;
                }
                case DuiPinShape::Circle:
                default:
                    canvas.FillEllipse(pinRect, pinFill);
                    canvas.StrokeRoundedRect(pinRect, editor_->style.pinRadius,
                                             pinFill, 1.0F);
                    break;
                }
                if (index < labels.size())
                {
                    // 引脚行若被内联控件占用，标签区已让位；缩放 ≠ 1 时控件不可用，
                    // 改用「引脚名 = 当前值」的文本占位，保持信息不丢失。
                    const auto& slots = isInput ? layout->inputWidgets : layout->outputWidgets;
                    const bool hasWidget = index < slots.size() && !slots[index].Empty();
                    const core::Control* widget = index < order.size()
                        ? editor_->PinContentOf(order[index]) : nullptr;
                    if (hasWidget && widget != nullptr && !editor_->EmbeddedVisible())
                    {
                        const core::DuiAccessibilityData data = widget->Accessibility();
                        const std::string text = data.value.empty()
                            ? pin.name : pin.name + ": " + data.value;
                        canvas.DrawText(text, labels[index],
                                        textStyle,
                                        isInput ? render::DuiTextAlignment::Start
                                                : render::DuiTextAlignment::End, false);
                    }
                    else
                    {
                        canvas.DrawText(pin.name, labels[index],
                                        textStyle,
                                        isInput ? render::DuiTextAlignment::Start
                                                : render::DuiTextAlignment::End, false);
                    }
                }
            }
        }
    };

    // 两遍绘制：第一遍画未选中节点，第二遍只画选中/悬停节点。
    // 每个节点的内嵌控件紧跟本体绘制并裁剪到节点矩形，避免盖住上层节点。
    const auto paintNodeWidgets = [&](DuiNodeId nodeId, const NodeLayout& layout)
    {
        if (!editor_->EmbeddedVisible())
            return;
        const core::Rect clip = core::Rect::Intersect(transform.ToScreenRect(layout.bounds), dirty);
        if (clip.Empty())
            return;
        canvas.PushClip(clip);
        const auto paintWidget = [&](core::Control* widget)
        {
            if (widget == nullptr || !widget->EffectivelyVisible())
                return;
            if (const auto* renderable = dynamic_cast<const render::DuiRenderable*>(widget))
                renderable->Paint(canvas, clip);
        };
        for (const auto& entry : editor_->pinContents)
        {
            const DuiGraphPin* pin = editor_->graph.FindPin(entry.first);
            if (pin != nullptr && pin->node == nodeId)
                paintWidget(entry.second);
        }
        for (const auto& entry : editor_->contents)
        {
            if (entry.first == nodeId)
                paintWidget(entry.second);
        }
        canvas.PopClip();
    };

    for (int pass = 0; pass < 2; ++pass)
    {
        for (const auto& node : editor_->graph.Nodes())
        {
            const bool focused = editor_->IsNodeSelected(node.id)
                || editor_->hovered.node == node.id;
            if ((pass == 0 && focused) || (pass == 1 && !focused))
                continue;
            paintNode(node);
            canvas.PopTransform();
            const NodeLayout* layout = editor_->LayoutOf(node.id);
            if (layout != nullptr)
                paintNodeWidgets(node.id, *layout);
            canvas.PushTransform(transform.ToCanvasTransform());
        }
    }
    canvas.PopTransform();

    // 拉线预览：从起点引脚到光标；落点合法时用成功色，否则用危险色
    if (editor_->interaction == Interaction::CreatingLink && editor_->linkFrom.Valid())
    {
        canvas.PushTransform(transform.ToCanvasTransform());
        const core::Point start = editor_->PinCenter(editor_->linkFrom);
        const core::Color color = editor_->linkCandidate.Valid()
            ? (editor_->linkCandidateValid ? theme.Get(core::ThemeSlot::StatusOnline)
                                           : theme.Get(core::ThemeSlot::Danger))
            : theme.Get(core::ThemeSlot::BrandPrimary);
        paintLink(start, editor_->linkCursorCanvas, color, editor_->style.linkThickness);
        canvas.PopTransform();
    }

    // 框选矩形：半透明填充 + 描边；对角拖拽时归一化 left/right、top/bottom
    if (editor_->interaction == Interaction::BoxSelecting)
    {
        const core::Rect raw = editor_->selectionBox;
        const core::Rect normalized{(std::min)(raw.left, raw.right), (std::min)(raw.top, raw.bottom),
                                    (std::max)(raw.left, raw.right), (std::max)(raw.top, raw.bottom)};
        const core::Rect box = core::Rect::Intersect(normalized, dirty);
        if (!box.Empty())
        {
            core::Color fill = theme.Get(core::ThemeSlot::BrandPrimary);
            fill.alpha = 48; // ~19% 不透明度，底下网格与节点仍应可见
            canvas.FillRect(box, fill);
            canvas.StrokeRoundedRect(box, 0, theme.Get(core::ThemeSlot::BrandPrimary), 1.0F);
        }
    }
    if (editor_->minimapVisible && !editor_->graph.Empty())
    {
        const core::Rect mini = editor_->MinimapRect();
        const core::Rect world = editor_->graph.ContentBounds();
        if (!mini.Empty() && !world.Empty())
        {
            core::Color back = theme.Get(core::ThemeSlot::SurfaceBackground);
            canvas.FillRect(mini, back);
            canvas.StrokeRoundedRect(mini, 0, theme.Get(core::ThemeSlot::BorderHeavy), 1.0F);
            const auto mapMini = [&](core::Point canvasPoint) -> core::Point
            {
                const double nx = world.Width() <= 0 ? 0.0
                    : static_cast<double>(canvasPoint.x - world.left) / world.Width();
                const double ny = world.Height() <= 0 ? 0.0
                    : static_cast<double>(canvasPoint.y - world.top) / world.Height();
                return {mini.left + static_cast<int>(std::lround(nx * mini.Width())),
                        mini.top + static_cast<int>(std::lround(ny * mini.Height()))};
            };
            for (const auto& node : editor_->graph.Nodes())
            {
                const core::Point a = mapMini(node.position);
                const core::Point b = mapMini({node.position.x + node.size.width,
                                               node.position.y + node.size.height});
                const core::Rect nodeRect{(std::min)(a.x, b.x), (std::min)(a.y, b.y),
                                          (std::max)(a.x, b.x), (std::max)(a.y, b.y)};
                if (nodeRect.Width() < 2 || nodeRect.Height() < 2)
                    continue;
                const core::Color fill = node.kind == DuiNodeKind::Group
                    ? theme.Get(core::ThemeSlot::BorderLight)
                    : node.kind == DuiNodeKind::Comment
                        ? theme.Get(core::ThemeSlot::StatusAway)
                        : BlendColor(theme.Get(core::ThemeSlot::SurfaceBackground),
                                     theme.Get(core::ThemeSlot::BrandPrimary), 0.45);
                canvas.FillRect(nodeRect, fill);
            }
            const core::Point viewA = mapMini(transform.ToCanvas({bounds.left, bounds.top}));
            const core::Point viewB = mapMini(transform.ToCanvas({bounds.right, bounds.bottom}));
            const core::Rect viewRect{(std::min)(viewA.x, viewB.x), (std::min)(viewA.y, viewB.y),
                                      (std::max)(viewA.x, viewB.x), (std::max)(viewA.y, viewB.y)};
            const core::Rect clipped = core::Rect::Intersect(viewRect, mini);
            if (!clipped.Empty())
                canvas.StrokeRoundedRect(clipped, 0, theme.Get(core::ThemeSlot::BrandPrimary), 1.0F);
        }
    }
    // 右下角缩放指示 + 空图提示：帮助使用者确认当前视图状态与可用操作
    {
        render::DuiTextStyle hintStyle = editor_->textStyle;
        hintStyle.pointSize = 9;
        hintStyle.color = theme.Get(core::ThemeSlot::TextSubtle);
        const std::string zoomText =
            "zoom " + std::to_string(static_cast<int>(std::lround(scale * 100.0))) + "%";
        const int zoomWidth = canvas.MeasureText(zoomText, hintStyle, {}).size.width;
        canvas.DrawText(zoomText,
                        {bounds.right - zoomWidth - 10, bounds.bottom - 18, bounds.right - 8, bounds.bottom - 2},
                        hintStyle, render::DuiTextAlignment::Start, false);
        if (editor_->graph.Empty())
        {
            const std::string emptyText =
                "Empty graph - wheel to zoom, middle/right drag to pan, F to fit";
            const int emptyWidth = canvas.MeasureText(emptyText, hintStyle, {}).size.width;
            const int left = bounds.left + (bounds.Width() - emptyWidth) / 2;
            canvas.DrawText(emptyText, {left, bounds.top + bounds.Height() / 2 - 9,
                                        left + emptyWidth, bounds.top + bounds.Height() / 2 + 9},
                            hintStyle, render::DuiTextAlignment::Start, false);
        }
    }
    canvas.PopClip();
    canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::BorderHeavy), 1.0F);
}

} // namespace ysDui::controls::graph
