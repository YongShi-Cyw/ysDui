#include "ysDui/controls/feedback/DuiEmojiPanel.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::feedback {
namespace {
constexpr int MinimumCellSize = 16;
constexpr int DefaultCellSize = 32;
constexpr int DefaultColumns = 8;
constexpr int ImageContentInset = 2;
constexpr int MinimumEmojiPixelSize = 12;
constexpr int LogicalDpi = 96;
constexpr int PointsPerInch = 72;
const std::shared_ptr<const render::DuiImage> EmptyImage;

int DefaultEmojiPointSize(int cellSize)
{
    const int pixelSize = (std::max)(MinimumEmojiPixelSize, cellSize - 6);
    return (pixelSize * PointsPerInch + LogicalDpi / 2) / LogicalDpi;
}
}

class DuiEmojiPanel::Impl {
public:
    struct Item {
        std::string sequence;
        std::string tooltip;
        std::shared_ptr<const render::DuiImage> image;
    };

    std::vector<Item> items;
    std::function<void(std::string_view, int)> picked;
    render::DuiTextStyle textStyle{{20, 20, 20, 255}, "Segoe UI Emoji", 12, false};
    int cellSize{DefaultCellSize};
    int columns{DefaultColumns};
    int hoveredIndex{-1};
    int pressedIndex{-1};
    bool customTextStyle{};
};

DuiEmojiPanel::DuiEmojiPanel() : emojiPanel_(std::make_unique<Impl>()) {}
DuiEmojiPanel::~DuiEmojiPanel() = default;
DuiEmojiPanel::DuiEmojiPanel(DuiEmojiPanel&&) noexcept = default;
DuiEmojiPanel& DuiEmojiPanel::operator=(DuiEmojiPanel&&) noexcept = default;

void DuiEmojiPanel::AddEmoji(std::string sequence, std::string tooltip) {
    if (sequence.empty())
        return;
    emojiPanel_->items.push_back({std::move(sequence), std::move(tooltip), {}});
}

void DuiEmojiPanel::AddEmojiImage(std::string sequence, std::shared_ptr<const render::DuiImage> image,
                                  std::string tooltip) {
    if (sequence.empty() && (!image || image->Empty()))
        return;
    emojiPanel_->items.push_back({std::move(sequence), std::move(tooltip), std::move(image)});
}

void DuiEmojiPanel::AddEmojiSet(const std::vector<std::string>& sequences) {
    for (const std::string& sequence : sequences) {
        AddEmoji(sequence);
    }
}

void DuiEmojiPanel::Clear() {
    emojiPanel_->items.clear();
    emojiPanel_->hoveredIndex = -1;
    emojiPanel_->pressedIndex = -1;
    SetCaptured(false);
}

int DuiEmojiPanel::Count() const {
    return static_cast<int>(emojiPanel_->items.size());
}

std::string DuiEmojiPanel::EmojiAt(int index) const {
    return index >= 0 && index < Count() ? emojiPanel_->items[index].sequence : std::string{};
}

std::string DuiEmojiPanel::TooltipAt(int index) const {
    return index >= 0 && index < Count() ? emojiPanel_->items[index].tooltip : std::string{};
}

const std::shared_ptr<const render::DuiImage>& DuiEmojiPanel::ImageAt(int index) const {
    return index >= 0 && index < Count() ? emojiPanel_->items[index].image : EmptyImage;
}

void DuiEmojiPanel::SetCellSize(int pixels) {
    emojiPanel_->cellSize = (std::max)(MinimumCellSize, pixels);
}

int DuiEmojiPanel::CellSize() const {
    return emojiPanel_->cellSize;
}

void DuiEmojiPanel::SetColumns(int columns) {
    emojiPanel_->columns = (std::max)(1, columns);
}

int DuiEmojiPanel::Columns() const {
    return emojiPanel_->columns;
}

int DuiEmojiPanel::RowCount() const {
    return Count() == 0 ? 0 : (Count() + Columns() - 1) / Columns();
}

