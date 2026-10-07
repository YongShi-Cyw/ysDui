#include "ysDui/controls/layout/DuiScrollView.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/controls/input/DuiScrollBar.hpp"
#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::layout {

class DuiScrollView::Impl {
public:
    core::Control* content{};
    controls::input::DuiScrollBar* scrollBar{};
    core::Size contentSize{};
    int scrollBarWidth{17};
    core::Rect viewport{};
    bool followBottom{};
};

DuiScrollView::DuiScrollView() : scrollView_(std::make_unique<Impl>()) {
    auto scrollBar = std::make_unique<controls::input::DuiScrollBar>();
    scrollView_->scrollBar = scrollBar.get();
    scrollBar->SetLineSize(16);
    // 位置变化时同步贴底状态：程序化定位不会触发该回调（notify=false）
    scrollBar->SetValueChangedHandler([this](int position)
    {
        scrollView_->followBottom = position >= scrollView_->scrollBar->Maximum();
        Layout(Bounds());
    });
    AddChild(std::move(scrollBar));
}
DuiScrollView::~DuiScrollView() = default;
DuiScrollView::DuiScrollView(DuiScrollView&& other) noexcept
    : core::Control(std::move(other)), scrollView_(std::move(other.scrollView_)) {
    if (scrollView_ && scrollView_->scrollBar)
        scrollView_->scrollBar->SetValueChangedHandler([this](int position)
        {
            scrollView_->followBottom = position >= scrollView_->scrollBar->Maximum();
            Layout(Bounds());
        });
}
DuiScrollView& DuiScrollView::operator=(DuiScrollView&& other) noexcept {
    if (this == &other) return *this;
    core::Control::operator=(std::move(other));
    scrollView_ = std::move(other.scrollView_);
    if (scrollView_ && scrollView_->scrollBar)
        scrollView_->scrollBar->SetValueChangedHandler([this](int position)
        {
            scrollView_->followBottom = position >= scrollView_->scrollBar->Maximum();
            Layout(Bounds());
        });
    return *this;
}

void DuiScrollView::SetContent(std::unique_ptr<core::Control> content) {
    if (scrollView_->content != nullptr) (void)RemoveChild(scrollView_->content);
    scrollView_->content = content.get();
    if (content != nullptr) AddChild(std::move(content));
    Layout(Bounds());
}
core::Control* DuiScrollView::Content() const { return scrollView_->content; }
void DuiScrollView::SetContentSize(core::Size size) { scrollView_->contentSize = {std::max(0, size.width), std::max(0, size.height)}; Layout(Bounds()); }
core::Size DuiScrollView::ContentSize() const { return scrollView_->contentSize; }
void DuiScrollView::SetScrollBarWidth(int pixels) { scrollView_->scrollBarWidth = std::max(0, pixels); Layout(Bounds()); }
int DuiScrollView::ScrollPosition() const { return scrollView_->scrollBar->Position(); }
void DuiScrollView::SetScrollPosition(int position)
{
    scrollView_->scrollBar->SetPosition(position, false);
    // 显式定位同样遵循"是否落在末尾"的贴底规则
    scrollView_->followBottom = position >= scrollView_->scrollBar->Maximum();
    Layout(Bounds());
}

void DuiScrollView::SetFollowBottom(bool follow)
{
    if (scrollView_->followBottom == follow)
        return;
    scrollView_->followBottom = follow;
    Layout(Bounds());
}
bool DuiScrollView::FollowBottom() const { return scrollView_->followBottom; }
void DuiScrollView::ScrollToBottom()
{
    scrollView_->scrollBar->SetPosition(scrollView_->scrollBar->Maximum(), false);
    scrollView_->followBottom = true;
    Layout(Bounds());
}
bool DuiScrollView::AtBottom() const
{
    return scrollView_->scrollBar->Position() >= scrollView_->scrollBar->Maximum();
}

void DuiScrollView::Layout(core::Rect bounds) {
    SetBounds(bounds);
    const bool overflow = scrollView_->contentSize.height > std::max(0, bounds.Height());
    const int scrollWidth = overflow ? scrollView_->scrollBarWidth : 0;
    scrollView_->viewport = {bounds.left, bounds.top, std::max(bounds.left, bounds.right - scrollWidth), bounds.bottom};
    scrollView_->scrollBar->SetVisible(overflow && scrollWidth > 0);
    scrollView_->scrollBar->Layout({scrollView_->viewport.right, bounds.top, bounds.right, bounds.bottom});
    scrollView_->scrollBar->SetPageSize(std::max(1, scrollView_->viewport.Height()));
    scrollView_->scrollBar->SetRange(0, std::max(0, scrollView_->contentSize.height - scrollView_->viewport.Height()));
    // 贴底跟随：范围更新后把位置钉在末尾，内容增长（流式追加）即自动跟随
    if (scrollView_->followBottom)
        scrollView_->scrollBar->SetPosition(scrollView_->scrollBar->Maximum(), false);
    if (scrollView_->content != nullptr) {
        const int top = scrollView_->viewport.top - scrollView_->scrollBar->Position();
        // 内容宽度限制在视口内，避免盖住滚动条导致无法点击拖拽
        scrollView_->content->Layout({scrollView_->viewport.left, top, scrollView_->viewport.right,
                                         top + scrollView_->contentSize.height});
    }
}

core::Control* DuiScrollView::HitTest(core::Point point)
{
    if (!EffectivelyVisible() || !Enabled() || !Bounds().Contains(point))
        return nullptr;
    // 优先命中滚动条，避免被超高内容子控件抢先
    if (scrollView_->scrollBar != nullptr && scrollView_->scrollBar->Visible()) {
        if (core::Control* hit = scrollView_->scrollBar->HitTest(point))
            return hit;
    }
    if (scrollView_->content != nullptr && scrollView_->viewport.Contains(point)) {
        if (core::Control* hit = scrollView_->content->HitTest(point))
            return hit;
    }
    return this;
}

bool DuiScrollView::OnEvent(const core::Event& event) {
    if (!Enabled() || !EffectivelyVisible()) return false;
    if (event.type != core::EventType::PointerWheel || !scrollView_->viewport.Contains(event.position))
        return false;
    // Shift+滚轮留给内容（如 Markdown 宽表横向滚动），本控件不拦截
    if ((event.modifiers & core::modifier::Shift) != 0)
        return false;
    const int direction = event.wheelDelta > 0 ? -1 : event.wheelDelta < 0 ? 1 : 0;
    if (direction == 0) return false;
    scrollView_->scrollBar->SetPosition(ScrollPosition() + direction * scrollView_->scrollBar->LineSize() * 3);
    return true;
}

void DuiScrollView::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible()) return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty()) return;
    canvas.FillRect(bounds, Theme().Get(core::ThemeSlot::ScrollViewBackground));
    const core::Rect view = core::Rect::Intersect(scrollView_->viewport, dirty);
    if (!view.Empty() && scrollView_->content != nullptr) {
        canvas.PushClip(scrollView_->viewport);
        if (const auto* renderable = dynamic_cast<const render::DuiRenderable*>(scrollView_->content)) renderable->Paint(canvas, view);
        canvas.PopClip();
    }
    if (scrollView_->scrollBar->Visible()) scrollView_->scrollBar->Paint(canvas, dirty);
    // 外框画在本控件上（含滚动条）：内容的底边会被视口裁掉，只有本控件知道完整可视范围
    canvas.StrokeRoundedRect(Bounds(), 0, Theme().Get(core::ThemeSlot::GridBorder), 1.0F);
}

} // namespace ysDui::controls::layout
