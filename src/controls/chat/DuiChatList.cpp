/**
 * 文件名：DuiChatList.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：变高虚拟列表的惰性测量、窗口布局、滚动与贴底跟随实现。
 */
#include "ysDui/controls/chat/DuiChatList.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

#include "ysDui/controls/input/DuiScrollBar.hpp"
#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::chat {
namespace {
/** 滚动条占用宽度。 */
constexpr int kScrollBarWidth = 17;
/** 未测量条目的默认高度估值。 */
constexpr int kDefaultEstimate = 72;
/** 默认预布局余量。 */
constexpr int kDefaultOverscan = 240;
/** 一次布局内允许的测量轮次：测量会改变偏移，需迭代至稳定。 */
constexpr int kMaxLayoutPasses = 4;
/** 一次滚轮事件滚动的行数。 */
constexpr int kWheelLines = 3;
/** 一倍行高的下限，避免估值过小导致滚动过慢。 */
constexpr int kMinScrollLineSize = 16;
} // namespace

class DuiChatList::Impl {
public:
    /** 条目：控件非拥有指针（所有权在基类的 Children 里）。 */
    struct Item
    {
        core::Control* control{};
        int height{kDefaultEstimate};
        int offset{};
        bool measured{};
        bool dirty{true};
    };

    input::DuiScrollBar* scrollBar{};
    mutable std::vector<Item> items;
    mutable bool offsetsDirty{true};
    mutable int contentHeight{};
    int scrollOffset{};
    int estimate{kDefaultEstimate};
    int overscan{kDefaultOverscan};
    int itemSpacing{};
    int scrollLineSize{};
    bool scrollLineOverride{};
    core::Color background{};
    bool backgroundOverride{};
    bool followBottom{};
    /** 未读计数：仅在"贴底跟随关闭时追加条目"处自增，是否归零由 UnseenCount() 派生决定。 */
    int unseen{};
    /** 上次已通过回调上报的未读值，用于只在变化时通知。 */
    int reportedUnseen{};
    std::function<void(int)> unseenChanged;
    core::Rect viewport{};
    int lastBodyWidth{-1};
    mutable int windowFirst{};
    mutable int windowLast{};
};

DuiChatList::DuiChatList() : list_(std::make_unique<Impl>())
{
    auto scrollBar = std::make_unique<input::DuiScrollBar>();
    list_->scrollBar = scrollBar.get();
    scrollBar->SetLineSize(kDefaultEstimate);
    // 用户拖动/滚动条自身变化：同步滚动位置与贴底状态
    scrollBar->SetValueChangedHandler([this](int position)
    {
        list_->scrollOffset = position;
        list_->followBottom = position >= MaxOffset();
        Layout(Bounds());
        // 事件分发期（非布局期）通知未读变化安全：页面可在回调里改浮层几何
        SyncUnseen();
    });
    AddChild(std::move(scrollBar));
}
DuiChatList::~DuiChatList() = default;

// ---- 条目 ----

void DuiChatList::SetItemCount(int count)
{
    count = (std::max)(0, count);
    while (static_cast<int>(list_->items.size()) > count)
        RemoveItem(static_cast<int>(list_->items.size()) - 1);
    while (static_cast<int>(list_->items.size()) < count)
        InsertItem(static_cast<int>(list_->items.size()), nullptr);
}
int DuiChatList::ItemCount() const { return static_cast<int>(list_->items.size()); }

void DuiChatList::SetItem(int index, std::unique_ptr<core::Control> item)
{
    if (index < 0 || index >= ItemCount())
        return;
    Impl::Item& slot = list_->items[static_cast<std::size_t>(index)];
    if (slot.control != nullptr)
        (void)RemoveChild(slot.control);
    slot.control = item.get();
    if (item != nullptr)
        AddChild(std::move(item));
    slot.measured = false;
    slot.dirty = true;
    slot.height = list_->estimate;
    list_->offsetsDirty = true;
    InvalidateLayout();
}
core::Control* DuiChatList::Item(int index) const
{
    if (index < 0 || index >= ItemCount())
        return nullptr;
    return list_->items[static_cast<std::size_t>(index)].control;
}

int DuiChatList::AppendItem(std::unique_ptr<core::Control> item)
{
    const int index = ItemCount();
    InsertAt(index, std::move(item), true);
    return index;
}

void DuiChatList::InsertItem(int index, std::unique_ptr<core::Control> item)
{
    InsertAt(index, std::move(item), false);
}

