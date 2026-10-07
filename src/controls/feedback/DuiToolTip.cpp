#include "ysDui/controls/feedback/DuiToolTip.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/core/DuiHost.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::feedback {
namespace {
constexpr int DefaultDelayMilliseconds = 500;
constexpr int HorizontalPadding = 8;
constexpr int VerticalPadding = 4;
constexpr int DefaultLineHeight = 18;
constexpr int CursorOffsetX = 12;
constexpr int CursorOffsetY = 18;

class ToolTipContent final : public core::Control, public render::DuiRenderable {
public:
    ToolTipContent(std::string text, render::DuiTextStyle style, core::Color background,
                   core::Color border)
        : text_(std::move(text)), style_(std::move(style)), background_(background), border_(border) {}

    void Paint(render::Canvas& canvas, core::Rect dirty) const override
    {
        const auto bounds = core::Rect::Intersect(Bounds(), dirty);
        if (bounds.Empty()) return;
        canvas.FillRect(bounds, background_);
        canvas.StrokeRoundedRect(bounds, 0, border_, 1.0F);
        canvas.DrawText(text_, {bounds.left + HorizontalPadding, bounds.top + VerticalPadding,
                                bounds.right - HorizontalPadding, bounds.bottom - VerticalPadding},
                        style_, render::DuiTextAlignment::Start, false);
    }

private:
    std::string text_;
    render::DuiTextStyle style_;
    core::Color background_;
    core::Color border_;
};
} // namespace

class DuiToolTipManager::Impl {
public:
    struct Entry final {
        core::Control* control{};
        std::string text;
    };

    core::AnimationClock* clock{};
    core::Host* host{};
    core::Host::PointerHoverHandlerId hoverHandler{};
    core::AnimationClock::TaskId delayTask{};
    ui::IPopupHost* popup{};
    render::DuiTextMeasurer* measurer{};
    render::DuiTextStyle style{{40, 40, 40, 255}, {}, 9, false};
    std::vector<Entry> entries;
    core::Control* pending{};
    core::Point anchor;
    std::string showingText;
    int delayMilliseconds{DefaultDelayMilliseconds};
    int generation{};
    bool showing{};
    bool styleOverride{};
};

DuiToolTipManager::DuiToolTipManager() : toolTip_(std::make_unique<Impl>()) {}
DuiToolTipManager::~DuiToolTipManager() { SetHost(nullptr); SetPopupHost(nullptr); SetAnimationClock(nullptr); }

void DuiToolTipManager::SetAnimationClock(core::AnimationClock* clock)
{
    if (toolTip_->clock == clock) return;
    CancelDelay();
    toolTip_->clock = clock;
}

void DuiToolTipManager::SetHost(core::Host* host)
{
    if (toolTip_->host == host) return;
    HideNow();
    if (toolTip_->host != nullptr)
        toolTip_->host->RemovePointerHoverHandler(toolTip_->hoverHandler);
    toolTip_->host = host;
    toolTip_->hoverHandler = 0;
    if (host != nullptr)
    {
        toolTip_->hoverHandler = host->SetPointerHoverHandler([this](core::Control* control, core::Point anchor) {
            if (control != nullptr) OnHover(control, anchor);
            else OnLeave();
        });
    }
}

void DuiToolTipManager::SetPopupHost(ui::IPopupHost* popup)
{
    if (toolTip_->popup == popup) return;
    if (toolTip_->popup) {
        toolTip_->popup->SetDismissedHandler({});
        if (toolTip_->showing) toolTip_->popup->RequestHide();
    }
    toolTip_->popup = popup;
    toolTip_->showing = false;
    if (popup) popup->SetDismissedHandler([state = toolTip_.get()] { state->showing = false; state->showingText.clear(); });
}

void DuiToolTipManager::SetTextMeasurer(render::DuiTextMeasurer* measurer) { toolTip_->measurer = measurer; }
void DuiToolTipManager::SetTextStyle(render::DuiTextStyle style) { toolTip_->style = std::move(style); toolTip_->styleOverride = true; }
void DuiToolTipManager::SetDelayMilliseconds(int milliseconds) { toolTip_->delayMilliseconds = (std::max)(0, milliseconds); }
int DuiToolTipManager::DelayMilliseconds() const { return toolTip_->delayMilliseconds; }

