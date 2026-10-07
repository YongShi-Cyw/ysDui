#include "ysDui/controls/input/DuiRatingControl.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::input {
namespace {
constexpr int DefaultStarSize = 18;
constexpr int DefaultStarGap = 4;
constexpr double Pi = 3.14159265358979323846;
constexpr int StarPoints = 5;             // 五角星
constexpr double InnerRadiusRatio = 0.382; // 凹点半径与凸点半径之比（正五角星比例）

/**
 * 构造一颗五角星的路径。
 * @param bounds 外接正方形区域。
 * @return 与 bounds 内切、顶角朝上的五角星闭合路径。
 */
render::DuiPath StarPath(core::Rect bounds)
{
    const double centerX = (bounds.left + bounds.right) / 2.0;
    const double centerY = (bounds.top + bounds.bottom) / 2.0;
    const double outerRadius = (std::min)(bounds.Width(), bounds.Height()) / 2.0;
    const double innerRadius = outerRadius * InnerRadiusRatio;
    render::DuiPath path;
    for (int index = 0; index < StarPoints * 2; ++index)
    {
        // 从正上方（-90 度）起，交替取凸点与凹点
        const double angle = -Pi / 2.0 + Pi * index / StarPoints;
        const double radius = (index % 2 == 0) ? outerRadius : innerRadius;
        const core::Point point{
            static_cast<int>(std::lround(centerX + radius * std::cos(angle))),
            static_cast<int>(std::lround(centerY + radius * std::sin(angle)))};
        if (index == 0)
            path.MoveTo(point);
        else
            path.LineTo(point);
    }
    path.Close();
    return path;
}
} // namespace

class DuiRatingControl::Impl {
public:
    int maxRating{5};
    int value{};
    int hovered{-1}; // 悬停预览星数；-1 表示无预览
    int starSize{DefaultStarSize};
    int starGap{DefaultStarGap};
    bool readOnly{};
    std::function<void(int)> changed;
};

DuiRatingControl::DuiRatingControl() : rating_(std::make_unique<Impl>())
{
    SetPointerCursor(core::DuiPointerCursor::Hand);
}
DuiRatingControl::~DuiRatingControl() = default;
DuiRatingControl::DuiRatingControl(DuiRatingControl&&) noexcept = default;
DuiRatingControl& DuiRatingControl::operator=(DuiRatingControl&&) noexcept = default;

void DuiRatingControl::SetMaxRating(int value)
{
    rating_->maxRating = (std::max)(1, value);
    if (rating_->value > rating_->maxRating)
        rating_->value = rating_->maxRating;
    if (rating_->hovered > rating_->maxRating)
        rating_->hovered = -1;
}
int DuiRatingControl::MaxRating() const { return rating_->maxRating; }

void DuiRatingControl::SetValue(int value, bool notify)
{
    const int next = (std::clamp)(value, 0, rating_->maxRating);
    if (next == rating_->value)
        return;
    rating_->value = next;
    if (notify && rating_->changed)
        rating_->changed(next);
}
int DuiRatingControl::Value() const { return rating_->value; }

void DuiRatingControl::SetReadOnly(bool readOnly) { rating_->readOnly = readOnly; }
bool DuiRatingControl::ReadOnly() const { return rating_->readOnly; }
void DuiRatingControl::SetStarSize(int pixels) { rating_->starSize = (std::max)(1, pixels); }
int DuiRatingControl::StarSize() const { return rating_->starSize; }
void DuiRatingControl::SetStarGap(int pixels) { rating_->starGap = (std::max)(0, pixels); }
int DuiRatingControl::StarGap() const { return rating_->starGap; }
void DuiRatingControl::SetValueChangedHandler(std::function<void(int)> handler) { rating_->changed = std::move(handler); }

