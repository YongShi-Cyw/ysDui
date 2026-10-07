/**
 * 文件名：DuiImageViewer.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：图片查看器的贴合、缩放、平移与绘制实现。
 */
#include "ysDui/controls/media/DuiImageViewer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiCanvasTransform.hpp"
#include "ysDui/render/DuiImage.hpp"

namespace ysDui::controls::media {
namespace {
/** 缩放下限：小于该值图像已无法辨认。 */
constexpr double kDefaultMinimumZoom = 0.05;
/** 缩放上限：再放大只会看到马赛克。 */
constexpr double kDefaultMaximumZoom = 16.0;
/** 每一格滚轮的缩放倍数（1.15^step）。 */
constexpr double kWheelZoomStep = 1.15;
/** 视口反算源矩形时向外扩张的画布单位，避免取整造成边缘缝隙。 */
constexpr int kSourceOverscan = 1;
/** 说明文字条的高度。 */
constexpr int kCaptionHeight = 26;
/** 说明文字的左右内边距。 */
constexpr int kCaptionPadding = 12;
/** 判定"已是贴合视图"的缩放容差，用于双击切换。 */
constexpr double kFitEqualTolerance = 1e-6;
} // namespace

class DuiImageViewer::Impl {
public:
    std::shared_ptr<const render::DuiImage> image;
    DuiImageViewerFit fit{DuiImageViewerFit::Contain};
    double scale{1.0};
    double minimumZoom{kDefaultMinimumZoom};
    double maximumZoom{kDefaultMaximumZoom};
    core::Point offset{};
    bool adjusted{};
    core::Color backdrop{16, 18, 24, 214};
    bool backdropOverride{};
    bool closeOnBackdrop{true};
    bool closeOnEscape{true};
    std::string caption;
    std::function<void()> closeRequested;
    bool panning{};
    core::Point panOrigin{};
    core::Point panStartOffset{};
};

DuiImageViewer::DuiImageViewer() : viewer_(std::make_unique<Impl>()) {}
DuiImageViewer::~DuiImageViewer() = default;

// ---- 图片与贴合 ----

void DuiImageViewer::SetImage(std::shared_ptr<const render::DuiImage> image)
{
    viewer_->image = std::move(image);
    ResetView();
}

const std::shared_ptr<const render::DuiImage>& DuiImageViewer::Image() const { return viewer_->image; }

void DuiImageViewer::SetFit(DuiImageViewerFit fit)
{
    viewer_->fit = fit;
    ResetView();
}
DuiImageViewerFit DuiImageViewer::Fit() const { return viewer_->fit; }

double DuiImageViewer::FitScale(core::Size content, core::Rect viewport, DuiImageViewerFit fit)
{
    if (content.width <= 0 || content.height <= 0 || viewport.Width() <= 0 || viewport.Height() <= 0)
        return 1.0;
    // Actual 与视口无关，先返回常数，避免视口极小时得到 0
    if (fit == DuiImageViewerFit::Actual)
        return 1.0;
    const double horizontal = static_cast<double>(viewport.Width()) / content.width;
    const double vertical = static_cast<double>(viewport.Height()) / content.height;
    return fit == DuiImageViewerFit::Cover ? (std::max)(horizontal, vertical)
                                           : (std::min)(horizontal, vertical);
}

void DuiImageViewer::ApplyFit()
{
    viewer_->scale = (std::clamp)(FitScale(ImageSize(), Viewport(), viewer_->fit),
                                  viewer_->minimumZoom, viewer_->maximumZoom);
    const core::Size content = ImageSize();
    const core::Rect viewport = Viewport();
    const int scaledWidth = static_cast<int>(std::lround(content.width * viewer_->scale));
    const int scaledHeight = static_cast<int>(std::lround(content.height * viewer_->scale));
    viewer_->offset = {viewport.left + (viewport.Width() - scaledWidth) / 2,
                       viewport.top + (viewport.Height() - scaledHeight) / 2};
    viewer_->adjusted = false;
    ClampOffset();
}

void DuiImageViewer::ResetView()
{
    viewer_->panning = false;
    SetCaptured(false);
    ApplyFit();
}

bool DuiImageViewer::ViewAdjusted() const { return viewer_->adjusted; }

// ---- 缩放与平移 ----

void DuiImageViewer::SetZoomLimits(double minimum, double maximum)
{
    viewer_->minimumZoom = (std::max)(0.01, minimum);
    viewer_->maximumZoom = (std::max)(viewer_->minimumZoom, maximum);
    viewer_->scale = (std::clamp)(viewer_->scale, viewer_->minimumZoom, viewer_->maximumZoom);
    ClampOffset();
}
double DuiImageViewer::MinimumZoom() const { return viewer_->minimumZoom; }
double DuiImageViewer::MaximumZoom() const { return viewer_->maximumZoom; }
double DuiImageViewer::Zoom() const { return viewer_->scale; }

void DuiImageViewer::SetZoom(double zoom)
{
    const core::Rect viewport = Viewport();
    const core::Point center{viewport.left + viewport.Width() / 2, viewport.top + viewport.Height() / 2};
    // 以视口中心为锚，缩放后中心点内容不动
    const core::Point anchorImage = ScreenToImage(center);
    viewer_->scale = (std::clamp)(zoom, viewer_->minimumZoom, viewer_->maximumZoom);
    viewer_->offset = {center.x - static_cast<int>(std::lround(anchorImage.x * viewer_->scale)),
                       center.y - static_cast<int>(std::lround(anchorImage.y * viewer_->scale))};
    viewer_->adjusted = true;
    ClampOffset();
}

void DuiImageViewer::ZoomAt(core::Point anchor, double factor)
{
    if (viewer_->image == nullptr || factor <= 0.0)
        return;
    const core::Point anchorImage = ScreenToImage(anchor);
    const double target = (std::clamp)(viewer_->scale * factor, viewer_->minimumZoom, viewer_->maximumZoom);
    if (target == viewer_->scale)
        return;
    viewer_->scale = target;
    // 锚点下的图像像素保持不动
    viewer_->offset = {anchor.x - static_cast<int>(std::lround(anchorImage.x * viewer_->scale)),
                       anchor.y - static_cast<int>(std::lround(anchorImage.y * viewer_->scale))};
    viewer_->adjusted = true;
    ClampOffset();
}

void DuiImageViewer::SetOffset(core::Point offset)
{
    viewer_->offset = offset;
    ClampOffset();
}
core::Point DuiImageViewer::Offset() const { return viewer_->offset; }

void DuiImageViewer::ClampOffset()
{
    const core::Size content = ImageSize();
    const core::Rect viewport = Viewport();
    if (content.width <= 0 || content.height <= 0 || viewport.Empty())
        return;
    const double scaledWidth = static_cast<double>(content.width) * viewer_->scale;
    const double scaledHeight = static_cast<double>(content.height) * viewer_->scale;
    const auto clampAxis = [](int offset, double scaled, int viewportStart, int viewportSize)
    {
        // 装得下：居中，不让用户把图像拖到边缘
        if (scaled <= viewportSize)
            return viewportStart + static_cast<int>(std::lround((viewportSize - scaled) / 2.0));
        // 装不下：保证视口始终被图像覆盖，不允许拖出空白
        const int minimum = viewportStart + viewportSize - static_cast<int>(std::lround(scaled));
        return (std::clamp)(offset, minimum, viewportStart);
    };
    viewer_->offset = {clampAxis(viewer_->offset.x, scaledWidth, viewport.left, viewport.Width()),
                       clampAxis(viewer_->offset.y, scaledHeight, viewport.top, viewport.Height())};
}

// ---- 外观与回调 ----

void DuiImageViewer::SetBackdropColor(core::Color color)
{
    viewer_->backdrop = color;
    viewer_->backdropOverride = true;
}
core::Color DuiImageViewer::BackdropColor() const
{
    return viewer_->backdropOverride ? viewer_->backdrop
                                     : Theme().Get(core::ThemeSlot::ImageViewerBackdrop);
}
void DuiImageViewer::SetCloseOnBackdropClick(bool enabled) { viewer_->closeOnBackdrop = enabled; }
bool DuiImageViewer::CloseOnBackdropClick() const { return viewer_->closeOnBackdrop; }
void DuiImageViewer::SetCloseOnEscape(bool enabled) { viewer_->closeOnEscape = enabled; }
bool DuiImageViewer::CloseOnEscape() const { return viewer_->closeOnEscape; }
void DuiImageViewer::SetCaptionText(std::string text) { viewer_->caption = std::move(text); }
const std::string& DuiImageViewer::CaptionText() const { return viewer_->caption; }
void DuiImageViewer::SetCloseRequestedHandler(std::function<void()> handler)
{
    viewer_->closeRequested = std::move(handler);
}

void DuiImageViewer::RequestClose()
{
    if (viewer_->closeRequested)
        viewer_->closeRequested();
}

// ---- 几何 ----

core::Rect DuiImageViewer::Viewport() const { return Bounds(); }

core::Size DuiImageViewer::ImageSize() const
{
    return viewer_->image != nullptr ? viewer_->image->Size() : core::Size{};
}

core::Rect DuiImageViewer::ImageRect() const
{
    const core::Size content = ImageSize();
    if (content.width <= 0 || content.height <= 0 || Viewport().Empty())
        return {};
    const int width = static_cast<int>(std::lround(content.width * viewer_->scale));
    const int height = static_cast<int>(std::lround(content.height * viewer_->scale));
    return {viewer_->offset.x, viewer_->offset.y, viewer_->offset.x + width, viewer_->offset.y + height};
}

core::Point DuiImageViewer::ScreenToImage(core::Point screen) const
{
    if (viewer_->scale <= 0.0)
        return screen;
    return {static_cast<int>(std::lround((screen.x - viewer_->offset.x) / viewer_->scale)),
            static_cast<int>(std::lround((screen.y - viewer_->offset.y) / viewer_->scale))};
}

// ---- 布局与交互 ----

void DuiImageViewer::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    // 未手动调整过：跟随视口重新贴合；否则保留用户的缩放与位置，仅夹取到合法范围
    if (viewer_->adjusted)
        ClampOffset();
    else
        ApplyFit();
}