void DuiToolTipManager::Register(core::Control* control, std::string text)
{
    if (!control) return;
    const auto found = std::find_if(toolTip_->entries.begin(), toolTip_->entries.end(), [control](const Impl::Entry& entry) { return entry.control == control; });
    if (text.empty()) {
        if (found != toolTip_->entries.end()) toolTip_->entries.erase(found);
        if (toolTip_->pending == control) HideNow();
        return;
    }
    if (found == toolTip_->entries.end()) toolTip_->entries.push_back({control, std::move(text)});
    else found->text = std::move(text);
}

void DuiToolTipManager::Unregister(core::Control* control)
{
    if (!control) return;
    std::erase_if(toolTip_->entries, [control](const Impl::Entry& entry) { return entry.control == control; });
    if (toolTip_->pending == control) HideNow();
}

std::string DuiToolTipManager::TextFor(const core::Control* control) const
{
    const auto found = std::find_if(toolTip_->entries.begin(), toolTip_->entries.end(), [control](const Impl::Entry& entry) { return entry.control == control; });
    return found == toolTip_->entries.end() ? std::string{} : found->text;
}

void DuiToolTipManager::OnHover(core::Control* control, core::Point anchor)
{
    const std::string text = TextFor(control);
    if (!control || text.empty()) { OnLeave(nullptr); return; }
    if (toolTip_->pending == control && (toolTip_->delayTask != 0 || toolTip_->showing)) return;
    HideNow();
    toolTip_->pending = control;
    toolTip_->anchor = anchor;
    const int generation = ++toolTip_->generation;
    if (!toolTip_->clock || toolTip_->delayMilliseconds == 0) {
        ShowPending();
        return;
    }
    toolTip_->delayTask = toolTip_->clock->Schedule(toolTip_->delayMilliseconds, [this, generation](double progress) {
        if (progress >= 1.0 && generation == toolTip_->generation) ShowPending();
    });
}

void DuiToolTipManager::OnLeave(const core::Control* control)
{
    if (control && control != toolTip_->pending && !toolTip_->showing) return;
    HideNow();
}

void DuiToolTipManager::HideNow()
{
    CancelDelay();
    ++toolTip_->generation;
    toolTip_->pending = nullptr;
    toolTip_->showingText.clear();
    if (toolTip_->popup && toolTip_->showing) toolTip_->popup->RequestHide();
    toolTip_->showing = false;
}

bool DuiToolTipManager::Showing() const { return toolTip_->showing; }
std::string DuiToolTipManager::ShowingText() const { return toolTip_->showingText; }

void DuiToolTipManager::CancelDelay()
{
    if (toolTip_->clock && toolTip_->delayTask != 0) toolTip_->clock->Cancel(toolTip_->delayTask);
    toolTip_->delayTask = 0;
}

void DuiToolTipManager::ShowPending()
{
    CancelDelay();
    if (!toolTip_->popup || !toolTip_->pending) return;
    const std::string text = TextFor(toolTip_->pending);
    if (text.empty()) return;
    const core::DuiTheme& theme = toolTip_->pending->Theme();
    render::DuiTextStyle style = toolTip_->style;
    if (!toolTip_->styleOverride) style.color = theme.Get(core::ThemeSlot::ToolTipText);
    const auto metrics = toolTip_->measurer ? toolTip_->measurer->MeasureText(text, style, {}) :
        render::DuiTextMetrics{{static_cast<int>(text.size()) * style.pointSize, DefaultLineHeight}, 1, DefaultLineHeight};
    const core::Size size{(std::max)(1, metrics.size.width + HorizontalPadding * 2), (std::max)(1, metrics.size.height + VerticalPadding * 2)};
    auto content = std::make_unique<ToolTipContent>(text, std::move(style),
        theme.Get(core::ThemeSlot::ToolTipBackground), theme.Get(core::ThemeSlot::ToolTipBorder));
    ToolTipContent* raw = content.get();
    ui::DuiPopupOptions options;
    options.anchor = {toolTip_->anchor.x + CursorOffsetX, toolTip_->anchor.y + CursorOffsetY,
                      toolTip_->anchor.x + CursorOffsetX, toolTip_->anchor.y + CursorOffsetY};
    options.size = size;
    options.dismissOnFocusLost = false;
    toolTip_->showing = toolTip_->popup->Show(options, std::move(content), [raw](core::Rect bounds) { raw->SetBounds(bounds); },
        [raw](render::Canvas& canvas, core::Rect dirty) { raw->Paint(canvas, dirty); });
    if (toolTip_->showing) toolTip_->showingText = text;
}

} // namespace ysDui::controls::feedback