void DuiChatList::InsertAt(int index, std::unique_ptr<core::Control> item, bool countsAsUnread)
{
    index = (std::clamp)(index, 0, ItemCount());
    Impl::Item slot;
    slot.control = item.get();
    slot.height = list_->estimate;
    if (item != nullptr)
        AddChild(std::move(item));
    list_->items.insert(list_->items.begin() + index, slot);
    if (countsAsUnread)
    {
        // 贴底跟随关闭说明用户没在看末尾，此时到达的消息算未读
        if (!list_->followBottom)
            ++list_->unseen;
    }
    else
    {
        // 结构性插入（载入历史、恢复会话）：视为重新装配，未读清零
        list_->unseen = 0;
    }
    list_->offsetsDirty = true;
    InvalidateLayout();
    SyncUnseen();
}

void DuiChatList::RemoveItem(int index)
{
    if (index < 0 || index >= ItemCount())
        return;
    Impl::Item slot = list_->items[static_cast<std::size_t>(index)];
    if (slot.control != nullptr)
        (void)RemoveChild(slot.control);
    list_->items.erase(list_->items.begin() + index);
    list_->offsetsDirty = true;
    InvalidateLayout();
    SyncUnseen();
}

void DuiChatList::ClearItems()
{
    if (list_->items.empty())
        return;
    for (const Impl::Item& slot : list_->items)
    {
        if (slot.control != nullptr)
            (void)RemoveChild(slot.control);
    }
    list_->items.clear();
    list_->scrollOffset = 0;
    list_->windowFirst = 0;
    list_->windowLast = 0;
    list_->unseen = 0;
    list_->offsetsDirty = true;
    InvalidateLayout();
    SyncUnseen();
}

// ---- 高度失效 ----

void DuiChatList::InvalidateItem(int index)
{
    if (index < 0 || index >= ItemCount())
        return;
    Impl::Item& slot = list_->items[static_cast<std::size_t>(index)];
    slot.dirty = true;
    InvalidateLayout();
}

void DuiChatList::InvalidateAllItems()
{
    for (Impl::Item& slot : list_->items)
        slot.dirty = true;
    list_->offsetsDirty = true;
    InvalidateLayout();
}

// ---- 偏移与窗口（内部） ----

void DuiChatList::RebuildOffsets() const
{
    if (!list_->offsetsDirty)
        return;
    int y = 0;
    for (Impl::Item& slot : list_->items)
    {
        slot.offset = y;
        const int height = slot.measured ? slot.height : list_->estimate;
        slot.height = (std::max)(1, height);
        y += slot.height + list_->itemSpacing;
    }
    list_->contentHeight = list_->items.empty() ? 0 : (std::max)(0, y - list_->itemSpacing);
    list_->offsetsDirty = false;
}

int DuiChatList::IndexFromContentY(int contentY) const
{
    if (list_->items.empty() || contentY < 0)
        return -1;
    const auto found = std::upper_bound(
        list_->items.begin(), list_->items.end(), contentY,
        [](int value, const Impl::Item& item) { return value < item.offset; });
    if (found == list_->items.begin())
        return -1;
    const int index = static_cast<int>(std::distance(list_->items.begin(), found)) - 1;
    const Impl::Item& slot = list_->items[static_cast<std::size_t>(index)];
    return contentY < slot.offset + slot.height ? index : -1;
}

void DuiChatList::ComputeWindow() const
{
    RebuildOffsets();
    const int height = list_->viewport.Height();
    if (list_->items.empty() || height <= 0)
    {
        list_->windowFirst = 0;
        list_->windowLast = 0;
        return;
    }
    const int top = (std::max)(0, list_->scrollOffset - list_->overscan);
    const int bottom = list_->scrollOffset + height + list_->overscan;
    int first = IndexFromContentY(top);
    if (first < 0)
        first = 0;
    int last = IndexFromContentY(bottom);
    if (last < 0)
        last = ItemCount() - 1;
    list_->windowFirst = first;
    // 半开区间；末尾条目也强制纳入，避免最后一个条目落在空白间隙里被排除
    list_->windowLast = (std::min)(ItemCount(), last + 2);
}