bool DuiImageViewer::OnEvent(const core::Event& event)
{
    if (!Enabled() || !EffectivelyVisible())
        return false;
    const core::Rect viewport = Viewport();
    switch (event.type)
    {
    case core::EventType::PointerWheel:
        if (!viewport.Contains(event.position) || event.wheelDelta == 0 || viewer_->image == nullptr)
            return false;
        ZoomAt(event.position, std::pow(kWheelZoomStep, static_cast<double>(event.wheelDelta) / 120.0));
        return true;
    case core::EventType::PointerDown:
        if (event.button != core::PointerButton::Primary || !viewport.Contains(event.position))
            return false;
        SetFocused(true);
        // 图片之外的背板：按配置请求关闭；图片之内的按下则开始平移
        if (!ImageRect().Contains(event.position))
        {
            if (viewer_->closeOnBackdrop)
                RequestClose();
            return true;
        }
        viewer_->panning = true;
        viewer_->panOrigin = event.position;
        viewer_->panStartOffset = viewer_->offset;
        SetCaptured(true);
        SetPointerCursor(core::DuiPointerCursor::Move);
        return true;
    case core::EventType::PointerMove:
        if (!viewer_->panning)
            return false;
        viewer_->adjusted = true;
        // 平移按屏幕位移累加，不经过缩放换算
        SetOffset({viewer_->panStartOffset.x + (event.position.x - viewer_->panOrigin.x),
                   viewer_->panStartOffset.y + (event.position.y - viewer_->panOrigin.y)});
        SetPointerCursor(core::DuiPointerCursor::Move);
        return true;
    case core::EventType::PointerUp:
        if (!viewer_->panning)
            return false;
        viewer_->panning = false;
        SetCaptured(false);
        SetPointerCursor(core::DuiPointerCursor::Arrow);
        return true;
    case core::EventType::PointerCancel:
        if (!viewer_->panning)
            return false;
        viewer_->panning = false;
        SetCaptured(false);
        SetPointerCursor(core::DuiPointerCursor::Arrow);
        return true;
    case core::EventType::PointerDoubleClick:
        if (!viewport.Contains(event.position) || viewer_->image == nullptr)
            return false;
        // 贴合视图 ↔ 原始尺寸 之间切换，锚在双击点
        if (std::abs(viewer_->scale - FitScale(ImageSize(), viewport, viewer_->fit)) <= kFitEqualTolerance)
        {
            const core::Point anchorImage = ScreenToImage(event.position);
            viewer_->scale = (std::clamp)(1.0, viewer_->minimumZoom, viewer_->maximumZoom);
            viewer_->offset = {event.position.x - static_cast<int>(std::lround(anchorImage.x * viewer_->scale)),
                               event.position.y - static_cast<int>(std::lround(anchorImage.y * viewer_->scale))};
            viewer_->adjusted = true;
            ClampOffset();
        }
        else
        {
            ResetView();
        }
        return true;
    case core::EventType::KeyDown:
        if (event.key == core::key::Escape && viewer_->closeOnEscape)
        {
            RequestClose();
            return true;
        }
        return false;
    default:
        return false;
    }
}

