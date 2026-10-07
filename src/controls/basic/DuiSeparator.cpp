#include "ysDui/controls/basic/DuiSeparator.hpp"

#include <algorithm>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::basic {
namespace {
/** 虚线单段长度与间隔（逻辑像素）。 */
constexpr int kDashLength = 4;
constexpr int kDashGap = 3;
/** 文本与线段间距的默认值。 */
constexpr int kDefaultTextGap = 8;
} // namespace

class DuiSeparator::Impl {
public:
    Orientation orientation{Orientation::Horizontal};
    core::Color color{220, 220, 224, 255};
    bool colorOverride{};
    int thickness{1};
    int inset{};
    std::string text;
    DuiSeparatorTextAlign textAlign{DuiSeparatorTextAlign::Center};
    int textGap{kDefaultTextGap};
    bool dashed{};
    render::DuiTextStyle textStyle{{20, 20, 20, 255}, {}, 9, false};
    bool textStyleOverride{};
};

DuiSeparator::DuiSeparator() : separator_(std::make_unique<Impl>()) {}
DuiSeparator::~DuiSeparator() = default;
DuiSeparator::DuiSeparator(DuiSeparator&&) noexcept = default;
DuiSeparator& DuiSeparator::operator=(DuiSeparator&&) noexcept = default;

void DuiSeparator::SetOrientation(Orientation orientation) { separator_->orientation = orientation; }
DuiSeparator::Orientation DuiSeparator::GetOrientation() const { return separator_->orientation; }
void DuiSeparator::SetColor(core::Color color) { separator_->color = color; separator_->colorOverride = true; }
core::Color DuiSeparator::GetColor() const { return separator_->colorOverride ? separator_->color : Theme().Get(core::ThemeSlot::BorderLight); }
void DuiSeparator::SetThickness(int pixels) { separator_->thickness = std::max(1, pixels); }
int DuiSeparator::GetThickness() const { return separator_->thickness; }
void DuiSeparator::SetInset(int pixels) { separator_->inset = std::max(0, pixels); }
int DuiSeparator::GetInset() const { return separator_->inset; }
void DuiSeparator::SetText(std::string text) { separator_->text = std::move(text); }
const std::string& DuiSeparator::Text() const { return separator_->text; }
void DuiSeparator::SetTextAlign(DuiSeparatorTextAlign align) { separator_->textAlign = align; }
DuiSeparatorTextAlign DuiSeparator::TextAlign() const { return separator_->textAlign; }
void DuiSeparator::SetTextGap(int pixels) { separator_->textGap = std::max(0, pixels); }
int DuiSeparator::TextGap() const { return separator_->textGap; }
void DuiSeparator::SetDashed(bool dashed) { separator_->dashed = dashed; }
bool DuiSeparator::Dashed() const { return separator_->dashed; }
void DuiSeparator::SetTextStyle(render::DuiTextStyle style) { separator_->textStyle = std::move(style); separator_->textStyleOverride = true; }
const render::DuiTextStyle& DuiSeparator::TextStyle() const { return separator_->textStyle; }

void DuiSeparator::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (!Visible()) { return; }
    const core::Rect item = Bounds();
    if (item.Empty()) { return; }

    const core::Color lineColor = GetColor();

    /** 沿主轴画一段线：虚线按固定段长拆分，实线一次填充。 */
    const auto paintSegment = [this, &canvas, &dirty, lineColor](core::Rect segment) {
        if (segment.Empty()) { return; }
        if (!separator_->dashed || separator_->orientation == Orientation::Vertical) {
            const core::Rect clipped = core::Rect::Intersect(segment, dirty);
            if (!clipped.Empty()) { canvas.FillRect(clipped, lineColor); }
            return;
        }
        for (int x = segment.left; x < segment.right; x += kDashLength + kDashGap) {
            const core::Rect dash{x, segment.top, (std::min)(x + kDashLength, segment.right), segment.bottom};
            const core::Rect clipped = core::Rect::Intersect(dash, dirty);
            if (!clipped.Empty()) { canvas.FillRect(clipped, lineColor); }
        }
    };

    if (separator_->orientation == Orientation::Vertical) {
        const int center = (item.left + item.right) / 2;
        paintSegment({center - separator_->thickness / 2, item.top + separator_->inset,
                      center - separator_->thickness / 2 + separator_->thickness,
                      item.bottom - separator_->inset});
        return;
    }

    // 水平：线段位于竖直中线，整条可用跨度是扣除内缩后的部分
    const int center = (item.top + item.bottom) / 2;
    const core::Rect lineSpan{item.left + separator_->inset, center - separator_->thickness / 2,
                              item.right - separator_->inset,
                              center - separator_->thickness / 2 + separator_->thickness};
    if (separator_->text.empty() || lineSpan.Empty()) {
        paintSegment(lineSpan);
        return;
    }

    render::DuiTextStyle style = separator_->textStyle;
    if (!separator_->textStyleOverride) { style.color = Theme().Get(core::ThemeSlot::TextSubtle); }
    const int textWidth = canvas.MeasureText(separator_->text, style, {}).size.width;
    // 跨度放不下文本时只画文本，避免出现负宽度的线段
    if (textWidth >= lineSpan.Width()) {
        canvas.DrawText(separator_->text, {lineSpan.left, item.top, lineSpan.right, item.bottom}, style,
                        separator_->textAlign == DuiSeparatorTextAlign::Left ? render::DuiTextAlignment::Start
                            : separator_->textAlign == DuiSeparatorTextAlign::Right ? render::DuiTextAlignment::End
                                                                                   : render::DuiTextAlignment::Center,
                        false);
        return;
    }

    int textLeft{};
    switch (separator_->textAlign) {
    case DuiSeparatorTextAlign::Left: textLeft = lineSpan.left; break;
    case DuiSeparatorTextAlign::Right: textLeft = lineSpan.right - textWidth; break;
    case DuiSeparatorTextAlign::Center: textLeft = lineSpan.left + (lineSpan.Width() - textWidth) / 2; break;
    }
    const int textRight = textLeft + textWidth;
    // 左侧线段只在文本靠右、居中时需要
    if (separator_->textAlign != DuiSeparatorTextAlign::Left)
        paintSegment({lineSpan.left, lineSpan.top, textLeft - separator_->textGap, lineSpan.bottom});
    // 右侧线段只在文本靠左、居中时需要
    if (separator_->textAlign != DuiSeparatorTextAlign::Right)
        paintSegment({textRight + separator_->textGap, lineSpan.top, lineSpan.right, lineSpan.bottom});
    // 文本矩形取整条高度，使其与线条在竖直方向对齐
    canvas.DrawText(separator_->text, {textLeft, item.top, textRight, item.bottom}, style,
                    render::DuiTextAlignment::Center, false);
}

} // namespace ysDui::controls::basic