core::Rect DuiRatingControl::StarRect(int index) const
{
    if (index < 0 || index >= rating_->maxRating)
        return {};
    const core::Rect bounds = Bounds();
    const int left = bounds.left + index * (rating_->starSize + rating_->starGap);
    return {left, bounds.top, left + rating_->starSize, bounds.top + rating_->starSize};
}

int DuiRatingControl::StarFromPoint(core::Point point) const
{
    if (!Bounds().Contains(point))
        return -1;
    for (int index = 0; index < rating_->maxRating; ++index)
    {
        if (StarRect(index).Contains(point))
            return index;
    }
    return -1;
}

int DuiRatingControl::DisplayedValue() const
{
    return rating_->hovered >= 0 ? rating_->hovered : rating_->value;
}

core::Size DuiRatingControl::DesiredSize() const
{
    const int width = rating_->maxRating > 0
        ? rating_->maxRating * rating_->starSize + (rating_->maxRating - 1) * rating_->starGap
        : 0;
    return {width, rating_->starSize};
}

bool DuiRatingControl::OnEvent(const core::Event& event)
{
    if (!Enabled())
        return false;

    if (event.type == core::EventType::PointerMove)
    {
        const int index = StarFromPoint(event.position);
        rating_->hovered = index >= 0 ? index + 1 : -1;
        SetHovered(index >= 0);
        return rating_->hovered >= 0;
    }
    if (event.type == core::EventType::PointerLeave)
    {
        rating_->hovered = -1;
        SetHovered(false);
        return false;
    }
    if (event.type == core::EventType::PointerDown && !rating_->readOnly)
    {
        const int index = StarFromPoint(event.position);
        if (index < 0)
            return false;
        // 再次点击当前星数视为取消评分，便于清空
        SetValue(rating_->value == index + 1 ? 0 : index + 1, true);
        return true;
    }
    if (event.type == core::EventType::KeyDown && !rating_->readOnly)
    {
        if (event.key == core::key::Left || event.key == core::key::Down)
        {
            SetValue(rating_->value - 1, true);
            return true;
        }
        if (event.key == core::key::Right || event.key == core::key::Up)
        {
            SetValue(rating_->value + 1, true);
            return true;
        }
    }
    return false;
}

void DuiRatingControl::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::DuiTheme& theme = Theme();
    const core::Color filled = theme.Get(core::ThemeSlot::MessageWarningFill);
    const core::Color empty = theme.Get(core::ThemeSlot::BorderHeavy);
    const int displayed = DisplayedValue();

    for (int index = 0; index < rating_->maxRating; ++index)
    {
        const core::Rect bounds = StarRect(index);
        if (bounds.Empty() || core::Rect::Intersect(bounds, dirty).Empty())
            continue;
        const render::DuiPath star = StarPath(bounds);
        if (index < displayed)
        {
            canvas.FillPath(star, filled);
            canvas.StrokePath(star, filled, 1.0F);
        }
        else
        {
            // 未评分的星：空槽底色 + 边框描边，保证在浅色背景上仍可见
            canvas.FillPath(star, theme.Get(core::ThemeSlot::SurfaceBackground));
            canvas.StrokePath(star, empty, 1.0F);
        }
    }
}

core::DuiAccessibilityData DuiRatingControl::CreateAccessibilityData() const
{
    const std::string value = std::to_string(rating_->value) + " / " + std::to_string(rating_->maxRating);
    core::DuiAccessibilityData data{core::DuiAccessibilityRole::Slider, {}, value, {}, true, {}};
    data.patterns = core::DuiAccessibilityPattern::Value;
    data.readOnly = rating_->readOnly;
    return data;
}

bool DuiRatingControl::PerformAccessibilityAction(core::DuiAccessibilityAction action, std::string_view value)
{
    if (rating_->readOnly || action != core::DuiAccessibilityAction::SetValue || value.empty())
        return false;
    int parsed{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
        return false;
    SetValue(parsed, true);
    return true;
}

} // namespace ysDui::controls::input
