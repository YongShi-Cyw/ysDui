/**
 * 文件名：DuiInspector.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的控件树调试覆盖层。
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiCanvas.hpp"

namespace ysDui::render {

/**
 * 控件树调试覆盖层。
 * 用途：使用标准画布命令标示可见控件的边界，并为悬停控件显示描述标签。
 */
class DuiInspector final
{
public:
    /** 创建处于关闭状态的调试覆盖层。 */
    DuiInspector();

    /** 释放调试覆盖层实现状态。 */
    ~DuiInspector();

    DuiInspector(const DuiInspector&) = delete;
    DuiInspector& operator=(const DuiInspector&) = delete;
    DuiInspector(DuiInspector&&) noexcept;
    DuiInspector& operator=(DuiInspector&&) noexcept;

    /**
     * 设置覆盖层是否参与绘制。
     * @param enabled 为 true 时绘制控件边界和悬停标签。
     */
    void SetEnabled(bool enabled);

    /** @return 覆盖层当前是否启用。 */
    [[nodiscard]] bool Enabled() const;

    /**
     * 收集根控件及其可见后代的边界。
     * @param root 要遍历的控件树根节点。
     * @param bounds 接收按树顺序排列的边界。
     */
    static void CollectVisibleRects(const core::Control& root, std::vector<core::Rect>& bounds);

    /**
     * 构造控件的调试标签文本。
     * @param control 要描述的控件。
     * @return 包含控件名称和边界的 UTF-16 文本。
     */
    [[nodiscard]] static std::string FormatControlInfo(const core::Control& control);

    /**
     * 绘制覆盖层。
     * @param root 要检查的控件树根节点。
     * @param canvas 接收平台无关绘制命令的画布。
     * @param dirty 本次需要更新的区域。
     */
    void PaintOverlay(const core::Control& root, Canvas& canvas, core::Rect dirty) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::render