bool DuiChatList::MeasureWindow()
{
    ComputeWindow();
    bool measured{};
    for (int index = list_->windowFirst; index < list_->windowLast; ++index)
    {
        Impl::Item& slot = list_->items[static_cast<std::size_t>(index)];
        if (slot.control == nullptr)
        {
            // 空位没有控件可测，始终按估值参与偏移；设置估值时无需特殊处理
            continue;
        }
        if (slot.measured && !slot.dirty)
            continue;
        // 只给宽度测高：正文（Markdown 等）会在此宽度下完成换行
        slot.control->Layout({list_->viewport.left, 0, list_->viewport.left + list_->viewport.Width(), 0});
        slot.height = (std::max)(1, slot.control->DesiredSize().height);
        slot.measured = true;
        slot.dirty = false;
        list_->offsetsDirty = true;
        measured = true;
    }
    if (measured)
        RebuildOffsets();
    return measured;
}

int DuiChatList::MaxOffset() const
{
    RebuildOffsets();
    return (std::max)(0, list_->contentHeight - (std::max)(0, list_->viewport.Height()));
}

void DuiChatList::ClampScroll()
{
    list_->scrollOffset = (std::clamp)(list_->scrollOffset, 0, MaxOffset());
}

// ---- 滚动与贴底 ----

void DuiChatList::SetScrollOffset(int pixels)
{
    list_->scrollOffset = pixels;
    ClampScroll();
    list_->followBottom = list_->scrollOffset >= MaxOffset();
    Layout(Bounds());
    SyncUnseen();
}
int DuiChatList::ScrollOffset() const
{
    RebuildOffsets();
    return (std::clamp)(list_->scrollOffset, 0, (std::max)(0, list_->contentHeight - (std::max)(0, list_->viewport.Height())));
}
void DuiChatList::ScrollToBottom()
{
    list_->followBottom = true;
    Layout(Bounds());
    SyncUnseen();
}
void DuiChatList::SetFollowBottom(bool follow)
{
    if (list_->followBottom == follow)
        return;
    list_->followBottom = follow;
    Layout(Bounds());
    SyncUnseen();
}
bool DuiChatList::FollowBottom() const { return list_->followBottom; }
bool DuiChatList::AtBottom() const { return ScrollOffset() >= MaxOffset(); }

void DuiChatList::EnsureItemVisible(int index)
{
    if (index < 0 || index >= ItemCount() || list_->viewport.Height() <= 0)
        return;
    RebuildOffsets();
    const Impl::Item& slot = list_->items[static_cast<std::size_t>(index)];
    int offset = list_->scrollOffset;
    if (slot.offset < offset)
        offset = slot.offset;
    else if (slot.offset + slot.height > offset + list_->viewport.Height())
        offset = slot.offset + slot.height - list_->viewport.Height();
    SetScrollOffset(offset);
}

// ---- 观测 ----

int DuiChatList::ContentHeight() const
{
    RebuildOffsets();
    return list_->contentHeight;
}

core::Rect DuiChatList::ItemContentRect(int index) const
{
    if (index < 0 || index >= ItemCount())
        return {};
    RebuildOffsets();
    const Impl::Item& slot = list_->items[static_cast<std::size_t>(index)];
    return {0, slot.offset, (std::max)(0, list_->viewport.Width()), slot.offset + slot.height};
}

core::Rect DuiChatList::ItemRect(int index) const
{
    if (index < 0 || index >= ItemCount())
        return {};
    RebuildOffsets();
    const Impl::Item& slot = list_->items[static_cast<std::size_t>(index)];
    const int top = list_->viewport.top + slot.offset - list_->scrollOffset;
    return {list_->viewport.left, top, list_->viewport.right, top + slot.height};
}

int DuiChatList::FirstVisibleIndex() const
{
    if (ItemCount() == 0)
        return -1;
    // 窗口是上一次布局的产物：条目被移除后可能越界，此处夹取到合法下标
    const int first = (std::min)(list_->windowFirst, ItemCount() - 1);
    return first < list_->windowLast ? first : -1;
}
int DuiChatList::LastVisibleIndex() const
{
    if (ItemCount() == 0)
        return -1;
    const int last = (std::min)(list_->windowLast, ItemCount());
    return list_->windowFirst < last ? last - 1 : -1;
}

int DuiChatList::IndexFromPoint(core::Point point) const
{
    if (!list_->viewport.Contains(point))
        return -1;
    const int contentY = point.y - list_->viewport.top + list_->scrollOffset;
    return IndexFromContentY(contentY);
}

int DuiChatList::MeasuredItemCount() const
{
    int count{};
    for (const Impl::Item& slot : list_->items)
        count += slot.measured ? 1 : 0;
    return count;
}

// ---- 未读条目 ----

int DuiChatList::UnseenCount() const
{
    // 派生规则：已到末尾（含内容不足一屏）意味着全部可见，未读恒为 0。
    // 这样"手动滚到底 / ScrollToBottom / 窗口变大到内容全可见"都无需改动状态即可正确归零。
    if (AtBottom())
        return 0;
    return (std::clamp)(list_->unseen, 0, ItemCount());
}

