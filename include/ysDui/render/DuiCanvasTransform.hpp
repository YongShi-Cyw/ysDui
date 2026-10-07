/**
 * 文件名：DuiCanvasTransform.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：声明画布二维相似变换（缩放 + 平移），供 Canvas 变换栈与节点编辑器命中共用。
 */
#pragma once

#include <algorithm>
#include <cmath>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiPath.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::render {

/**
 * 画布坐标到设备 DIP 的相似变换：`screen = offset + canvas * scale`。
 * 各边独立舍入，避免通过宽高累加产生漂移。
 */
struct DuiCanvasTransform final
{
    double scale{1.0};    // 缩放系数，1.0 表示 1 画布单位 = 1 DIP
    core::Point offset{}; // 画布原点对应的屏幕坐标

    [[nodiscard]] bool Identity() const
    {
        return scale == 1.0 && offset.x == 0 && offset.y == 0;
    }

    [[nodiscard]] core::Point MapPoint(core::Point canvas) const
    {
        return {offset.x + static_cast<int>(std::lround(canvas.x * scale)),
                offset.y + static_cast<int>(std::lround(canvas.y * scale))};
    }

    [[nodiscard]] core::Point UnmapPoint(core::Point screen) const
    {
        if (scale <= 0.0)
            return screen;
        return {static_cast<int>(std::lround((screen.x - offset.x) / scale)),
                static_cast<int>(std::lround((screen.y - offset.y) / scale))};
    }

    [[nodiscard]] core::Rect MapRect(core::Rect canvas) const
    {
        const core::Point topLeft = MapPoint({canvas.left, canvas.top});
        const core::Point bottomRight = MapPoint({canvas.right, canvas.bottom});
        return {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    }

    /** 长度换算：用于圆角、引脚半径、连线粗细等标量。 */
    [[nodiscard]] int MapLength(int canvasLength) const
    {
        return static_cast<int>(std::lround(static_cast<double>(canvasLength) * scale));
    }

    [[nodiscard]] float MapStroke(float width) const
    {
        return static_cast<float>(static_cast<double>(width) * scale);
    }

    /**
     * 按缩放调整字号，使文字与几何同步放大/缩小。
     * 测量文本时不要走这条路径：布局应在未变换的画布单位下进行。
     */
    [[nodiscard]] DuiTextStyle MapText(const DuiTextStyle& style) const
    {
        if (Identity())
            return style;
        DuiTextStyle scaled = style;
        const int base = (std::max)(1, style.pointSize);
        scaled.pointSize = (std::clamp)(static_cast<int>(std::lround(base * scale)), 1, 96);
        return scaled;
    }

    [[nodiscard]] DuiPath MapPath(const DuiPath& path) const
    {
        if (Identity())
            return path;
        DuiPath mapped;
        for (const DuiPathCommand& command : path.Commands())
        {
            if (command.type == DuiPathCommandType::Close)
            {
                mapped.Close();
                continue;
            }
            const core::Point point = MapPoint(command.point);
            if (command.type == DuiPathCommandType::MoveTo)
                mapped.MoveTo(point);
            else
                mapped.LineTo(point);
        }
        return mapped;
    }

    /** 先应用 local，再应用 parent（与嵌套 PushTransform 一致）。 */
    [[nodiscard]] static DuiCanvasTransform Compose(const DuiCanvasTransform& parent,
                                                    const DuiCanvasTransform& local)
    {
        DuiCanvasTransform result;
        result.scale = parent.scale * local.scale;
        result.offset = parent.MapPoint(local.offset);
        return result;
    }
};

} // namespace ysDui::render
