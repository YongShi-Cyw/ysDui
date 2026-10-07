/**
 * 文件名：DuiImageToolTip.cpp
 * 开发者：青蓝
 * 开发时间：2026-09-26
 * 用途：实现图片预览提示条控件
 */
#include "ysDui/controls/feedback/DuiImageToolTip.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "ysDui/controls/basic/DuiGroupBox.hpp"
#include "ysDui/controls/media/DuiImage.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/ui/DuiLayeredHost.hpp"

namespace ysDui::controls::feedback {
namespace {

/** 预览贴边保留的最小边距（像素）。 */
constexpr int EdgeMargin = 8;

/**
 * 将图片等比缩放到上限之内（只缩小、不放大）
 * @param imageSize 原始尺寸
 * @param maxSize 上限尺寸；任一分量非正表示该方向不设限
 * @return 显示尺寸
 */
core::Size FitIntoLimit(core::Size imageSize, core::Size maxSize)
{
    if (imageSize.Empty())
        return {};

    const int limitWidth = maxSize.width > 0 ? maxSize.width : imageSize.width;
    const int limitHeight = maxSize.height > 0 ? maxSize.height : imageSize.height;
    if (imageSize.width <= limitWidth && imageSize.height <= limitHeight)
        return imageSize;

    const double scale = (std::min)(static_cast<double>(limitWidth) / imageSize.width,
                                    static_cast<double>(limitHeight) / imageSize.height);
    const int width = static_cast<int>(std::lround(imageSize.width * scale));
    const int height = static_cast<int>(std::lround(imageSize.height * scale));
    return {width > 0 ? width : 1, height > 0 ? height : 1};
}

/**
 * 在锚点附近摆放内容矩形：优先锚点右下，越界翻转到左上，仍越界则贴齐客户区
 * @param anchor 锚点（相对 owner 客户区）
 * @param size 内容尺寸
 * @param offset 锚点偏移
 * @param ownerClient owner 客户区；为空矩形时不做翻转与贴边
 * @return 内容矩形
 */
core::Rect PlaceNearAnchor(core::Point anchor, core::Size size, core::Point offset,
                           core::Rect ownerClient)
{
    int left = anchor.x + offset.x;
    int top = anchor.y + offset.y;
    if (ownerClient.Empty())
        return {left, top, left + size.width, top + size.height};

    if (left + size.width > ownerClient.right - EdgeMargin)
        left = anchor.x - offset.x - size.width; // 右侧放不下：翻到锚点左侧
    if (top + size.height > ownerClient.bottom - EdgeMargin)
        top = anchor.y - offset.y - size.height; // 下方放不下：翻到锚点上方

    const int maxLeft = ownerClient.right - EdgeMargin - size.width;
    const int maxTop = ownerClient.bottom - EdgeMargin - size.height;
    left = (std::max)(ownerClient.left + EdgeMargin, (std::min)(left, maxLeft));
    top = (std::max)(ownerClient.top + EdgeMargin, (std::min)(top, maxTop));
    return {left, top, left + size.width, top + size.height};
}

/**
 * 提示条内容：白色底板 + 分组框（标题为该图片名称），图片置于分组框内容区。
 * 说明：分组框负责绘制边框、标题与内容区图片，本类只补一层白底，
 *       因 GroupBox 不填充主体背景，缺了白底会露出分层窗默认底色。
 */
class ImagePreviewContent final : public core::Control, public render::DuiRenderable
{
public:
    ImagePreviewContent(std::shared_ptr<const render::DuiImage> image, std::string title, int margin,
                        int padding, core::Color background, core::Color border)
        : margin_(margin), background_(background), border_(border)
    {
        auto groupBox = std::make_unique<basic::DuiGroupBox>();
        groupBox->SetTitle(std::move(title));
        groupBox->SetBorderColor(border);
        groupBox->SetTitleBackgroundColor(background); // 标题底与底板同色，避免割裂
        groupBox->SetPadding({padding, padding, padding, padding});

        auto picture = std::make_unique<media::DuiImage>();
        picture->SetImage(std::move(image));
        picture->SetScaleMode(media::DuiImageScaleMode::Stretch); // 外层已按比例算好尺寸
        groupBox->SetContent(std::move(picture));

        groupBox_ = groupBox.get();
        AddChild(std::move(groupBox));
    }

    /**
     * 按图片显示尺寸计算整体尺寸（含组控件留白、标题条与内边距）
     * @param imageSize 图片显示尺寸
     * @return 提示条整体尺寸
     */
    [[nodiscard]] core::Size PreferredSizeFor(core::Size imageSize) const
    {
        if (groupBox_ == nullptr)
            return imageSize;
        const basic::DuiGroupBoxPadding padding = groupBox_->Padding();
        return {imageSize.width + padding.left + padding.right + MarginLeft() + MarginRight(),
                imageSize.height + groupBox_->TitleStripHeight() + padding.top + padding.bottom
                    + MarginTop() + MarginBottom()};
    }

    /** 设置整体矩形，留白后交由分组框给图片定位 */
    void Layout(core::Rect bounds)
    {
        SetBounds(bounds);
        if (groupBox_ != nullptr)
            groupBox_->Layout(GroupBoxRect(bounds));
    }

