/**
 * 文件名：DuiImageViewer.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：声明图片查看器：按视口贴合显示图片，支持缩放、平移与关闭请求。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::render {
class DuiImage;
}

namespace ysDui::controls::media {

/** 图像的贴合方式。 */
enum class DuiImageViewerFit
{
    Contain, // 完整可见：等比缩放到视口内，可能缩小也可能放大（等价 CSS object-fit: contain）。
    Actual,  // 原始尺寸：1 图像像素 = 1 DIP，超出部分裁切。
    Cover,   // 铺满视口：等比缩放到覆盖视口，超出部分裁切。
};

/**
 * 图片查看器（灯箱）。
 *
 * 用途：在对话或图库中放大查看一张图片。把本控件放在根控件树的**最后**并铺满整个宿主，
 * 即为常见"灯箱"效果；放在一个局部矩形里则是一个内嵌的图片预览框。
 *
 * 交互：
 * - 滚轮以上一指针位置为锚缩放（锚点下的像素保持不动）；
 * - 主键拖拽平移；双击在"贴合视图"与"原始尺寸"之间切换；
 * - 点击图片之外的背板请求关闭（可用 `SetCloseOnBackdropClick` 关闭该行为）；
 * - Escape 请求关闭（可用 `SetCloseOnEscape` 关闭）。Escape 依赖焦点，点击查看器即可获得焦点。
 *
 * 关闭语义：本控件**不会隐藏自己**，只调用 `SetCloseRequestedHandler` 注册的回调，
 * 由调用方决定隐藏、销毁还是什么都不做——避免控件在回调栈里销毁自身。
 *
 * 坐标系：内部以**图像像素**为画布单位（1 像素 = 1 画布单位），
 * 缩放在绘制时由 `render::DuiCanvasTransform` 施加，因此缩放不会重新采样多次。
 */
class DuiImageViewer final : public core::Control, public render::DuiRenderable {
public:
    DuiImageViewer();
    ~DuiImageViewer() override;
    DuiImageViewer(const DuiImageViewer&) = delete;
    DuiImageViewer& operator=(const DuiImageViewer&) = delete;
    // 持有背板/文字等状态，移动收益低；与其它复杂控件保持一致地禁止移动
    DuiImageViewer(DuiImageViewer&&) = delete;
    DuiImageViewer& operator=(DuiImageViewer&&) = delete;

    /**
     * 设置图片并复位视图（按当前视口重新贴合并居中）。
     * @param image 图片；空指针表示无图
     */
    void SetImage(std::shared_ptr<const render::DuiImage> image);
    /** @return 当前图片 */
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& Image() const;

    /**
     * 设置贴合方式并复位视图。
     * @param fit 贴合方式
     */
    void SetFit(DuiImageViewerFit fit);
    /** @return 当前贴合方式 */
    [[nodiscard]] DuiImageViewerFit Fit() const;

    /**
     * 设置缩放系数（以视口中心为锚），并标记视图已手动调整。
     * @param zoom 缩放系数，自动夹取到 `SetZoomLimits` 设定的范围
     */
    void SetZoom(double zoom);
    /** @return 当前缩放系数（1.0 表示原始尺寸） */
    [[nodiscard]] double Zoom() const;
    /**
     * 设置缩放范围。
     * @param minimum 下限；至少为 0.01
     * @param maximum 上限；至少为 minimum
     */
    void SetZoomLimits(double minimum, double maximum);
    /** @return 缩放下限 */
    [[nodiscard]] double MinimumZoom() const;
    /** @return 缩放上限 */
    [[nodiscard]] double MaximumZoom() const;
    /**
     * 以指定屏幕点为锚缩放：该点下方的图像像素保持不动。
     * @param anchor 屏幕坐标锚点
     * @param factor 缩放倍数
     */
    void ZoomAt(core::Point anchor, double factor);

    /** 复位视图：回到初始贴合并居中，清除"已手动调整"标记。 */
    void ResetView();
    /** @return 是否已偏离初始贴合视图（被缩放或平移过） */
    [[nodiscard]] bool ViewAdjusted() const;

    /**
     * 设置平移量（图像左上角在屏幕上的位置）。
     * @param offset 屏幕坐标；会被夹取到"图像不会被完全拖离视口"的范围
     */
    void SetOffset(core::Point offset);
    /** @return 当前平移量 */
    [[nodiscard]] core::Point Offset() const;

    /** 设置背板颜色（通常带透明度）；默认取主题 `ScrollViewBackground` 加深。 */
    void SetBackdropColor(core::Color color);
    /** @return 背板颜色 */
    [[nodiscard]] core::Color BackdropColor() const;
    /** 设置是否允许点击背板请求关闭（图片区域内点击不触发）。默认开启。 */
    void SetCloseOnBackdropClick(bool enabled);
    /** @return 是否允许点击背板请求关闭 */
    [[nodiscard]] bool CloseOnBackdropClick() const;
    /** 设置是否允许 Escape 请求关闭。默认开启。 */
    void SetCloseOnEscape(bool enabled);
    /** @return 是否允许 Escape 请求关闭 */
    [[nodiscard]] bool CloseOnEscape() const;
    /**
     * 设置说明文字，绘制在底部。
     * @param text UTF-8 文本；空串表示不绘制
     */
    void SetCaptionText(std::string text);
    /** @return 说明文字 */
    [[nodiscard]] const std::string& CaptionText() const;
    /**
     * 设置关闭请求回调。
     * 本控件不会自行隐藏：由回调决定隐藏/销毁/忽略。
     * @param handler 回调；空回调表示忽略关闭请求
     */
    void SetCloseRequestedHandler(std::function<void()> handler);

    // ---- 几何（可在无画布环境下单测） ----

    /**
     * 计算贴合缩放系数。
     * @param content 图像尺寸（像素）
     * @param viewport 视口矩形
     * @param fit 贴合方式
     * @return 缩放系数；尺寸非法时返回 1.0
     */
    [[nodiscard]] static double FitScale(core::Size content, core::Rect viewport, DuiImageViewerFit fit);
    /** @return 图像当前的屏幕矩形；无图或未布局时为空 */
    [[nodiscard]] core::Rect ImageRect() const;
    /**
     * 屏幕坐标 → 图像像素坐标。
     * @param screen 屏幕坐标
     * @return 图像坐标（可越界，调用方自行判断）
     */
    [[nodiscard]] core::Point ScreenToImage(core::Point screen) const;

    void Layout(core::Rect bounds) override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;

private:
    /** 依据贴合方式重算缩放并居中。 */
    void ApplyFit();
    /** 把平移量夹取到"图像不会被完全拖离视口"的范围（某轴装得下时居中）。 */
    void ClampOffset();
    /** 触发关闭请求。 */
    void RequestClose();
    /** @return 视口矩形（等于 Bounds）。 */
    [[nodiscard]] core::Rect Viewport() const;
    /** @return 当前图像尺寸；无图时为 {0, 0}。 */
    [[nodiscard]] core::Size ImageSize() const;

    class Impl;
    std::unique_ptr<Impl> viewer_;
};

} // namespace ysDui::controls::media
