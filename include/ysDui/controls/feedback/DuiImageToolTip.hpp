/**
 * 文件名：DuiImageToolTip.hpp
 * 开发者：青蓝
 * 开发时间：2026-09-26
 * 用途：图片预览提示条声明，在分层宿主中按锚点显示白底分组框包裹的预览图
 */
#pragma once

#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiImage.hpp"

namespace ysDui::ui {
class ILayeredHost;
}

namespace ysDui::controls::feedback {

/**
 * 图片预览提示条
 * 用途：在锚点（通常为光标）附近显示一张图片，如零件预览图；
 *       外观为白色底板 + 分组框（GroupBox）标题显示名称信息，图片位于分组框内容区。
 * 说明：宿主由调用方创建并注入（对齐 DuiToolTipManager::SetPopupHost 的做法）；
 *       本类只接受「相对 owner 客户区」的坐标，屏幕坐标与窗口句柄的换算由调用方负责，
 *       以免平台细节回流到控件层。
 */
class DuiImageToolTip final
{
public:
    /** 图片最大显示尺寸（像素）；图片更大时等比缩小，永不放大。 */
    static constexpr core::Size DefaultMaxSize{360, 360};
    /** 组控件与显示窗口边缘的留白（像素）：四边统一生效。 */
    static constexpr int DefaultMargin = 10;
    /** 组控件边框与内部图片之间的内边距（像素）。 */
    static constexpr int DefaultPadding = 8;
    /** 锚点默认偏移（像素），让预览避开光标。 */
    static constexpr core::Point DefaultAnchorOffset{12, 18};

    DuiImageToolTip();
    ~DuiImageToolTip();
    DuiImageToolTip(const DuiImageToolTip&) = delete;
    DuiImageToolTip& operator=(const DuiImageToolTip&) = delete;
    DuiImageToolTip(DuiImageToolTip&&) noexcept;
    DuiImageToolTip& operator=(DuiImageToolTip&&) noexcept;

    /**
     * 设置承载用的分层宿主；传入 nullptr 时先隐藏再解绑。
     * 说明：本类会接管宿主的 dismissed 回调以维持 Showing()，调用方不应再占用该回调。
     * @param host 宿主；生命周期由调用方保证，须长于本对象的使用期
     */
    void SetLayeredHost(ui::ILayeredHost* host);

    /** 设置图片最大显示尺寸（像素）；任一分量非正时该方向不设限。 */
    void SetMaxSize(core::Size maxSize);
    /** 设置组控件与显示窗口边缘的留白（像素）；负值按 0 处理，四边统一。 */
    void SetMargin(int margin);
    /** 设置组控件边框与内部图片之间的内边距（像素）；负值按 0 处理。 */
    void SetPadding(int padding);
    /** 设置底板与分组框标题底色（默认白色）；标题底与底板同色以避免割裂。 */
    void SetBackgroundColor(core::Color color);
    /** 设置分组框边框色。 */
    void SetBorderColor(core::Color color);
    /** 设置锚点偏移（像素）。 */
    void SetAnchorOffset(core::Point offset);

    /**
     * 在锚点附近显示图片（越界时先翻转方向，再贴齐 owner 客户区）
     * @param image 图像；为空或空图时不显示
     * @param title 分组框标题（名称信息，通常为预览文件名）；为空时只画边框
     * @param anchor 锚点（相对 owner 客户区，通常为光标位置）
     * @param ownerClient owner 客户区矩形，用于翻转与贴边；为空矩形时不做贴边处理
     * @return 宿主接受显示返回 true
     */
    bool Show(std::shared_ptr<const render::DuiImage> image, std::string title, core::Point anchor,
              core::Rect ownerClient);

    /** 请求隐藏（异步，可在鼠标钩子等受限上下文中调用）。 */
    void HideNow();
    /** @return 当前是否正在显示。 */
    [[nodiscard]] bool Showing() const;

private:
    class Impl;
    std::unique_ptr<Impl> toolTip_;
};

} // namespace ysDui::controls::feedback