void DuiImageViewer::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::Rect bounds = Bounds();
    const core::Rect visible = core::Rect::Intersect(bounds, dirty);
    if (visible.Empty())
        return;
    canvas.FillRect(visible, BackdropColor());
    const core::Size content = ImageSize();
    if (viewer_->image != nullptr && content.width > 0 && content.height > 0 && !bounds.Empty())
    {
        // 只绘制视口覆盖到的源区域：放大到高倍时避免让 GDI+ 处理整张图的放大结果
        const render::DuiCanvasTransform transform{viewer_->scale, viewer_->offset};
        const core::Point topLeft = transform.UnmapPoint({bounds.left, bounds.top});
        const core::Point bottomRight = transform.UnmapPoint({bounds.right, bounds.bottom});
        const core::Rect source{(std::max)(0, topLeft.x - kSourceOverscan),
                                (std::max)(0, topLeft.y - kSourceOverscan),
                                (std::min)(content.width, bottomRight.x + kSourceOverscan),
                                (std::min)(content.height, bottomRight.y + kSourceOverscan)};
        if (!source.Empty())
        {
            canvas.PushClip(bounds);
            canvas.PushTransform(transform);
            // 画布单位即图像像素，故目标矩形与源矩形相同，缩放由变换完成
            canvas.DrawImage(*viewer_->image, source, source);
            canvas.PopTransform();
            canvas.PopClip();
        }
    }
    if (!viewer_->caption.empty() && bounds.Height() > kCaptionHeight)
    {
        const core::Rect strip{bounds.left, bounds.bottom - kCaptionHeight, bounds.right, bounds.bottom};
        const core::Rect stripVisible = core::Rect::Intersect(strip, dirty);
        if (!stripVisible.Empty())
        {
            canvas.FillRect(stripVisible, {12, 14, 20, 190});
            render::DuiTextStyle style;
            style.color = {238, 240, 246, 255};
            style.pointSize = 9;
            canvas.DrawText(viewer_->caption,
                            {strip.left + kCaptionPadding, strip.top, strip.right - kCaptionPadding, strip.bottom},
                            style, render::DuiTextAlignment::Center, false);
        }
    }
}

core::DuiAccessibilityData DuiImageViewer::CreateAccessibilityData() const
{
    core::DuiAccessibilityData data;
    data.role = core::DuiAccessibilityRole::Image;
    data.name = viewer_->caption.empty() ? "Image viewer" : viewer_->caption;
    data.keyboardFocusable = true;
    return data;
}

} // namespace ysDui::controls::media
