/**
 * 文件名：DuiCanvas.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明绝对定位容器——子控件按给定坐标与尺寸摆放，不参与流式布局。
 */
#pragma once

#include <memory>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::layout {

/** 绝对定位容器中的一项：坐标相对容器左上角。 */
struct DuiCanvasItem final {
    int left{};
    int top{};
    /** 宽度；<= 0 表示取子控件 DesiredSize().width。 */
    int width{};
    /** 高度；<= 0 表示取子控件 DesiredSize().height。 */
    int height{};
};

/**
 * 绝对定位容器。
 * 用于浮层、角标、自绘叠加等需要精确坐标的场景。
 * 绘制与命中顺序均为子控件的插入顺序（先加入者在下层），不提供 ZIndex——
 * 避免排序表与命中测试不一致导致的"看得见点不到"。
 */
class DuiCanvas final : public core::Control, public render::DuiRenderable {
public:
    DuiCanvas();
    ~DuiCanvas() override;
    DuiCanvas(const DuiCanvas&) = delete;
    DuiCanvas& operator=(const DuiCanvas&) = delete;
    DuiCanvas(DuiCanvas&&) noexcept;
    DuiCanvas& operator=(DuiCanvas&&) noexcept;

    /** 追加子控件并指定位置；宽度或高度留空（0）时取该控件的 DesiredSize。 */
    void AddChild(std::unique_ptr<core::Control> child, DuiCanvasItem item = {});
    /** 更新已有子控件的位置与尺寸；子控件不存在时追加。 */
    void SetItem(core::Control* child, DuiCanvasItem item);
    /** @return 子控件的位置与尺寸；不存在时返回默认值。 */
    [[nodiscard]] DuiCanvasItem GetItem(core::Control* child) const;
    /** 仅移动子控件，保留原有尺寸设置。 */
    void SetPosition(core::Control* child, int left, int top);

    void Layout(core::Rect bounds);
    /** @return 覆盖全部子控件所需的最小尺寸（含各自坐标与尺寸）。 */
    [[nodiscard]] core::Size DesiredSize() const override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> canvas_;
};

} // namespace ysDui::controls::layout
