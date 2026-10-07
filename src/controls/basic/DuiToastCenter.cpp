#include "ysDui/controls/basic/DuiToastCenter.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiPaintChildren.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::basic {
namespace {
constexpr int DefaultEdgeOffset = 16;
constexpr int DefaultGap = 8;
constexpr int DefaultMaxWidth = 480;
constexpr int BadgeHeight = 16;   // 重复计数徽标高度
constexpr int BadgeMinWidth = 16; // 徽标最小宽度（个位数）
constexpr int BadgePadding = 5;   // 徽标左右内边距
constexpr int BadgeCharWidth = 7; // 单字符宽度估算
} // namespace

class DuiToastCenter::Impl {
public:
    /**
     * 堆叠条目。
     * toast 为非拥有指针：DuiToast 的所有权在 Control 的子节点表里（AddChild 之后），
     * 本表只记录顺序与元数据。
     */
    struct Entry final
    {
        DuiToast* toast{};
        int repeatNum{1};
        bool retired{}; // 已结束，等待安全时机移除
    };

    core::AnimationClock* clock{};
    render::DuiTextMeasurer* textMeasurer{};
    DuiToastPlacement placement{DuiToastPlacement::Top};
    int edgeOffset{DefaultEdgeOffset};
    int gap{DefaultGap};
    int maxWidth{DefaultMaxWidth};
    bool grouping{};
    std::vector<Entry> entries;

    /** @return 条目下标；未登记时返回 -1。 */
    [[nodiscard]] int IndexOf(const DuiToast* toast) const
    {
        for (int index = 0; index < static_cast<int>(entries.size()); ++index)
        {
            if (entries[static_cast<std::size_t>(index)].toast == toast)
                return index;
        }
        return -1;
    }
};

DuiToastCenter::DuiToastCenter() : center_(std::make_unique<Impl>()) {}
DuiToastCenter::~DuiToastCenter() = default;

void DuiToastCenter::SetAnimationClock(core::AnimationClock* clock)
{
    center_->clock = clock;
    for (const auto& entry : center_->entries)
        entry.toast->SetAnimationClock(clock);
}
void DuiToastCenter::SetPlacement(DuiToastPlacement placement) { center_->placement = placement; }
DuiToastPlacement DuiToastCenter::Placement() const { return center_->placement; }
void DuiToastCenter::SetEdgeOffset(int pixels) { center_->edgeOffset = (std::max)(0, pixels); }
int DuiToastCenter::EdgeOffset() const { return center_->edgeOffset; }
void DuiToastCenter::SetGap(int pixels) { center_->gap = (std::max)(0, pixels); }
int DuiToastCenter::Gap() const { return center_->gap; }
void DuiToastCenter::SetMaxWidth(int pixels) { center_->maxWidth = (std::max)(0, pixels); }
int DuiToastCenter::MaxWidth() const { return center_->maxWidth; }
void DuiToastCenter::SetTextMeasurer(render::DuiTextMeasurer* measurer) { center_->textMeasurer = measurer; }
void DuiToastCenter::SetGrouping(bool enabled) { center_->grouping = enabled; }
bool DuiToastCenter::Grouping() const { return center_->grouping; }

void DuiToastCenter::retire(const DuiToast* toast)
{
    const int index = center_->IndexOf(toast);
    if (index < 0)
        return;
    // 此刻可能正处在动画推进的调用栈内，只做标记，实际移除交给 Purge
    center_->entries[static_cast<std::size_t>(index)].retired = true;
}

void DuiToastCenter::Purge()
{
    // 仅移除已标记结束的条目；控件销毁会取消其动画，故不会再有回调落到已释放对象
    for (int index = static_cast<int>(center_->entries.size()) - 1; index >= 0; --index)
    {
        if (!center_->entries[static_cast<std::size_t>(index)].retired)
            continue;
        DuiToast* toast = center_->entries[static_cast<std::size_t>(index)].toast;
        center_->entries.erase(center_->entries.begin() + index);
        toast->SetClosedHandler({});
        (void)RemoveChild(toast); // 返回的 unique_ptr 在此析构，完成销毁
    }
}