void DuiChatList::ClearUnseen()
{
    list_->unseen = 0;
    SyncUnseen();
}

void DuiChatList::SetUnseenChangedHandler(std::function<void(int)> handler)
{
    list_->unseenChanged = std::move(handler);
}

void DuiChatList::SyncUnseen()
{
    // 已到末尾即视为已读：必须把计数**真正落零**，而不是只靠 UnseenCount() 派生。
    // 否则用户"跳到底再上滚"时，早已读过的旧计数会重新出现。
    if (AtBottom())
        list_->unseen = 0;
    const int current = UnseenCount();
    if (current == list_->reportedUnseen)
        return;
    list_->reportedUnseen = current;
    if (list_->unseenChanged)
        list_->unseenChanged(current);
}

// ---- 外观与参数 ----

void DuiChatList::SetEstimatedItemHeight(int pixels)
{
    list_->estimate = (std::max)(1, pixels);
    // 估值是固定常量，未测量（含空位）条目直接采用新值，无需逐个改写高度
    list_->offsetsDirty = true;
    InvalidateLayout();
}
int DuiChatList::EstimatedItemHeight() const { return list_->estimate; }

void DuiChatList::SetOverscan(int pixels)
{
    list_->overscan = (std::max)(0, pixels);
    InvalidateLayout();
}
int DuiChatList::Overscan() const { return list_->overscan; }

void DuiChatList::SetItemSpacing(int pixels)
{
    list_->itemSpacing = (std::max)(0, pixels);
    list_->offsetsDirty = true;
    InvalidateLayout();
}
int DuiChatList::ItemSpacing() const { return list_->itemSpacing; }

void DuiChatList::SetBackgroundColor(core::Color color)
{
    list_->background = color;
    list_->backgroundOverride = true;
}
core::Color DuiChatList::BackgroundColor() const
{
    return list_->backgroundOverride ? list_->background : Theme().Get(core::ThemeSlot::ListBackground);
}

void DuiChatList::SetScrollLineSize(int pixels)
{
    list_->scrollLineSize = (std::max)(1, pixels);
    list_->scrollLineOverride = true;
    list_->scrollBar->SetLineSize(list_->scrollLineSize);
}
int DuiChatList::ScrollLineSize() const
{
    return list_->scrollLineOverride ? list_->scrollLineSize : (std::max)(kMinScrollLineSize, list_->estimate);
}

// ---- 布局 ----

void DuiChatList::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    // 滚动条宽度会改变正文宽度，进而改变高度：先假设溢出，不溢出时再按完整宽度重排一次
    const int bodyRight = (std::max)(bounds.left, bounds.right - kScrollBarWidth);
    list_->viewport = {bounds.left, bounds.top, bodyRight, bounds.bottom};
    RunLayoutPasses();
    if (list_->contentHeight <= bounds.Height())
    {
        list_->viewport = {bounds.left, bounds.top, bounds.right, bounds.bottom};
        RunLayoutPasses();
    }
    PlaceWindow();
    UpdateScrollBar();
    // 布局期改动条目几何会被 InvalidateLayout 吞掉（祖先处于 inLayout），
    // 故此处只同步"未读计数"这一纯状态：回调实现应只改文本/显隐，
    // 需要随尺寸变化的浮层几何请由页面在自己的 Layout() 里读取 UnseenCount() 决定。
    SyncUnseen();
}

void DuiChatList::RunLayoutPasses()
{
    const int bodyWidth = list_->viewport.Width();
    if (bodyWidth != list_->lastBodyWidth)
    {
        // 宽度变化会改变换行结果，全部高度失效
        list_->lastBodyWidth = bodyWidth;
        for (Impl::Item& slot : list_->items)
            slot.dirty = true;
        list_->offsetsDirty = true;
    }
    // 不变量：测量只会改动窗口内条目的高度，窗口之上条目的偏移在本帧内不变，
    // 因此滚动位置不需要做锚定补偿——用户正在看的顶部条目不会被测量挤动。
    for (int pass = 0; pass < kMaxLayoutPasses; ++pass)
    {
        RebuildOffsets();
        if (list_->followBottom)
            list_->scrollOffset = MaxOffset();
        ClampScroll();
        if (!MeasureWindow())
            break;
    }
    // 末轮测量可能改变了高度，按最终偏移重新定位窗口
    RebuildOffsets();
    if (list_->followBottom)
        list_->scrollOffset = MaxOffset();
    ClampScroll();
    ComputeWindow();
}