core::Size DuiEmojiPanel::DesiredSize() const {
    return {Columns() * CellSize(), RowCount() * CellSize()};
}

void DuiEmojiPanel::SetPickHandler(std::function<void(std::string_view, int)> handler) {
    emojiPanel_->picked = std::move(handler);
}

void DuiEmojiPanel::SetTextStyle(render::DuiTextStyle style) {
    emojiPanel_->textStyle = std::move(style);
    emojiPanel_->customTextStyle = true;
}

core::Rect DuiEmojiPanel::CellRect(int index) const {
    if (index < 0 || index >= Count()) {
        return {};
    }
    const int column = index % Columns();
    const int row = index / Columns();
    const int left = Bounds().left + column * CellSize();
    const int top = Bounds().top + row * CellSize();
    return {left, top, left + CellSize(), top + CellSize()};
}

int DuiEmojiPanel::HitTestIndex(core::Point point) const {
    if (!Bounds().Contains(point)) {
        return -1;
    }
    const int column = (point.x - Bounds().left) / CellSize();
    const int row = (point.y - Bounds().top) / CellSize();
    const int index = row * Columns() + column;
    return index >= 0 && index < Count() ? index : -1;
}

int DuiEmojiPanel::HoveredIndex() const {
    return emojiPanel_->hoveredIndex;
}

bool DuiEmojiPanel::OnEvent(const core::Event& event) {
    if (!Enabled()) {
        return false;
    }
    const int index = HitTestIndex(event.position);
    switch (event.type) {
    case core::EventType::PointerMove:
        emojiPanel_->hoveredIndex = index;
        return index >= 0;
    case core::EventType::PointerDown:
        if (index < 0) {
            return false;
        }
        emojiPanel_->pressedIndex = index;
        SetCaptured(true);
        return true;
    case core::EventType::PointerCancel:
        emojiPanel_->pressedIndex = -1;
        SetCaptured(false);
        return true;
    case core::EventType::PointerUp: {
        const int pressedIndex = emojiPanel_->pressedIndex;
        emojiPanel_->pressedIndex = -1;
        SetCaptured(false);
        if (pressedIndex < 0 || pressedIndex != index) {
            return false;
        }
        if (emojiPanel_->picked) {
            emojiPanel_->picked(emojiPanel_->items[index].sequence, index);
        }
        return true;
    }
    default:
        return false;
    }
}

void DuiEmojiPanel::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!EffectivelyVisible()) {
        return;
    }
    const core::Rect clip = core::Rect::Intersect(Bounds(), dirty);
    if (clip.Empty()) {
        return;
    }
    canvas.PushClip(clip);
    for (int index = 0; index < Count(); ++index) {
        const core::Rect cell = CellRect(index);
        if (core::Rect::Intersect(cell, dirty).Empty()) {
            continue;
        }
        if (index == emojiPanel_->pressedIndex)
            canvas.FillRoundedRect(cell, 4, Theme().Get(core::ThemeSlot::EmojiPressed));
        else if (index == emojiPanel_->hoveredIndex)
            canvas.FillRoundedRect(cell, 4, Theme().Get(core::ThemeSlot::EmojiHover));
        const auto& item = emojiPanel_->items[index];
        if (item.image && !item.image->Empty()) {
            const core::Rect content{cell.left + ImageContentInset, cell.top + ImageContentInset,
                                     cell.right - ImageContentInset, cell.bottom - ImageContentInset};
            canvas.DrawImage(*item.image, content);
        } else {
            render::DuiTextStyle textStyle = emojiPanel_->textStyle;
            if (!emojiPanel_->customTextStyle) {
                textStyle.color = Theme().Get(core::ThemeSlot::EmojiText);
                textStyle.pointSize = DefaultEmojiPointSize(CellSize());
            }
            canvas.DrawText(item.sequence, cell, textStyle,
                            render::DuiTextAlignment::Center, false);
        }
    }
    canvas.PopClip();
}

} // namespace ysDui::controls::feedback
