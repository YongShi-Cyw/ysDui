#include "ysDui/controls/basic/DuiToolBar.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {
namespace {
constexpr int DefaultHeight = 32;
constexpr int MinimumButtonWidth = 28;
constexpr int ButtonHorizontalPadding = 10;
constexpr int SeparatorWidth = 8;
constexpr int OverflowWidth = 28;

enum class ItemKind { Button, Separator, Stretch };
}

class DuiToolBar::Impl {
public:
    struct Item final {
        ItemKind kind{ItemKind::Button};
        std::string text;
        std::uint32_t id{};
        core::Rect bounds;
        bool overflow{};
    };

    std::vector<Item> items;
    render::DuiTextMeasurer* measurer{};
    std::function<void(std::uint32_t)> commandHandler;
    std::function<void(std::vector<DuiToolBarCommand>)> overflowHandler;
    render::DuiTextStyle textStyle{{40, 40, 50, 255}, {}, 9, false};
    core::Color text{40, 40, 50, 255};
    bool textStyleOverride{};
    core::Rect overflowBounds;
    bool hasOverflow{};

    [[nodiscard]] int ButtonWidth(const Item& item) const {
        if (measurer) {
            const int width = measurer->MeasureText(item.text, textStyle, {}).size.width;
            return (std::max)(MinimumButtonWidth, width + ButtonHorizontalPadding * 2);
        }
        return (std::max)(MinimumButtonWidth, static_cast<int>(item.text.size()) * textStyle.pointSize + ButtonHorizontalPadding * 2);
    }
};

