#include "ysDui/controls/basic/DuiStatusBar.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {

class DuiStatusBar::Impl {
public:
    struct Pane final {
        std::string text;
        int width{80};
        bool spring{};
    };

    std::vector<Pane> panes;
    render::DuiTextStyle style{{60, 60, 70, 255}, {}, 9, false};
    core::Color background{245, 245, 247, 255};
    core::Color border{210, 210, 216, 255};
    bool styleOverride{};
    bool backgroundOverride{};
    bool borderOverride{};
};

DuiStatusBar::DuiStatusBar() : statusBar_(std::make_unique<Impl>()) {}
DuiStatusBar::~DuiStatusBar() = default;
DuiStatusBar::DuiStatusBar(DuiStatusBar&&) noexcept = default;
DuiStatusBar& DuiStatusBar::operator=(DuiStatusBar&&) noexcept = default;

int DuiStatusBar::AddPane(int width, bool spring)
{
    statusBar_->panes.push_back({{}, std::max(1, width), spring});
    return static_cast<int>(statusBar_->panes.size() - 1);
}

void DuiStatusBar::ClearPanes() { statusBar_->panes.clear(); }
int DuiStatusBar::PaneCount() const { return static_cast<int>(statusBar_->panes.size()); }

void DuiStatusBar::SetPaneText(int index, std::string text)
{
    if (index >= 0 && index < PaneCount()) statusBar_->panes[static_cast<std::size_t>(index)].text = std::move(text);
}

std::string DuiStatusBar::PaneText(int index) const
{
    return index >= 0 && index < PaneCount() ? statusBar_->panes[static_cast<std::size_t>(index)].text : std::string{};
}

void DuiStatusBar::SetPaneWidth(int index, int width)
{
    if (index >= 0 && index < PaneCount()) statusBar_->panes[static_cast<std::size_t>(index)].width = std::max(1, width);
}

void DuiStatusBar::SetPaneSpring(int index, bool spring)
{
    if (index >= 0 && index < PaneCount()) statusBar_->panes[static_cast<std::size_t>(index)].spring = spring;
}

core::Rect DuiStatusBar::ComputePaneRect(int index, core::Rect bounds) const
{
    if (index < 0 || index >= PaneCount()) return {};
    int fixedWidth{};
    int springCount{};
    for (const auto& pane : statusBar_->panes) {
        if (pane.spring) ++springCount;
        else fixedWidth += pane.width;
    }
    const int springWidth = springCount == 0 ? 0 : std::max(0, bounds.Width() - fixedWidth) / springCount;
    int left = bounds.left;
    for (int current = 0; current < PaneCount(); ++current) {
        const auto& pane = statusBar_->panes[static_cast<std::size_t>(current)];
        const int width = pane.spring ? springWidth : pane.width;
        if (current == index) return {left, bounds.top, left + width, bounds.bottom};
        left += width;
    }
    return {};
}

core::Size DuiStatusBar::DesiredSize() const { return {200, DefaultHeight}; }
void DuiStatusBar::SetStyle(render::DuiTextStyle style) { statusBar_->style = std::move(style); statusBar_->styleOverride = true; }
void DuiStatusBar::SetBackgroundColor(core::Color color) { statusBar_->background = color; statusBar_->backgroundOverride = true; }
void DuiStatusBar::SetBorderColor(core::Color color) { statusBar_->border = color; statusBar_->borderOverride = true; }
void DuiStatusBar::Layout(core::Rect bounds) { SetBounds(bounds); }

void DuiStatusBar::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible()) return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty()) return;
    const core::Color background = statusBar_->backgroundOverride ? statusBar_->background
        : Theme().Get(core::ThemeSlot::StatusBackground);
    const core::Color border = statusBar_->borderOverride ? statusBar_->border
        : Theme().Get(core::ThemeSlot::PanelBorder);
    render::DuiTextStyle style = statusBar_->style;
    if (!statusBar_->styleOverride)
        style.color = Theme().Get(core::ThemeSlot::StatusText);
    canvas.FillRect(bounds, background);
    canvas.FillRect({bounds.left, bounds.top, bounds.right, std::min(bounds.bottom, bounds.top + 1)}, border);
    for (int index = 0; index < PaneCount(); ++index) {
        const core::Rect pane = core::Rect::Intersect(ComputePaneRect(index, Bounds()), dirty);
        if (pane.Empty()) continue;
        if (index + 1 < PaneCount() && pane.Width() > 0)
            canvas.FillRect({pane.right - 1, pane.top + 3, pane.right, std::max(pane.top + 3, pane.bottom - 3)}, border);
        const auto& text = statusBar_->panes[static_cast<std::size_t>(index)].text;
        if (!text.empty()) {
            core::Rect textBounds{pane.left + 6, pane.top, std::max(pane.left + 6, pane.right - 4), pane.bottom};
            canvas.DrawText(text, textBounds, style, render::DuiTextAlignment::Start, false);
        }
    }
}

} // namespace ysDui::controls::basic
