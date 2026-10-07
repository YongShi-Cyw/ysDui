#include "ysDui/controls/basic/DuiLabel.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {
namespace {

std::vector<std::size_t> Utf8Boundaries(std::string_view text)
{
    std::vector<std::size_t> result{0};
    for (std::size_t index = 0; index < text.size();)
    {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        const std::size_t width = first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
        index += (std::min)(width, text.size() - index);
        result.push_back(index);
    }
    return result;
}

} // namespace

class DuiLabel::Impl
{
public:
    std::string text;
    render::DuiTextStyle style;
    render::DuiTextAlignment alignment{render::DuiTextAlignment::Start};
    std::string linkTarget;
    std::function<void(std::string_view)> linkActivated;
    ui::DuiClipboard* clipboard{};
    bool wordWrap{};
    bool selectable{};
    core::Color selectionColor{217, 232, 252, 255};
    bool styleOverride{};
    bool selectionColorOverride{};
    bool visited{};
    bool captured{};
    std::size_t selectionAnchor{};
    std::size_t selectionCaret{};
    int textLeft{};
    std::vector<std::size_t> boundaries;
    std::vector<int> advances;
};

DuiLabel::DuiLabel() : label_(std::make_unique<Impl>()) {}
DuiLabel::~DuiLabel() = default;
DuiLabel::DuiLabel(DuiLabel&&) noexcept = default;
DuiLabel& DuiLabel::operator=(DuiLabel&&) noexcept = default;
void DuiLabel::SetText(std::string text)
{
    label_->text = std::move(text);
    label_->selectionAnchor = 0;
    label_->selectionCaret = 0;
    label_->boundaries.clear();
    label_->advances.clear();
}
const std::string& DuiLabel::Text() const { return label_->text; }
void DuiLabel::SetStyle(render::DuiTextStyle style) { label_->style = std::move(style); label_->styleOverride = true; }
const render::DuiTextStyle& DuiLabel::Style() const { return label_->style; }
void DuiLabel::SetAlignment(render::DuiTextAlignment alignment) { label_->alignment = alignment; }
render::DuiTextAlignment DuiLabel::Alignment() const { return label_->alignment; }
void DuiLabel::SetWordWrap(bool wordWrap)
{
    label_->wordWrap = wordWrap;
    if (wordWrap)
    {
        label_->selectable = false;
        label_->selectionAnchor = 0;
        label_->selectionCaret = 0;
        label_->captured = false;
        SetCaptured(false);
    }
}
bool DuiLabel::WordWrap() const { return label_->wordWrap; }
void DuiLabel::SetSelectable(bool selectable)
{
    label_->selectable = selectable && !label_->wordWrap;
    if (!label_->selectable)
        label_->selectionAnchor = label_->selectionCaret = 0;
}
bool DuiLabel::Selectable() const { return label_->selectable; }
void DuiLabel::SetSelected(bool selected)
{
    label_->selectionAnchor = 0;
    label_->selectionCaret = selected && label_->selectable ? label_->text.size() : 0;
}
bool DuiLabel::Selected() const { return label_->selectionAnchor != label_->selectionCaret; }
void DuiLabel::SetSelectionColor(core::Color color) { label_->selectionColor = color; label_->selectionColorOverride = true; }
core::Color DuiLabel::SelectionColor() const { return label_->selectionColorOverride ? label_->selectionColor : Theme().Get(core::ThemeSlot::SelectionBackground); }
void DuiLabel::SetClipboard(ui::DuiClipboard* clipboard) { label_->clipboard = clipboard; }
std::string DuiLabel::SelectedText() const
{
    const std::size_t first = (std::min)(label_->selectionAnchor, label_->selectionCaret);
    const std::size_t last = (std::max)(label_->selectionAnchor, label_->selectionCaret);
    return label_->text.substr(first, last - first);
}
void DuiLabel::SetLinkTarget(std::string target) { label_->linkTarget = std::move(target); }
const std::string& DuiLabel::LinkTarget() const { return label_->linkTarget; }
void DuiLabel::SetVisited(bool visited) { label_->visited = visited; }
bool DuiLabel::Visited() const { return label_->visited; }
void DuiLabel::SetLinkActivatedHandler(std::function<void(std::string_view)> handler) { label_->linkActivated = std::move(handler); }
core::DuiAccessibilityData DuiLabel::CreateAccessibilityData() const
{
    return {label_->linkTarget.empty() ? core::DuiAccessibilityRole::Text : core::DuiAccessibilityRole::Hyperlink,
            label_->text, {}, {}, !label_->linkTarget.empty() || label_->selectable, {}};
}
bool DuiLabel::OnEvent(const core::Event& event)
{
    if (!Enabled() || (!label_->selectable && label_->linkTarget.empty()))
        return false;

    const auto indexAt = [this](int x)
    {
        if (label_->boundaries.empty() || label_->advances.empty())
            return std::size_t{};
        const int localX = x - label_->textLeft;
        for (std::size_t index = 0; index + 1 < label_->advances.size(); ++index)
        {
            const int midpoint = label_->advances[index] +
                (label_->advances[index + 1] - label_->advances[index]) / 2;
            if (localX < midpoint)
                return label_->boundaries[index];
        }
        return label_->boundaries.back();
    };

    const bool contains = Bounds().Contains(event.position);
    if (event.type == core::EventType::PointerMove)
    {
        SetHovered(contains);
        if (label_->captured && label_->selectable)
            label_->selectionCaret = indexAt(event.position.x);
        return label_->captured;
    }
    if (event.type == core::EventType::PointerDown && contains)
    {
        label_->captured = true;
        SetCaptured(true);
        if (label_->selectable)
            label_->selectionAnchor = label_->selectionCaret = indexAt(event.position.x);
        return true;
    }
    if (event.type == core::EventType::PointerCancel && label_->captured)
    {
        label_->captured = false;
        SetCaptured(false);
        SetHovered(false);
        return true;
    }
    if (event.type == core::EventType::PointerUp && label_->captured)
    {
        label_->captured = false;
        SetCaptured(false);
        SetHovered(contains);
        if (label_->selectable)
            label_->selectionCaret = indexAt(event.position.x);
        if (contains && !label_->linkTarget.empty() && !Selected())
        {
            label_->visited = true;
            if (label_->linkActivated)
                label_->linkActivated(label_->linkTarget);
        }
        return true;
    }
    if (event.type == core::EventType::KeyDown && !label_->linkTarget.empty() &&
        (event.key == core::key::Enter || event.key == core::key::Space))
    {
        label_->visited = true;
        if (label_->linkActivated)
            label_->linkActivated(label_->linkTarget);
        return true;
    }
    if (event.type == core::EventType::KeyDown && label_->selectable &&
        (event.modifiers & core::modifier::Control) != 0)
    {
        if (event.key == 'A' || event.key == 'a')
        {
            SetSelected(true);
            return true;
        }
        if (event.key == 'C' || event.key == 'c')
        {
            if (label_->clipboard != nullptr)
                label_->clipboard->SetText(Selected() ? SelectedText() : label_->text);
            return true;
        }
    }
    return false;
}
void DuiLabel::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible() || label_->text.empty())
        return;
    const core::Rect clipped = core::Rect::Intersect(Bounds(), dirty);
    if (clipped.Empty())
        return;

    render::DuiTextStyle style = label_->style;
    if (!label_->styleOverride)
        // 禁用时取禁用文字色：标签没有自己的交互，但需要随所属容器（如可勾选分组框）变灰
        style.color = Theme().Get(Enabled() ? core::ThemeSlot::ControlText
                                            : core::ThemeSlot::ButtonChoiceDisabledText);
    if (!label_->linkTarget.empty())
    {
        style.color = !Enabled() ? Theme().Get(core::ThemeSlot::ButtonChoiceDisabledText)
                                 : label_->visited ? Theme().Get(core::ThemeSlot::LinkVisited)
                                                   : Theme().Get(core::ThemeSlot::TextLink);
        style.underline = true;
    }
    if (label_->selectable)
    {
        label_->boundaries = Utf8Boundaries(label_->text);
        label_->advances.assign(label_->boundaries.size(), 0);
        for (std::size_t index = 1; index < label_->boundaries.size(); ++index)
            label_->advances[index] = canvas.MeasureText(label_->text.substr(0, label_->boundaries[index]), style, {}).size.width;
        const int textWidth = label_->advances.back();
        label_->textLeft = Bounds().left;
        if (label_->alignment == render::DuiTextAlignment::Center)
            label_->textLeft += (Bounds().Width() - textWidth) / 2;
        else if (label_->alignment == render::DuiTextAlignment::End)
            label_->textLeft = Bounds().right - textWidth;

        if (Selected())
        {
            const std::size_t first = (std::min)(label_->selectionAnchor, label_->selectionCaret);
            const std::size_t last = (std::max)(label_->selectionAnchor, label_->selectionCaret);
            const auto firstIt = std::lower_bound(label_->boundaries.begin(), label_->boundaries.end(), first);
            const auto lastIt = std::lower_bound(label_->boundaries.begin(), label_->boundaries.end(), last);
            const std::size_t firstIndex = static_cast<std::size_t>(firstIt - label_->boundaries.begin());
            const std::size_t lastIndex = static_cast<std::size_t>(lastIt - label_->boundaries.begin());
            canvas.FillRect({label_->textLeft + label_->advances[firstIndex], Bounds().top,
                             label_->textLeft + label_->advances[lastIndex], Bounds().bottom}, SelectionColor());
        }
    }
    canvas.DrawText(label_->text, clipped, style, label_->alignment, label_->wordWrap);
}

} // namespace ysDui::controls::basic