DuiToolBar::DuiToolBar() : toolBar_(std::make_unique<Impl>()) {}
DuiToolBar::~DuiToolBar() = default;
DuiToolBar::DuiToolBar(DuiToolBar&&) noexcept = default;
DuiToolBar& DuiToolBar::operator=(DuiToolBar&&) noexcept = default;
void DuiToolBar::AddButton(std::string text, std::uint32_t id) { toolBar_->items.push_back({ItemKind::Button, std::move(text), id, {}, false}); Layout(Bounds()); }
void DuiToolBar::AddSeparator() { toolBar_->items.push_back({ItemKind::Separator, {}, 0, {}, false}); Layout(Bounds()); }
void DuiToolBar::AddStretch() { toolBar_->items.push_back({ItemKind::Stretch, {}, 0, {}, false}); Layout(Bounds()); }
void DuiToolBar::ClearItems() { toolBar_->items.clear(); toolBar_->overflowBounds = {}; toolBar_->hasOverflow = false; }
int DuiToolBar::ItemCount() const { return static_cast<int>(toolBar_->items.size()); }
void DuiToolBar::SetTextMeasurer(render::DuiTextMeasurer* measurer) { toolBar_->measurer = measurer; Layout(Bounds()); }
void DuiToolBar::SetTextStyle(render::DuiTextStyle style) { toolBar_->textStyle = std::move(style); toolBar_->textStyleOverride = true; Layout(Bounds()); }
void DuiToolBar::SetCommandHandler(std::function<void(std::uint32_t)> handler) { toolBar_->commandHandler = std::move(handler); }
void DuiToolBar::SetOverflowRequestedHandler(std::function<void(std::vector<DuiToolBarCommand>)> handler) { toolBar_->overflowHandler = std::move(handler); }
bool DuiToolBar::HasOverflow() const { return toolBar_->hasOverflow; }
core::Rect DuiToolBar::OverflowRect() const { return toolBar_->overflowBounds; }
core::Rect DuiToolBar::ItemRect(int index) const { return index >= 0 && index < ItemCount() ? toolBar_->items[index].bounds : core::Rect{}; }
core::Size DuiToolBar::DesiredSize() const { return {200, DefaultHeight}; }
void DuiToolBar::Layout(core::Rect bounds) {
    SetBounds(bounds);
    auto& state = *toolBar_;
    state.hasOverflow = false;
    state.overflowBounds = {};
    int stretchCount{};
    for (auto& item : state.items) {
        item.bounds = {};
        item.overflow = false;
        if (item.kind == ItemKind::Stretch) ++stretchCount;
    }
    int visibleUsed{};
    bool overflowing{};
    for (auto& item : state.items) {
        if (item.kind == ItemKind::Stretch) continue;
        const int width = item.kind == ItemKind::Button ? state.ButtonWidth(item) : SeparatorWidth;
        if (item.kind == ItemKind::Button && (overflowing || visibleUsed + width + OverflowWidth > bounds.Width())) {
            item.overflow = true;
            state.hasOverflow = true;
            overflowing = true;
        } else if (!item.overflow) {
            visibleUsed += width;
        }
    }
    if (state.hasOverflow) visibleUsed += OverflowWidth;
    const int stretchSpace = (std::max)(0, bounds.Width() - visibleUsed);
    const int stretchEach = stretchCount == 0 ? 0 : stretchSpace / stretchCount;
    int stretchRemainder = stretchCount == 0 ? 0 : stretchSpace % stretchCount;
    int left = bounds.left;
    for (auto& item : state.items) {
        if (item.overflow) continue;
        int width{};
        if (item.kind == ItemKind::Button) width = state.ButtonWidth(item);
        else if (item.kind == ItemKind::Separator) width = SeparatorWidth;
        else { width = stretchEach + (stretchRemainder > 0 ? 1 : 0); if (stretchRemainder > 0) --stretchRemainder; }
        item.bounds = {left, bounds.top, left + width, bounds.bottom};
        left += width;
    }
    if (state.hasOverflow) state.overflowBounds = {left, bounds.top, bounds.right, bounds.bottom};
}
bool DuiToolBar::OnEvent(const core::Event& event) {
    if (!Enabled() || event.type != core::EventType::PointerDown) return false;
    if (toolBar_->hasOverflow && toolBar_->overflowBounds.Contains(event.position)) {
        if (toolBar_->overflowHandler) {
            std::vector<DuiToolBarCommand> commands;
            for (const auto& item : toolBar_->items) if (item.overflow) commands.push_back({item.id, item.text});
            toolBar_->overflowHandler(std::move(commands));
        }
        return true;
    }
    for (const auto& item : toolBar_->items) {
        if (item.kind != ItemKind::Button || item.overflow || !item.bounds.Contains(event.position)) continue;
        if (toolBar_->commandHandler) toolBar_->commandHandler(item.id);
        return true;
    }
    return false;
}
void DuiToolBar::Paint(render::Canvas& canvas, core::Rect dirty) const {
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty()) return;
    render::DuiTextStyle textStyle = toolBar_->textStyle;
    if (!toolBar_->textStyleOverride)
        textStyle.color = Theme().Get(core::ThemeSlot::PanelText);
    const core::Color border = Theme().Get(core::ThemeSlot::PanelBorder);
    canvas.FillRect(bounds, Theme().Get(core::ThemeSlot::ToolbarBackground));
    canvas.FillRect({bounds.left, bounds.bottom - 1, bounds.right, bounds.bottom}, border);
    for (const auto& item : toolBar_->items) {
        if (item.overflow || item.kind != ItemKind::Button) continue;
        canvas.DrawText(item.text, item.bounds, textStyle, render::DuiTextAlignment::Center, false);
    }
    for (const auto& item : toolBar_->items) {
        if (item.overflow || item.kind != ItemKind::Separator || item.bounds.Empty()) continue;
        const int center = (item.bounds.left + item.bounds.right) / 2;
        canvas.FillRect({center, item.bounds.top + 6, center + 1, item.bounds.bottom - 6}, border);
    }
    if (toolBar_->hasOverflow)
        canvas.DrawText("\xE2\x80\xA6", toolBar_->overflowBounds, textStyle,
                        render::DuiTextAlignment::Center, false);
}

} // namespace ysDui::controls::basic