    void Paint(render::Canvas& canvas, core::Rect dirty) const override
    {
        const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
        if (bounds.Empty())
            return;
        canvas.FillRect(bounds, background_); // 白色底板
        // 弹窗边框与组控件边框同色，避免外沿与组框两种灰不一致
        canvas.StrokeRoundedRect(bounds, 0, border_, 1.0F);
        if (groupBox_ != nullptr)
            groupBox_->Paint(canvas, dirty);
    }

private:
    /** @return 组控件矩形＝整体矩形内缩留白（四边可见留白统一等于 margin） */
    [[nodiscard]] core::Rect GroupBoxRect(core::Rect outer) const
    {
        return {outer.left + MarginLeft(), outer.top + MarginTop(), outer.right - MarginRight(),
                outer.bottom - MarginBottom()};
    }

    [[nodiscard]] int MarginLeft() const { return margin_; }
    [[nodiscard]] int MarginRight() const { return margin_; }
    [[nodiscard]] int MarginBottom() const { return margin_; }
    /**
     * @return 顶部内缩量
     * 说明：DuiGroupBox 把顶边框画在标题条中线（titleStripHeight/2）处，
     *       若顶部也内缩 margin，可见顶边框会比左右、底部多出半个标题条。
     *       故这里减去该偏移，使四边「可见」留白一致。
     */
    [[nodiscard]] int MarginTop() const
    {
        if (groupBox_ == nullptr)
            return margin_;
        return (std::max)(0, margin_ - groupBox_->TitleStripHeight() / 2);
    }

    basic::DuiGroupBox* groupBox_{};
    int margin_{};
    core::Color background_{};
    core::Color border_{};
};

} // namespace

class DuiImageToolTip::Impl
{
public:
    ui::ILayeredHost* host{};
    core::Size maxSize{DefaultMaxSize};
    core::Point anchorOffset{DefaultAnchorOffset};
    int margin{DefaultMargin};
    int padding{DefaultPadding};
    core::Color background{255, 255, 255, 255};
    core::Color border{200, 200, 208, 255}; // 与 DuiGroupBox 默认边框同色，弹窗边框与组框保持一致
    std::string currentPath; // 正在显示的图片标识，用于复用宿主
    bool showing{};
};

DuiImageToolTip::DuiImageToolTip() : toolTip_(std::make_unique<Impl>()) {}

DuiImageToolTip::~DuiImageToolTip()
{
    SetLayeredHost(nullptr);
}

DuiImageToolTip::DuiImageToolTip(DuiImageToolTip&&) noexcept = default;
DuiImageToolTip& DuiImageToolTip::operator=(DuiImageToolTip&&) noexcept = default;

void DuiImageToolTip::SetLayeredHost(ui::ILayeredHost* host)
{
    if (toolTip_->host == host)
        return;

    HideNow();
    if (toolTip_->host != nullptr)
        toolTip_->host->SetDismissedHandler({});
    toolTip_->host = host;
    if (host == nullptr)
        return;

    host->SetDismissedHandler([this] {
        toolTip_->showing = false;
        toolTip_->currentPath.clear();
    });
}

void DuiImageToolTip::SetMaxSize(core::Size maxSize)
{
    toolTip_->maxSize = maxSize;
}

void DuiImageToolTip::SetPadding(int padding)
{
    toolTip_->padding = (std::max)(0, padding);
}

void DuiImageToolTip::SetMargin(int margin)
{
    toolTip_->margin = (std::max)(0, margin);
}

void DuiImageToolTip::SetBackgroundColor(core::Color color)
{
    toolTip_->background = color;
}

void DuiImageToolTip::SetBorderColor(core::Color color)
{
    toolTip_->border = color;
}

void DuiImageToolTip::SetAnchorOffset(core::Point offset)
{
    toolTip_->anchorOffset = offset;
}

bool DuiImageToolTip::Show(std::shared_ptr<const render::DuiImage> image, std::string title,
                          core::Point anchor, core::Rect ownerClient)
{
    if (toolTip_->host == nullptr || !image || image->Empty())
        return false;

    const core::Size display = FitIntoLimit(image->Size(), toolTip_->maxSize);
    if (display.Empty())
        return false;

    const std::string key = title; // 标题即预览文件名，用作复用标识
    auto content = std::make_unique<ImagePreviewContent>(std::move(image), std::move(title),
                                                        toolTip_->margin, toolTip_->padding,
                                                        toolTip_->background, toolTip_->border);
    ImagePreviewContent* raw = content.get();
    const core::Rect bounds =
        PlaceNearAnchor(anchor, raw->PreferredSizeFor(display), toolTip_->anchorOffset, ownerClient);

    // 同一张图重复悬停：只移动已显示窗口，避免反复重建分层窗造成闪烁
    if (toolTip_->showing && toolTip_->currentPath == key && toolTip_->host->Visible())
    {
        toolTip_->host->SetBounds(bounds);
        return true;
    }

    ui::DuiLayeredOptions options;
    options.bounds = bounds;
    options.dismissOnFocusLost = false;
    options.mouseTransparent = true;      // 提示条不拦截鼠标，避免遮挡被悬停的树控件
    options.repaintOnPointerMove = false; // 内容静态，指针移动无需重绘

    raw->Layout(bounds);
    toolTip_->showing = toolTip_->host->Show(
        options, std::move(content), [raw](core::Rect layoutBounds) { raw->Layout(layoutBounds); },
        [raw](render::Canvas& canvas, core::Rect dirty) { raw->Paint(canvas, dirty); });
    if (toolTip_->showing)
        toolTip_->currentPath = key;
    return toolTip_->showing;
}

void DuiImageToolTip::HideNow()
{
    if (toolTip_->host != nullptr && toolTip_->showing)
        toolTip_->host->RequestHide();
    toolTip_->showing = false;
    toolTip_->currentPath.clear();
}

bool DuiImageToolTip::Showing() const
{
    return toolTip_->showing;
}

} // namespace ysDui::controls::feedback