bool DuiToastCenter::Show(std::string message, DuiToastOptions options)
{
    if (message.empty())
        return false;
    Purge();

    // 分组：相同文字合并到已有条目并累加计数（对应 grouping / repeatNum）
    if (center_->grouping)
    {
        for (auto& entry : center_->entries)
        {
            if (entry.toast->Text() != message)
                continue;
            ++entry.repeatNum;
            // 重新计时并刷新外观，使合并后的消息继续停留在屏幕上
            entry.toast->SetType(options.type);
            entry.toast->SetAppearance(options.appearance);
            entry.toast->SetIcon(options.icon);
            if (options.background.has_value())
                entry.toast->SetBackgroundColor(*options.background);
            entry.toast->SetDurationMilliseconds(options.durationMilliseconds);
            entry.toast->Show(message);
            if (!Bounds().Empty())
                Layout(Bounds());
            return false;
        }
    }

    auto toast = std::make_unique<DuiToast>();
    DuiToast* raw = toast.get();
    toast->SetAnimationClock(center_->clock);
    toast->SetTextMeasurer(center_->textMeasurer);
    toast->SetPlacement(center_->placement);
    toast->SetEdgeOffset(center_->edgeOffset);
    toast->SetMaxWidth(center_->maxWidth);
    toast->SetType(options.type);
    toast->SetAppearance(options.appearance);
    toast->SetIcon(options.icon);
    if (options.background.has_value())
        toast->SetBackgroundColor(*options.background);
    toast->SetDurationMilliseconds(options.durationMilliseconds);
    toast->SetShowClose(options.showClose);
    // 结束只做标记，不在回调栈内销毁
    toast->SetClosedHandler([this, raw] { retire(raw); });
    AddChild(std::move(toast));
    center_->entries.push_back({raw, 1, false});
    raw->Show(std::move(message));
    if (!Bounds().Empty())
        Layout(Bounds());
    return true;
}

void DuiToastCenter::Clear()
{
    // 主动清空：先摘掉回调再隐藏，最后整体释放，避免经过 closed 通知路径
    for (auto& entry : center_->entries)
    {
        entry.toast->SetClosedHandler({});
        entry.toast->HideNow();
    }
    center_->entries.clear();
    while (!Children().empty())
        (void)RemoveChild(Children().front().get());
}

int DuiToastCenter::Count() const { return static_cast<int>(center_->entries.size()); }

const std::string& DuiToastCenter::MessageAt(int index) const
{
    static const std::string empty;
    if (index < 0 || index >= static_cast<int>(center_->entries.size()))
        return empty;
    return center_->entries[static_cast<std::size_t>(index)].toast->Text();
}

int DuiToastCenter::RepeatNumAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(center_->entries.size()))
        return 0;
    return center_->entries[static_cast<std::size_t>(index)].repeatNum;
}

core::Rect DuiToastCenter::BadgeRectAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(center_->entries.size()))
        return {};
    const Impl::Entry& entry = center_->entries[static_cast<std::size_t>(index)];
    if (entry.repeatNum <= 1 || !entry.toast->Active())
        return {};
    const int digits = static_cast<int>(std::to_string(entry.repeatNum).size());
    const int width = (std::max)(BadgeMinWidth, digits * BadgeCharWidth + BadgePadding * 2);
    const core::Rect toastBounds = entry.toast->Bounds();
    const int top = toastBounds.top - BadgeHeight / 2;
    return {toastBounds.right - width / 2, top, toastBounds.right + width / 2, top + BadgeHeight};
}

void DuiToastCenter::Layout(core::Rect availableBounds)
{
    SetBounds(availableBounds);
    Purge();
    int accumulated{};
    for (const auto& entry : center_->entries)
    {
        // 用收缩后的可用区域表达堆叠位置，复用 DuiToast 自身的水平居中与边缘偏移逻辑
        core::Rect slot = availableBounds;
        if (center_->placement == DuiToastPlacement::Bottom)
            slot.bottom -= accumulated;
        else
            slot.top += accumulated;
        entry.toast->Layout(slot);
        accumulated += entry.toast->Bounds().Height() + center_->gap;
    }
}

core::Size DuiToastCenter::DesiredSize() const { return {}; }

core::Control* DuiToastCenter::HitTest(core::Point point)
{
    // 只认领消息上的关闭按钮；中心自身区域不参与命中，避免遮挡下层控件
    for (const auto& child : Children())
    {
        if (core::Control* hit = child->HitTest(point))
            return hit;
    }
    return nullptr;
}

void DuiToastCenter::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    render::PaintChildren(*this, canvas, dirty);

    // 重复计数徽标画在消息之上（对应 repeatNum）
    const core::DuiTheme& theme = Theme();
    const render::DuiTextStyle badgeStyle{theme.Get(core::ThemeSlot::TextOnPrimary), {}, 8, true};
    for (int index = 0; index < static_cast<int>(center_->entries.size()); ++index)
    {
        const core::Rect badge = BadgeRectAt(index);
        if (badge.Empty() || core::Rect::Intersect(badge, dirty).Empty())
            continue;
        canvas.FillRoundedRect(badge, badge.Height() / 2, theme.Get(core::ThemeSlot::BadgeBackground));
        canvas.DrawText(std::to_string(center_->entries[static_cast<std::size_t>(index)].repeatNum),
                        badge, badgeStyle, render::DuiTextAlignment::Center, false);
    }
}

} // namespace ysDui::controls::basic
