#include "ysDui/controls/window/DuiFlyout.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/render/DuiPopupSurface.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"

namespace ysDui::controls::window {
namespace {
/** 内容未提供有效首选尺寸时的兜底尺寸，避免弹出零尺寸窗口。 */
constexpr core::Size FallbackContentSize{200, 120};
} // namespace

class DuiFlyout::Impl {
public:
    std::function<std::unique_ptr<core::Control>()> contentFactory;
    std::function<void(core::Control&, core::Rect)> layoutHandler;
    std::function<void()> dismissed;
    std::unique_ptr<ui::IPopupHost> popup;
    core::Size fixedSize{};
    bool visible{};
};

DuiFlyout::DuiFlyout() : flyout_(std::make_unique<Impl>()) {}
DuiFlyout::~DuiFlyout()
{
    // 先清空内容工厂与回调，避免宿主析构路径回调到已失效的状态
    if (flyout_)
    {
        flyout_->dismissed = {};
        flyout_->contentFactory = {};
        flyout_->popup.reset();
    }
}
DuiFlyout::DuiFlyout(DuiFlyout&&) noexcept = default;
DuiFlyout& DuiFlyout::operator=(DuiFlyout&&) noexcept = default;

void DuiFlyout::SetContentFactory(std::function<std::unique_ptr<core::Control>()> factory)
{
    flyout_->contentFactory = std::move(factory);
}
void DuiFlyout::SetFixedSize(core::Size size) { flyout_->fixedSize = size; }
core::Size DuiFlyout::FixedSize() const { return flyout_->fixedSize; }
void DuiFlyout::SetLayoutHandler(std::function<void(core::Control&, core::Rect)> handler)
{
    flyout_->layoutHandler = std::move(handler);
}
void DuiFlyout::SetDismissedHandler(std::function<void()> handler) { flyout_->dismissed = std::move(handler); }

bool DuiFlyout::Show(ui::IUiHostFactory& factory, ui::HostRef owner, core::Rect anchor,
                     ui::DuiPopupPlacement placement)
{
    if (!flyout_->contentFactory)
        return false;
    std::unique_ptr<core::Control> content = flyout_->contentFactory();
    if (!content)
        return false;

    // 关闭上一次尚未结束的气泡，避免同时存在两个
    flyout_->popup.reset();
    flyout_->visible = false;

    flyout_->popup = factory.CreatePopupHost(owner);
    if (!flyout_->popup)
        return false;

    // 尺寸：显式固定值优先，其次内容首选尺寸，最后兜底
    const core::Size desired = content->DesiredSize();
    ui::DuiPopupOptions options;
    options.anchor = anchor;
    options.placement = placement;
    options.size = {
        flyout_->fixedSize.width > 0 ? flyout_->fixedSize.width
                                     : desired.width > 0 ? desired.width : FallbackContentSize.width,
        flyout_->fixedSize.height > 0 ? flyout_->fixedSize.height
                                      : desired.height > 0 ? desired.height : FallbackContentSize.height,
    };
    options.dismissOnFocusLost = true;

    core::Control* raw = content.get();
    const auto layoutHandler = flyout_->layoutHandler;
    const auto layout = [raw, layoutHandler](core::Rect bounds)
    {
        if (layoutHandler)
            layoutHandler(*raw, bounds);
        else
            raw->Layout(bounds);
    };
    const core::Size popupSize = options.size;
    const auto paint = [raw, popupSize](render::Canvas& canvas, core::Rect dirty)
    {
        // 弹出宿主不画背景：气泡内容必须自绘表面，否则会与宿主窗口背景混为一体
        // 先判空再取 Theme：避免 MSVC /analyze 把 dynamic_cast 失败误判为 raw 为空（C28182）。
        if (raw == nullptr)
            return;
        const core::DuiTheme& theme = raw->Theme();
        const core::Rect surface{0, 0, popupSize.width, popupSize.height};
        render::PaintPopupBackground(canvas, core::Rect::Intersect(surface, dirty), theme);
        if (auto* renderable = dynamic_cast<render::DuiRenderable*>(raw))
            renderable->Paint(canvas, dirty);
        render::PaintPopupBorder(canvas, surface, theme);
    };

    if (!flyout_->popup->Show(options, std::move(content), layout, paint))
    {
        flyout_->popup.reset();
        return false;
    }
    flyout_->popup->SetDismissedHandler([state = flyout_.get()]
    {
        state->visible = false;
        if (state->dismissed)
            state->dismissed();
    });
    flyout_->visible = true;
    return true;
}

void DuiFlyout::Hide()
{
    flyout_->popup.reset();
    flyout_->visible = false;
}

bool DuiFlyout::Visible() const { return flyout_->visible; }

std::uintptr_t DuiFlyout::NativeHandle() const
{
    return flyout_->popup ? flyout_->popup->NativeHandle() : 0;
}

} // namespace ysDui::controls::window