void DuiChatList::PlaceWindow()
{
    for (int index = list_->windowFirst; index < list_->windowLast; ++index)
    {
        const Impl::Item& slot = list_->items[static_cast<std::size_t>(index)];
        if (slot.control == nullptr)
            continue;
        const int top = list_->viewport.top + slot.offset - list_->scrollOffset;
        slot.control->Layout({list_->viewport.left, top, list_->viewport.right, top + slot.height});
    }
}

void DuiChatList::UpdateScrollBar()
{
    const int maxOffset = MaxOffset();
    input::DuiScrollBar* bar = list_->scrollBar;
    bar->SetPageSize((std::max)(1, list_->viewport.Height()));
    bar->SetRange(0, maxOffset);
    bar->SetVisible(maxOffset > 0);
    if (bar->Visible())
        bar->SetBounds({Bounds().right - kScrollBarWidth, Bounds().top, Bounds().right, Bounds().bottom});
    bar->SetPosition(list_->scrollOffset, false);
}

// ---- 交互 ----

core::Control* DuiChatList::HitTest(core::Point point)
{
    if (!EffectivelyVisible() || !Enabled() || !Bounds().Contains(point))
        return nullptr;
    // 优先命中滚动条，避免被条目抢先
    if (list_->scrollBar->Visible() && list_->scrollBar->HitTest(point) != nullptr)
        return list_->scrollBar;
    if (!list_->viewport.Contains(point))
        return this;
    const int index = IndexFromPoint(point);
    // 只有窗口内的条目有有效几何；窗口外的候选一律由本控件接管
    if (index >= list_->windowFirst && index < list_->windowLast)
    {
        if (core::Control* control = list_->items[static_cast<std::size_t>(index)].control)
        {
            if (control->HitTest(point) != nullptr)
                return control;
        }
    }
    return this;
}

bool DuiChatList::OnEvent(const core::Event& event)
{
    if (!Enabled() || !EffectivelyVisible())
        return false;
    const int line = ScrollLineSize();
    if (event.type == core::EventType::PointerWheel && Bounds().Contains(event.position))
    {
        if (event.wheelDelta == 0)
            return false;
        SetScrollOffset(ScrollOffset() + (event.wheelDelta > 0 ? -1 : 1) * line * kWheelLines);
        return true;
    }
    if (event.type != core::EventType::KeyDown)
        return false;
    const int page = (std::max)(line, list_->viewport.Height());
    int target = ScrollOffset();
    switch (event.key)
    {
    case core::key::Up: target -= line; break;
    case core::key::Down: target += line; break;
    case core::key::PageUp: target -= page; break;
    case core::key::PageDown: target += page; break;
    case core::key::Home: target = 0; break;
    case core::key::End: target = MaxOffset(); break;
    default: return false;
    }
    SetScrollOffset(target);
    return true;
}

void DuiChatList::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    if (!EffectivelyVisible())
        return;
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (bounds.Empty())
        return;
    RebuildOffsets();
    canvas.FillRect(bounds, BackgroundColor());
    canvas.PushClip(list_->viewport);
    // 窗口是上一次布局的产物：条目可能已被移除，循环上界必须夹取
    const int windowEnd = (std::min)(list_->windowLast, ItemCount());
    for (int index = (std::max)(0, list_->windowFirst); index < windowEnd; ++index)
    {
        const Impl::Item& slot = list_->items[static_cast<std::size_t>(index)];
        if (slot.control == nullptr)
            continue;
        const core::Rect rect = ItemRect(index);
        if (core::Rect::Intersect(rect, dirty).Empty())
            continue;
        if (const auto* renderable = dynamic_cast<const render::DuiRenderable*>(slot.control))
            renderable->Paint(canvas, dirty);
    }
    canvas.PopClip();
    if (list_->scrollBar->Visible())
        list_->scrollBar->Paint(canvas, dirty);
    // 列表底色与页面底色同为浅色：靠外框区分控件范围（与 DuiVirtualList 一致）
    canvas.StrokeRoundedRect(Bounds(), 0, Theme().Get(core::ThemeSlot::GridBorder), 1.0F);
}

core::DuiAccessibilityData DuiChatList::CreateAccessibilityData() const
{
    core::DuiAccessibilityData data;
    data.role = core::DuiAccessibilityRole::List;
    data.name = "Chat";
    data.keyboardFocusable = true;
    return data;
}

} // namespace ysDui::controls::chat
